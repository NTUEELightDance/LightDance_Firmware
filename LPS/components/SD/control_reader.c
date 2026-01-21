#include "control_reader.h"

#include "ff.h"
#include "esp_log.h"
#include <stdlib.h>
#include <string.h>

static const char *TAG = "control_reader";

static esp_err_t fr_to_err(FRESULT fr)
{
    switch (fr) {
        case FR_OK:         return ESP_OK;
        case FR_NO_FILE:
        case FR_NO_PATH:    return ESP_ERR_NOT_FOUND;
        case FR_DENIED:     return ESP_ERR_INVALID_STATE;
        default:            return ESP_FAIL;
    }
}

void control_reader_free(control_info_t *info)
{
    if (!info) return;
    free(info->led_num);
    free(info->timestamps);
    memset(info, 0, sizeof(*info));
}

esp_err_t control_reader_load(const char *path, control_info_t *out)
{
    if (!path || !out) return ESP_ERR_INVALID_ARG;
    memset(out, 0, sizeof(*out));

    FIL fp;
    UINT br;
    FRESULT fr = f_open(&fp, path, FA_READ);
    if (fr != FR_OK) {
        ESP_LOGE(TAG, "open %s failed (fr=%d)", path, (int)fr);
        return fr_to_err(fr);
    }

    /* ---- fixed header ---- */
    uint8_t ver_bytes[2];
    if (f_read(&fp, ver_bytes, 2, &br) != FR_OK || br != 2) goto io_fail;
    out->version = (uint16_t)ver_bytes[0] | ((uint16_t)ver_bytes[1] << 8);

    if (f_read(&fp, &out->of_num, 1, &br) != FR_OK || br != 1) goto io_fail;
    if (f_read(&fp, &out->strip_num, 1, &br) != FR_OK || br != 1) goto io_fail;

    /* document constraints: OF 0~40, Strip 0~8 */
    if (out->of_num > 40 || out->strip_num > 8) {
        ESP_LOGE(TAG, "control constraint violation: of=%u strip=%u",
                 (unsigned)out->of_num, (unsigned)out->strip_num);
        goto fmt_fail;
    }

    /* ---- LED_num[] ---- */
    out->led_num = (out->strip_num > 0) ? (uint8_t*)malloc(out->strip_num) : NULL;
    if (out->strip_num > 0 && !out->led_num) goto mem_fail;

    if (out->strip_num > 0) {
        if (f_read(&fp, out->led_num, out->strip_num, &br) != FR_OK || br != out->strip_num)
            goto io_fail;

        /* document constraint: LED_num 0~100 */
        for (uint8_t i = 0; i < out->strip_num; i++) {
            if (out->led_num[i] > 100) {
                ESP_LOGE(TAG, "LED_num[%u]=%u > 100", (unsigned)i, (unsigned)out->led_num[i]);
                goto fmt_fail;
            }
        }
    }

    /* ---- Frame_num ---- */
    if (f_read(&fp, &out->frame_num, 4, &br) != FR_OK || br != 4) goto io_fail;

    /* ---- time_stamp[] ---- */
    if (out->frame_num > 0) {
        out->timestamps = (uint32_t*)malloc(out->frame_num * sizeof(uint32_t));
        if (!out->timestamps) goto mem_fail;

        uint32_t need = out->frame_num * 4u;
        if (f_read(&fp, out->timestamps, need, &br) != FR_OK || br != need)
            goto io_fail;
    } else {
        out->timestamps = NULL;
    }

    f_close(&fp);
    ESP_LOGI(TAG, "loaded %s: of=%u strip=%u frames=%u",
             path, (unsigned)out->of_num, (unsigned)out->strip_num, (unsigned)out->frame_num);
    return ESP_OK;

io_fail:
    ESP_LOGE(TAG, "I/O error while reading %s", path);
    f_close(&fp);
    control_reader_free(out);
    return ESP_FAIL;

mem_fail:
    ESP_LOGE(TAG, "no memory while reading %s", path);
    f_close(&fp);
    control_reader_free(out);
    return ESP_ERR_NO_MEM;

fmt_fail:
    ESP_LOGE(TAG, "format/constraint error in %s", path);
    f_close(&fp);
    control_reader_free(out);
    return ESP_ERR_INVALID_RESPONSE;
}
