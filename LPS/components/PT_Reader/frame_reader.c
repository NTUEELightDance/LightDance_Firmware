#include "frame_reader.h"
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include "esp_log.h"
#include "ld_board.h"  // global ch_info
#include "readframe.h"

/* ================= config ================= */

static const uint8_t EXPECTED_VERSION_MAJOR = 1;
static const uint8_t EXPECTED_VERSION_MINOR = 2;

#define FRAME_RAW_MAX_SIZE 8192
#define CHECKSUM_SIZE 4  // uint8 (reserved)
#define FRAME_FILE_HEADER_SIZE 2U

/* ================= static ================= */

static const char* TAG = "FRAME_READER";

static FILE* fp = NULL;
static bool opened = false;
static uint32_t g_frame_size = 0;

/* ================= helpers ================= */

static inline void checksum_add_u8(uint32_t* sum, uint8_t b) {
    *sum += (uint32_t)b;
}

/* ================= init / deinit ================= */

esp_err_t frame_reader_init(const char* path) {
    if(!path) {
        ESP_LOGE(TAG, "frame reader initialization rejected: path is NULL");
        return ESP_ERR_INVALID_ARG;
    }

    /* -------- sanity: ch_info must be valid -------- */

    uint32_t of_cnt = 0;
    uint32_t led_cnt = 0;

    for(int i = 0; i < LD_BOARD_PCA9955B_CH_NUM; i++)
        if(ch_info_snapshot.i2c_leds[i])
            of_cnt++;

    for(int i = 0; i < LD_BOARD_WS2812B_NUM; i++)
        led_cnt += ch_info_snapshot.rmt_strips[i];

    if(of_cnt == 0 && led_cnt == 0) {
        ESP_LOGE(TAG, "frame reader initialization failed: channel configuration has no enabled LED outputs");
        return ESP_ERR_INVALID_STATE;
    }

    /* -------- open file -------- */

    fp = fopen(path, "rb");
    if(!fp) {
        ESP_LOGE(TAG, "frame file open failed: path=%s errno=%d (%s)", path, errno, strerror(errno));
        return errno == ENOENT ? ESP_ERR_NOT_FOUND : ESP_FAIL;
    }

    /* -------- check version -------- */

    uint8_t version_bytes[2];
    if(fread(version_bytes, 1, sizeof(version_bytes), fp) != sizeof(version_bytes)) {
        ESP_LOGE(TAG, "frame file version header is missing or truncated: path=%s", path);
        fclose(fp);
        fp = NULL;
        return ESP_FAIL;
    }

    uint8_t major = version_bytes[0];
    uint8_t minor = version_bytes[1];

    if(major != EXPECTED_VERSION_MAJOR || minor != EXPECTED_VERSION_MINOR) {
        ESP_LOGE(TAG, "unsupported frame file version: path=%s expected=%u.%u actual=%u.%u", path, EXPECTED_VERSION_MAJOR, EXPECTED_VERSION_MINOR, major, minor);
        fclose(fp);
        fp = NULL;
        return ESP_FAIL;
    }

    ESP_LOGD(TAG, "frame file version accepted: path=%s version=%u.%u", path, major, minor);

    /* -------- calculate frame size  -------- */

    g_frame_size = 4 +             /* start_time */
                   1 +             /* fade */
                   (of_cnt * 3) +  /* OF GRB */
                   (led_cnt * 3) + /* LED GRB */
                   CHECKSUM_SIZE;  /* checksum */

    if(g_frame_size > FRAME_RAW_MAX_SIZE) {
        ESP_LOGE(TAG,
                 "calculated frame size exceeds reader buffer: frame_size=%u maximum=%u pca_pixels=%u ws_pixels=%u",
                 (unsigned)g_frame_size,
                 (unsigned)FRAME_RAW_MAX_SIZE,
                 (unsigned)of_cnt,
                 (unsigned)led_cnt);
        fclose(fp);
        fp = NULL;
        return ESP_ERR_INVALID_SIZE;
    }

    opened = true;

    ESP_LOGI(TAG, "frame file ready: path=%s frame_size=%u bytes pca_pixels=%u ws_pixels=%u", path, (unsigned)g_frame_size, (unsigned)of_cnt, (unsigned)led_cnt);

    return ESP_OK;
}

void frame_reader_deinit(void) {
    if(!opened)
        return;

    fclose(fp);
    fp = NULL;
    opened = false;
    ESP_LOGD(TAG, "frame file closed");
}

esp_err_t frame_reader_reset(void) {
    return frame_reader_seek(0);
}

esp_err_t frame_reader_seek(uint32_t frame_idx) {
    if(!opened) {
        ESP_LOGE(TAG, "seek rejected: frame file is not open");
        return ESP_ERR_INVALID_STATE;
    }

    long offset = (long)FRAME_FILE_HEADER_SIZE + ((long)frame_idx * (long)g_frame_size);
    if(fseek(fp, offset, SEEK_SET) != 0) {
        ESP_LOGE(TAG, "frame seek failed: frame_index=%lu file_offset=%ld errno=%d (%s)", (unsigned long)frame_idx, offset, errno, strerror(errno));
        return ESP_FAIL;
    }

    ESP_LOGD(TAG, "frame seek completed: frame_index=%lu file_offset=%ld", (unsigned long)frame_idx, offset);

    return ESP_OK;
}

uint32_t frame_reader_frame_size(void) {
    return g_frame_size;
}

/* ================= read one frame ================= */

esp_err_t frame_reader_read(table_frame_t* out) {
    // if (memcmp(&ch_info, &ch_info_snapshot, sizeof(ch_info)) != 0) {
    // ESP_LOGE(TAG, "ch_info changed after init");
    // return ESP_FAIL;
    // }
    if(!opened) {
        ESP_LOGE(TAG, "frame read rejected: frame file is not open");
        return ESP_ERR_INVALID_STATE;
    }
    if(!out) {
        ESP_LOGE(TAG, "frame read rejected: output pointer is NULL");
        return ESP_ERR_INVALID_ARG;
    }

    static uint8_t raw[FRAME_RAW_MAX_SIZE];

    memset(out, 0, sizeof(*out));

    size_t br = fread(raw, 1, g_frame_size, fp);

    /* 1) FatFs 回錯：I/O error，不是 EOF */
    if(ferror(fp)) {
        ESP_LOGE(TAG, "frame file read failed: requested=%u bytes received=%u bytes errno=%d (%s)", (unsigned)g_frame_size, (unsigned)br, errno, strerror(errno));
        return ESP_FAIL;  // 或 ESP_ERR_INVALID_STATE / 你自訂 IO err
    }

    /* 2) fr OK 但 br 不足：分成 EOF 與 truncated/corrupt */
    if(br != g_frame_size) {
        if(br == 0) {
            return ESP_ERR_NOT_FOUND;  // 你目前用 NOT_FOUND 當 EOF（沿用）
        }
        ESP_LOGE(TAG, "truncated frame: expected=%u bytes received=%u bytes", (unsigned)g_frame_size, (unsigned)br);
        return ESP_ERR_INVALID_SIZE;  // 檔案截斷/損壞
    }

    uint8_t* p = raw;
    uint32_t sum = 0;

    /* -------- start_time -------- */
    out->timestamp = (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);

    for(int i = 0; i < 4; i++)
        checksum_add_u8(&sum, p[i]);

    p += 4;

    /* -------- fade -------- */
    out->fade = (*p != 0);
    checksum_add_u8(&sum, *p);
    p += 1;

    /* -------- OF GRB (only enabled) -------- */
    for(int ch = 0; ch < LD_BOARD_PCA9955B_CH_NUM; ch++) {
        if(!ch_info_snapshot.i2c_leds[ch])
            continue;

        uint8_t g = p[0];
        uint8_t r = p[1];
        uint8_t b = p[2];

        checksum_add_u8(&sum, g);
        checksum_add_u8(&sum, r);
        checksum_add_u8(&sum, b);

        out->data.pca9955b[ch].g = g;
        out->data.pca9955b[ch].r = r;
        out->data.pca9955b[ch].b = b;

        p += 3;
    }

    /* -------- WS2812B LED strips -------- */
    for(int strip = 0; strip < LD_BOARD_WS2812B_NUM; strip++) {
        uint16_t cnt = ch_info_snapshot.rmt_strips[strip];

        for(uint16_t i = 0; i < cnt; i++) {
            uint8_t g = p[0];
            uint8_t r = p[1];
            uint8_t b = p[2];

            checksum_add_u8(&sum, g);
            checksum_add_u8(&sum, r);
            checksum_add_u8(&sum, b);

            out->data.ws2812b[strip][i].g = g;
            out->data.ws2812b[strip][i].r = r;
            out->data.ws2812b[strip][i].b = b;

            p += 3;
        }
    }

    /* -------- checksum (reserved, consume only) -------- */
    uint32_t read_checksum = 0;
    read_checksum |= (uint32_t)p[0];
    read_checksum |= (uint32_t)p[1] << 8;
    read_checksum |= (uint32_t)p[2] << 16;
    read_checksum |= (uint32_t)p[3] << 24;

    p += CHECKSUM_SIZE;

    if(read_checksum != sum) {
        ESP_LOGE(TAG, "frame checksum mismatch: timestamp_ms=%lu stored=%lu calculated=%lu", (unsigned long)out->timestamp, (unsigned long)read_checksum, (unsigned long)sum);
        return ESP_ERR_INVALID_CRC;
    }

    /* -------- final guard -------- */
    if((uint32_t)(p - raw) != g_frame_size) {
        ESP_LOGE(TAG, "frame parser consumed an unexpected byte count: consumed=%u expected=%u", (unsigned)(p - raw), (unsigned)g_frame_size);
        return ESP_FAIL;
    }

    return ESP_OK;
}
