#include "control_reader.h"

#include <string.h>
#include <stdbool.h>

#include "esp_log.h"
#include "ff.h"
#include "ld_board.h"

static const char* TAG = "control_reader";

static const uint8_t EXPECTED_VERSION_MAJOR = 1;
static const uint8_t EXPECTED_VERSION_MINOR = 2;

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
    uint32_t checksum_calc = 0;
    uint32_t checksum_read = 0;

    FRESULT fr = f_open(&fp, control_path, FA_READ);
    if(fr != FR_OK) {
        ESP_LOGE(TAG, "open %s failed (fr=%d)", control_path, fr);
        return fr_to_err(fr);
    }

    /* ===== version check ===== */
    uint8_t version_bytes[2];
    fr = f_read(&fp, version_bytes, 2, &br);

    if(fr != FR_OK || br != 2) {
        ESP_LOGE(TAG, "Failed to read version header");
        f_close(&fp);
        return ESP_FAIL ;
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
        if(f_read(&fp, &v, 1, &br) != FR_OK || br != 1) {
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
        if(f_read(&fp, &v, 1, &br) != FR_OK || br != 1) {
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
    if(f_read(&fp, &frame_num, 4, &br) != FR_OK || br != 4) {
        goto io_fail;
    }

    checksum_add_u32(&checksum_calc, frame_num);

    /* ===== timestamps ===== */
    for(uint32_t i = 0; i < frame_num; i++) {
        uint32_t timestamp;
        if(f_read(&fp, &timestamp, 4, &br) != FR_OK || br != 4) {
            goto io_fail;
        }
        checksum_add_u32(&checksum_calc, timestamp);
    }


    /* ===== checksum ===== */
    if(f_read(&fp, &checksum_read, 4, &br) != FR_OK || br != 4) {
        goto io_fail;
    }

    /* ===== verify checksum ===== */
    if(checksum_read != checksum_calc) {
        ESP_LOGE(TAG, "checksum mismatch, read=%lu calculated=%lu", (unsigned long)checksum_read, (unsigned long)checksum_calc);
        f_close(&fp);
        memset(out, 0, sizeof(*out));
        return ESP_ERR_INVALID_CRC;
    }

    f_close(&fp);

    ESP_LOGI(TAG, "channel info loaded, checksum OK");
    return ESP_OK;
    /* ---------------- error paths ---------------- */

io_fail:
    ESP_LOGE(TAG, "I/O error while reading %s (fr=%d, br=%u)",
             control_path, fr, (unsigned)br);
    f_close(&fp);
    memset(out, 0, sizeof(*out));
    return fr_to_err(fr);
fmt_fail:
    ESP_LOGE(TAG, "format error in %s", control_path);
    f_close(&fp);
    memset(out, 0, sizeof(*out));
    return ESP_ERR_INVALID_RESPONSE;

version_fail:
    ESP_LOGE(TAG, "Version mismatch, Expected %d.%d, got %d.%d", EXPECTED_VERSION_MAJOR, EXPECTED_VERSION_MINOR, major, minor);
    f_close(&fp);
    memset(out, 0, sizeof(*out));
    return ESP_ERR_NOT_SUPPORTED;
}
