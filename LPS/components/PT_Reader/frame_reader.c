#include "frame_reader.h"

#include <string.h>
#include "esp_err.h"
#include "esp_log.h"
#include "ff.h"
#include "ld_frame.h"
#include "ld_board.h"  // global ch_info
#include "readframe.h"

/* ================= config ================= */

#define FRAME_FILE_HEADER_SIZE 2  // uint16 version
#define FRAME_RAW_MAX_SIZE 8192
#define CHECKSUM_SIZE 4  // uint8 (reserved)

/* ================= static ================= */

static const char* TAG = "frame_reader";

static FIL fp;
static bool opened = false;
static uint32_t g_frame_size = 0;

/* ================= helpers ================= */

static inline void checksum_add_u8(uint32_t* sum, uint8_t b) {
    *sum += (uint32_t)b;
}

/* ================= init / deinit ================= */

esp_err_t frame_reader_init(const char* path) {
    if(!path)
        return ESP_ERR_INVALID_ARG;

    /* -------- sanity: ch_info must be valid -------- */

    uint32_t of_cnt = 0;
    uint32_t led_cnt = 0;

    for(int i = 0; i < PCA9955B_CH_NUM; i++)
        if(ch_info_snapshot.i2c_leds[i])
            of_cnt++;

    for(int i = 0; i < WS2812B_NUM; i++)
        led_cnt += ch_info_snapshot.rmt_strips[i];

    if(of_cnt == 0 && led_cnt == 0) {
        ESP_LOGE(TAG, "ch_info empty (no OF, no LED)");
        return ESP_ERR_INVALID_STATE;
    }

    /* -------- open file -------- */

    FRESULT fr = f_open(&fp, path, FA_READ);
    if(fr != FR_OK) {
        ESP_LOGE(TAG, "open %s failed (fr=%d)", path, fr);
        return ESP_ERR_NOT_FOUND;
    }

    /* skip frame.dat header */
    if(f_lseek(&fp, FRAME_FILE_HEADER_SIZE) != FR_OK) {
        f_close(&fp);
        return ESP_FAIL;
    }

    g_frame_size = 4 +             /* start_time */
                   1 +             /* fade */
                   (of_cnt * 3) +  /* OF GRB */
                   (led_cnt * 3) + /* LED GRB */
                   CHECKSUM_SIZE;  /* checksum */

    if(g_frame_size > FRAME_RAW_MAX_SIZE) {
        ESP_LOGE(TAG, "frame_size %u exceeds max", (unsigned)g_frame_size);
        f_close(&fp);
        return ESP_ERR_INVALID_SIZE;
    }

    opened = true;

    ESP_LOGI(TAG, "frame_reader init: frame_size=%u (OF=%u LED=%u)", (unsigned)g_frame_size, (unsigned)of_cnt, (unsigned)led_cnt);

    return ESP_OK;
}

void frame_reader_deinit(void) {
    if(!opened)
        return;

    f_close(&fp);
    opened = false;
}

esp_err_t frame_reader_reset(void) {
    if(!opened)
        return ESP_ERR_INVALID_STATE;

    if(f_lseek(&fp, FRAME_FILE_HEADER_SIZE) != FR_OK)
        return ESP_FAIL;

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
    if(!opened)
        return ESP_ERR_INVALID_STATE;
    if(!out)
        return ESP_ERR_INVALID_ARG;

    static uint8_t raw[FRAME_RAW_MAX_SIZE];
    UINT br;

    memset(out, 0, sizeof(*out));

    FRESULT fr = f_read(&fp, raw, g_frame_size, &br);
    if(fr != FR_OK || br != g_frame_size)
        return ESP_ERR_NOT_FOUND; /* EOF */

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
    for(int ch = 0; ch < PCA9955B_CH_NUM; ch++) {
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
    for(int strip = 0; strip < WS2812B_NUM; strip++) {
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
    uint8_t chk = *p;
    (void)chk; /* currently unused */
    p += CHECKSUM_SIZE;

    /* -------- final guard -------- */
    if((uint32_t)(p - raw) != g_frame_size) {
        ESP_LOGE(TAG, "frame consume mismatch used=%u size=%u", (unsigned)(p - raw), (unsigned)g_frame_size);
        return ESP_FAIL;
    }

    return ESP_OK;
}
