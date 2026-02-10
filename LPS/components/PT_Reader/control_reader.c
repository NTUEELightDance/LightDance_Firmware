#include "control_reader.h"

#include <stdlib.h>
#include <string.h>
#include "esp_log.h"
#include "ff.h"
#include "ld_board.h"

static const char* TAG = "control_reader";

/* -------------------------------------------------- */

static esp_err_t fr_to_err(FRESULT fr) {
    switch(fr) {
        case FR_OK:
            return ESP_OK;
        case FR_NO_FILE:
        case FR_NO_PATH:
            return ESP_ERR_NOT_FOUND;
        case FR_DENIED:
            return ESP_ERR_INVALID_STATE;
        default:
            return ESP_FAIL;
    }
}

/* -------------------------------------------------- */

esp_err_t get_channel_info(const char* control_path, ch_info_t* out) {
    if(!control_path || !out) {
        return ESP_ERR_INVALID_ARG;
    }

    memset(out, 0, sizeof(*out));

    FIL fp;
    UINT br;
    FRESULT fr = f_open(&fp, control_path, FA_READ);
    if(fr != FR_OK) {
        ESP_LOGE(TAG, "open %s failed (fr=%d)", control_path, fr);
        return fr_to_err(fr);
    }

    /* ===== version (skip or keep if needed) ===== */
    uint8_t ver[2];
    if(f_read(&fp, ver, 2, &br) != FR_OK || br != 2) {
        goto io_fail;
    }

    /* ===== PCA9955B enable flags ===== */
    for(int i = 0; i < LD_BOARD_PCA9955B_CH_NUM; i++) {
        uint8_t v;
        if(f_read(&fp, &v, 1, &br) != FR_OK || br != 1) {
            goto io_fail;
        }

        if(v > 1) {
            ESP_LOGE(TAG, "of_enable[%d]=%u invalid", i, v);
            goto fmt_fail;
        }

        out->i2c_leds[i] = v ? 1 : 0;
    }

    /* ===== WS2812B strip LED counts ===== */
    for(int i = 0; i < LD_BOARD_WS2812B_NUM; i++) {
        uint8_t v;
        if(f_read(&fp, &v, 1, &br) != FR_OK || br != 1) {
            goto io_fail;
        }

        if(v > LD_BOARD_WS2812B_MAX_PIXEL_NUM) {
            ESP_LOGE(TAG, "strip_led_num[%d]=%u > %u", i, v, LD_BOARD_WS2812B_MAX_PIXEL_NUM);
            goto fmt_fail;
        }

        out->rmt_strips[i] = v;
    }

    /* ===== frame_num (read & ignore) ===== */
    uint32_t frame_num;
    if(f_read(&fp, &frame_num, 4, &br) != FR_OK || br != 4) {
        goto io_fail;
    }

    f_close(&fp);

    ESP_LOGI(TAG, "channel info loaded: PCA=%d, WS=%d", LD_BOARD_PCA9955B_CH_NUM, LD_BOARD_WS2812B_NUM);

    return ESP_OK;

    /* ---------------- error paths ---------------- */

io_fail:
    ESP_LOGE(TAG, "I/O error while reading %s", control_path);
    f_close(&fp);
    memset(out, 0, sizeof(*out));
    return ESP_FAIL;

fmt_fail:
    ESP_LOGE(TAG, "format error in %s", control_path);
    f_close(&fp);
    memset(out, 0, sizeof(*out));
    return ESP_ERR_INVALID_RESPONSE;
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
