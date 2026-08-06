#include "control_reader.h"

#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "esp_log.h"
#include "ld_board.h"

static const char* TAG = "control_reader";

static const uint8_t EXPECTED_VERSION_MAJOR = 1;
static const uint8_t EXPECTED_VERSION_MINOR = 2;

static uint32_t* s_timestamps = NULL;
static uint32_t s_frame_num = 0;
static bool s_timeline_loaded = false;

/* checksum helper */
static inline void checksum_add_u8(uint32_t* sum, uint8_t b) {
    *sum += (uint32_t)b;
}
static inline void checksum_add_u32(uint32_t* sum, uint32_t val) {
    checksum_add_u8(sum, val & 0xFF);
    checksum_add_u8(sum, (val >> 8) & 0xFF);
    checksum_add_u8(sum, (val >> 16) & 0xFF);
    checksum_add_u8(sum, (val >> 24) & 0xFF);
}

/* -------------------------------------------------- */

static void clear_cached_timestamps(void) {
    free(s_timestamps);
    s_timestamps = NULL;
    s_frame_num = 0;
    s_timeline_loaded = false;
}

/* -------------------------------------------------- */

esp_err_t get_channel_info(const char* control_path, ch_info_t* out) {
    if(!control_path || !out) {
        return ESP_ERR_INVALID_ARG;
    }

    clear_cached_timestamps();
    memset(out, 0, sizeof(*out));

    uint32_t checksum_calc = 0;
    uint32_t checksum_read = 0;
    uint32_t* timestamps = NULL;

    FILE* fp = fopen(control_path, "rb");
    if(!fp) {
        ESP_LOGE(TAG, "open %s failed (errno=%d)", control_path, errno);
        return errno == ENOENT ? ESP_ERR_NOT_FOUND : ESP_FAIL;
    }

    /* ===== version check ===== */
    uint8_t version_bytes[2];
    if(fread(version_bytes, 1, sizeof(version_bytes), fp) != sizeof(version_bytes)) {
        ESP_LOGE(TAG, "Failed to read version header");
        fclose(fp);
        return ESP_FAIL;
    }

    uint8_t major = version_bytes[0];
    uint8_t minor = version_bytes[1];

    checksum_add_u8(&checksum_calc, major);
    checksum_add_u8(&checksum_calc, minor);

    if(major != EXPECTED_VERSION_MAJOR || minor != EXPECTED_VERSION_MINOR) {
        goto version_fail;
    }

    ESP_LOGI(TAG, "control.dat version: %d.%d (OK)", major, minor);

    /* ===== PCA9955B enable flags ===== */
    for(int i = 0; i < LD_BOARD_PCA9955B_CH_NUM; i++) {
        uint8_t v;
        if(fread(&v, 1, 1, fp) != 1) {
            goto io_fail;
        }
        checksum_add_u8(&checksum_calc, v);
        if(v > 1) {
            ESP_LOGE(TAG, "of_enable[%d]=%u invalid", i, v);
            goto fmt_fail;
        }

        out->i2c_leds[i] = v ? 1 : 0;
    }

    /* ===== WS2812B strip LED counts ===== */
    for(int i = 0; i < LD_BOARD_WS2812B_NUM; i++) {
        uint8_t v;
        if(fread(&v, 1, 1, fp) != 1) {
            goto io_fail;
        }
        checksum_add_u8(&checksum_calc, v);
        if(v > LD_BOARD_WS2812B_MAX_PIXEL_NUM) {
            ESP_LOGE(TAG, "strip_led_num[%d]=%u > %u", i, v, LD_BOARD_WS2812B_MAX_PIXEL_NUM);
            goto fmt_fail;
        }

        out->rmt_strips[i] = v;
    }

    /* ===== frame_num ===== */
    uint32_t frame_num;
    if(fread(&frame_num, 1, sizeof(frame_num), fp) != sizeof(frame_num)) {
        goto io_fail;
    }

    checksum_add_u32(&checksum_calc, frame_num);

    if(frame_num > 0) {
        timestamps = (uint32_t*)malloc(frame_num * sizeof(uint32_t));
        if(!timestamps) {
            ESP_LOGE(TAG, "no memory for %lu timestamps", (unsigned long)frame_num);
            fclose(fp);
            memset(out, 0, sizeof(*out));
            return ESP_ERR_NO_MEM;
        }
    }

    /* ===== timestamps ===== */
    for(uint32_t i = 0; i < frame_num; i++) {
        uint32_t timestamp;
        if(fread(&timestamp, 1, sizeof(timestamp), fp) != sizeof(timestamp)) {
            goto io_fail;
        }
        checksum_add_u32(&checksum_calc, timestamp);
        timestamps[i] = timestamp;
    }

    /* ===== checksum ===== */
    if(fread(&checksum_read, 1, sizeof(checksum_read), fp) != sizeof(checksum_read)) {
        goto io_fail;
    }

    /* ===== verify checksum ===== */
    if(checksum_read != checksum_calc) {
        ESP_LOGE(TAG, "checksum mismatch! read=%lu calculated=%lu", (unsigned long)checksum_read, (unsigned long)checksum_calc);
        free(timestamps);
        fclose(fp);
        memset(out, 0, sizeof(*out));
        return ESP_ERR_INVALID_CRC;
    }

    fclose(fp);
    s_timestamps = timestamps;
    s_frame_num = frame_num;
    s_timeline_loaded = true;

    if(frame_num == 0) {
        ESP_LOGI(TAG, "timeline loaded: frame_num=0");
    } else {
        ESP_LOGI(TAG, "timeline loaded: frame_num=%lu first_ts=%lu last_ts=%lu", (unsigned long)frame_num, (unsigned long)s_timestamps[0], (unsigned long)s_timestamps[frame_num - 1U]);
    }
    ESP_LOGI(TAG, "channel info loaded, checksum OK");
    return ESP_OK;
    /* ---------------- error paths ---------------- */

io_fail:
    ESP_LOGE(TAG, "I/O error while reading %s", control_path);
    free(timestamps);
    fclose(fp);
    memset(out, 0, sizeof(*out));
    return ESP_FAIL;

fmt_fail:
    ESP_LOGE(TAG, "format error in %s", control_path);
    free(timestamps);
    fclose(fp);
    memset(out, 0, sizeof(*out));
    return ESP_ERR_INVALID_RESPONSE;

version_fail:
    ESP_LOGE(TAG, "Version mismatch! Expected %d.%d, got %d.%d", EXPECTED_VERSION_MAJOR, EXPECTED_VERSION_MINOR, major, minor);
    free(timestamps);
    fclose(fp);
    memset(out, 0, sizeof(*out));
    return ESP_FAIL;
}

esp_err_t control_reader_find_seek_frame_idx(uint64_t time_ms, uint32_t* out_frame_idx) {
    if(!out_frame_idx) {
        return ESP_ERR_INVALID_ARG;
    }
    if(!s_timeline_loaded) {
        return ESP_ERR_INVALID_STATE;
    }
    if(s_frame_num == 0) {
        return ESP_ERR_NOT_FOUND;
    }

    uint32_t frame_idx = 0;

    while((frame_idx + 1U) < s_frame_num && (uint64_t)s_timestamps[frame_idx + 1U] <= time_ms) {
        frame_idx++;
    }

    if(s_frame_num >= 2U && frame_idx >= (s_frame_num - 1U)) {
        frame_idx = s_frame_num - 2U;
    }

    *out_frame_idx = frame_idx;
    return ESP_OK;
}

uint32_t control_reader_frame_count(void) {
    return s_frame_num;
}

esp_err_t control_reader_get_timestamp(uint32_t frame_idx, uint32_t* out_timestamp) {
    if(!out_timestamp) {
        return ESP_ERR_INVALID_ARG;
    }
    if(!s_timeline_loaded) {
        return ESP_ERR_INVALID_STATE;
    }
    if(frame_idx >= s_frame_num) {
        return ESP_ERR_NOT_FOUND;
    }

    *out_timestamp = s_timestamps[frame_idx];
    return ESP_OK;
}

void control_reader_clear(void) {
    clear_cached_timestamps();
}
/* ---------- helpers ---------- */

// static esp_err_t fr_to_err(FRESULT fr)
// {
//     switch (fr) {
//     case FR_OK:         return ESP_OK;
//     case FR_NO_FILE:
//     case FR_NO_PATH:    return ESP_ERR_NOT_FOUND;
//     case FR_DENIED:     return ESP_ERR_INVALID_STATE;
//     default:            return ESP_FAIL;
//     }
// }

/* ---------- public API ---------- */

// void control_reader_free(control_info_t *info)
// {
//     if (!info) return;

//     if (info->timestamps) {
//         free(info->timestamps);
//         info->timestamps = NULL;
//     }

//     memset(info, 0, sizeof(*info));
// }

// esp_err_t control_reader_load(const char *path, control_info_t *out)

// {
//     if (!path || !out) return ESP_ERR_INVALID_ARG;
//     memset(out, 0, sizeof(*out));

//     FIL fp;
//     UINT br;
//     FRESULT fr = f_open(&fp, path, FA_READ);
//     if (fr != FR_OK) {
//         ESP_LOGE(TAG, "open %s failed (fr=%d)", path, (int)fr);
//         return fr_to_err(fr);
//     }

//     /* ===== version ===== */
//     uint8_t ver_bytes[2];
//     if (f_read(&fp, ver_bytes, 2, &br) != FR_OK || br != 2)
//         goto io_fail;

//     out->version = (uint16_t)ver_bytes[0] |
//                    ((uint16_t)ver_bytes[1] << 8);

//     /* ===== OF enable flags (40) ===== */
//     if (f_read(&fp, out->of_enable, LD_BOARD_PCA9955B_CH_NUM, &br) != FR_OK ||
//         br != LD_BOARD_PCA9955B_CH_NUM)
//         goto io_fail;

//     /* constraint: OF enable must be 0 or 1 */
//     for (int i = 0; i < LD_BOARD_PCA9955B_CH_NUM; i++) {
//         if (out->of_enable[i] > 1) {
//             ESP_LOGE(TAG, "of_enable[%d]=%u invalid", i, out->of_enable[i]);
//             goto fmt_fail;
//         }
//     }

//     /* ===== Strip LED counts (8) ===== */
//     /* ===== Strip LED counts (8) ===== */
//     for (int i = 0; i < LD_BOARD_WS2812B_NUM; i++) {
//         uint8_t v;
//         if (f_read(&fp, &v, 1, &br) != FR_OK || br != 1)
//             goto io_fail;

//         out->strip_led_num[i] = v;

//         if (out->strip_led_num[i] > LD_BOARD_WS2812B_MAX_PIXEL_NUM) {
//             ESP_LOGE(TAG, "strip_led_num[%d]=%u > %u",
//                     i,
//                     (unsigned)out->strip_led_num[i],
//                     (unsigned)LD_BOARD_WS2812B_MAX_PIXEL_NUM);
//             goto fmt_fail;
//         }
//     }

//     /* ===== frame_num ===== */
//     if (f_read(&fp, &out->frame_num, 4, &br) != FR_OK || br != 4)
//         goto io_fail;

//     /* sanity check */
//     if (out->frame_num == 0) {
//         ESP_LOGW(TAG, "frame_num = 0");
//         out->timestamps = NULL;
//         f_close(&fp);
//         return ESP_OK;
//     }

//     /* ===== timestamps ===== */
//     out->timestamps = (uint32_t *)malloc(out->frame_num * sizeof(uint32_t));
//     if (!out->timestamps)
//         goto mem_fail;

//     if (f_read(&fp,
//                out->timestamps,
//                out->frame_num * sizeof(uint32_t),
//                &br) != FR_OK ||
//         br != out->frame_num * sizeof(uint32_t))
//         goto io_fail;

//     f_close(&fp);

//     ESP_LOGI(TAG,
//              "loaded %s: ver=%u, frames=%u",
//              path,
//              (unsigned)out->version,
//              (unsigned)out->frame_num);

//     return ESP_OK;

// /* ---------- error paths ---------- */

// io_fail:
//     ESP_LOGE(TAG, "I/O error while reading %s", path);
//     f_close(&fp);
//     control_reader_free(out);
//     return ESP_FAIL;

// mem_fail:
//     ESP_LOGE(TAG, "no memory while reading %s", path);
//     f_close(&fp);
//     control_reader_free(out);
//     return ESP_ERR_NO_MEM;

// fmt_fail:
//     ESP_LOGE(TAG, "format/constraint error in %s", path);
//     f_close(&fp);
//     control_reader_free(out);
//     return ESP_ERR_INVALID_RESPONSE;
// }
