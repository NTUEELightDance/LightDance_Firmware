#include "LedController.hpp"

#include "string.h"

#include "esp_check.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "ld_board.h"
#include "ld_config.h"

static const char* TAG = "LED_CTRL";

bool pca_enable[LD_BOARD_PCA9955B_NUM] = {0};

LedController::LedController() {}

LedController::~LedController() {}

esp_err_t LedController::init() {
    esp_err_t ret = ESP_OK;
    int active_pca_devices = 0;
    ESP_LOGI(TAG, "LED controller initialization started: ws2812b_devices=%d pca9955b_devices=%d", LD_BOARD_WS2812B_NUM, LD_BOARD_PCA9955B_NUM);

    // 1. Input Validation
    ESP_RETURN_ON_FALSE(GPIO_IS_VALID_GPIO(LD_BOARD_I2C_SDA_GPIO), ESP_ERR_INVALID_ARG, TAG, "invalid I2C SDA GPIO: gpio=%d", LD_BOARD_I2C_SDA_GPIO);
    ESP_RETURN_ON_FALSE(GPIO_IS_VALID_GPIO(LD_BOARD_I2C_SCL_GPIO), ESP_ERR_INVALID_ARG, TAG, "invalid I2C SCL GPIO: gpio=%d", LD_BOARD_I2C_SCL_GPIO);

    // 2. Initialize output handles to 0
    memset(ws2812b_devs, 0, sizeof(ws2812b_devs));
    memset(pca9955b_devs, 0, sizeof(pca9955b_devs));
    bus_handle = NULL;
    ESP_LOGD(TAG, "LED device handles cleared");

    // 3. Initialize I2C Bus
    ESP_LOGD(TAG, "I2C bus initialization started: sda_gpio=%d scl_gpio=%d", LD_BOARD_I2C_SDA_GPIO, LD_BOARD_I2C_SCL_GPIO);
    ESP_GOTO_ON_ERROR(i2c_bus_init(LD_BOARD_I2C_SDA_GPIO, LD_BOARD_I2C_SCL_GPIO, &bus_handle), err, TAG, "I2C bus initialization failed");
    ESP_LOGD(TAG, "I2C bus ready");

    // 4. Initialize WS2812B Strips
    for(int i = 0; i < LD_BOARD_WS2812B_NUM; i++) {
        ESP_LOGD(TAG, "WS2812B initialization started: device=%d gpio=%d pixels=%d", i, BOARD_HW_CONFIG.rmt_pins[i], ch_info.rmt_strips[i]);
#if LD_CFG_IGNORE_DRIVER_INIT_FAIL
        esp_err_t ws_ret = ws2812b_init(&ws2812b_devs[i], BOARD_HW_CONFIG.rmt_pins[i], ch_info.rmt_strips[i]);
        if(ws_ret != ESP_OK) {
            ESP_LOGW(TAG, "WS2812B initialization failed; continuing without device: device=%d err=%s", i, esp_err_to_name(ws_ret));
        }
#else
        ESP_GOTO_ON_ERROR(ws2812b_init(&ws2812b_devs[i], BOARD_HW_CONFIG.rmt_pins[i], ch_info.rmt_strips[i]), err, TAG, "WS2812B initialization failed: device=%d", i);
#endif
    }

    // 5. Initialize PCA9955B Chips
    for(int i = 0; i < LD_BOARD_PCA9955B_NUM; i++) {
        pca_enable[i] = false;
        ESP_LOGD(TAG, "PCA9955B initialization started: device=%d address=0x%02x", i, BOARD_HW_CONFIG.i2c_addrs[i]);
        esp_err_t probe_ret = i2c_master_probe(bus_handle, BOARD_HW_CONFIG.i2c_addrs[i], LD_BOARD_I2C_PROBE_TIMEOUT_MS);
        if(probe_ret != ESP_OK) {
            ESP_LOGD(TAG, "PCA9955B disabled: device=%d address=0x%02x reason=not_detected probe_result=%s", i, BOARD_HW_CONFIG.i2c_addrs[i], esp_err_to_name(probe_ret));
            continue;
        }

        ESP_LOGD(TAG, "PCA9955B detected: device=%d address=0x%02x", i, BOARD_HW_CONFIG.i2c_addrs[i]);
        esp_err_t pca_ret = pca9955b_init(&pca9955b_devs[i], BOARD_HW_CONFIG.i2c_addrs[i], bus_handle);
        if(pca_ret == ESP_OK) {
            pca_enable[i] = true;
            active_pca_devices++;
            continue;
        }

#if LD_CFG_IGNORE_DRIVER_INIT_FAIL
        ESP_LOGD(TAG, "PCA9955B disabled: device=%d address=0x%02x reason=initialization_failed err=%s", i, BOARD_HW_CONFIG.i2c_addrs[i], esp_err_to_name(pca_ret));
#else
        ESP_LOGE(TAG, "PCA9955B initialization failed: device=%d address=0x%02x err=%s", i, BOARD_HW_CONFIG.i2c_addrs[i], esp_err_to_name(pca_ret));
        ret = pca_ret;
        goto err;
#endif
    }

    ESP_LOGI(TAG, "LED controller initialized: ws2812b_devices=%d active_pca9955b_devices=%d configured_pca9955b_devices=%d", LD_BOARD_WS2812B_NUM, active_pca_devices, LD_BOARD_PCA9955B_NUM);
    return ESP_OK;

err:
    // If init failed, cleanup whatever was allocated
    ESP_LOGE(TAG, "LED controller initialization failed; releasing initialized devices: %s", esp_err_to_name(ret));
    deinit();
    return ret;
}

esp_err_t LedController::write_channel(int ch_idx, const grb8_t* data) {
    // 1. Validate Input
    ESP_RETURN_ON_FALSE(data, ESP_ERR_INVALID_ARG, TAG, "channel write rejected: data buffer is NULL channel=%d", ch_idx);

    // 2. Handle PCA9955B Strips
    if(ch_idx < LD_BOARD_PCA9955B_CH_NUM) {

        // Calculate device and pixel index (LD_BOARD_PCA9955B_RGB_PER_IC LEDs per PCA9955B chip)
        int dev_idx = ch_idx / LD_BOARD_PCA9955B_RGB_PER_IC;
        int pixel_idx = ch_idx % LD_BOARD_PCA9955B_RGB_PER_IC;

        // Validate PCA device bounds
        if(dev_idx >= LD_BOARD_PCA9955B_NUM) {
            ESP_LOGE(TAG, "PCA9955B channel index is out of range: channel=%d device=%d maximum_device=%d", ch_idx, dev_idx, LD_BOARD_PCA9955B_NUM - 1);
            return ESP_ERR_INVALID_ARG;
        }

        if(!pca_enable[dev_idx]) {
            return ESP_OK;
        }

        return pca9955b_set_pixel(&pca9955b_devs[dev_idx], pixel_idx, data[0]);
    }

    // 3. Handle WS2812B Strips
    if(ch_idx >= LD_BOARD_PCA9955B_CH_NUM) {
        int ws_idx = ch_idx - LD_BOARD_PCA9955B_CH_NUM;
        ESP_RETURN_ON_FALSE(ws_idx >= 0 && ws_idx < LD_BOARD_WS2812B_NUM,
                            ESP_ERR_INVALID_ARG,
                            TAG,
                            "WS2812B channel write rejected: channel=%d strip=%d valid_strip_range=0..%d",
                            ch_idx,
                            ws_idx,
                            LD_BOARD_WS2812B_NUM - 1);

        // Ensure the device handle is valid before writing
        ESP_RETURN_ON_FALSE(ws2812b_devs[ws_idx].rmt_channel && ws2812b_devs[ws_idx].rmt_encoder, ESP_ERR_INVALID_STATE, TAG, "WS2812B channel write rejected: strip=%d is not initialized", ws_idx);

        // Pass the full strip buffer to the HAL
        return ws2812b_write_grb(&ws2812b_devs[ws_idx], data, ws2812b_devs[ws_idx].pixel_num);
    }

    return ESP_ERR_INVALID_ARG;
}

esp_err_t LedController::write_frame(const frame_data* frame) {
    ESP_RETURN_ON_FALSE(frame, ESP_ERR_INVALID_ARG, TAG, "frame write rejected: frame pointer is NULL");
    ESP_LOGV(TAG, "frame buffer write started");

    for(int i = 0; i < LD_BOARD_PCA9955B_CH_NUM; ++i) {
        ESP_RETURN_ON_ERROR(write_channel(i, &frame->pca9955b[i]), TAG, "PCA9955B channel buffer write failed: channel=%d", i);
    }

    for(int i = 0; i < LD_BOARD_WS2812B_NUM; ++i) {
        ESP_RETURN_ON_ERROR(write_channel(i + LD_BOARD_PCA9955B_CH_NUM, frame->ws2812b[i]), TAG, "WS2812B strip buffer write failed: strip=%d", i);
    }

    ESP_LOGV(TAG, "frame buffer write completed");
    return ESP_OK;
}

esp_err_t LedController::show() {
    esp_err_t ret = ESP_OK;
    esp_err_t err = ESP_OK;
    ESP_LOGV(TAG, "LED frame transmission started");

#if LD_CFG_SHOW_TIME_PER_FRAME
    uint64_t start = esp_timer_get_time();
#endif

    // 1. Trigger WS2812B transmission (Asynchronous/Non-blocking)
    ESP_LOGV(TAG, "transmission phase 1: start WS2812B range=[0,%d)", LD_BOARD_LED_SHOW_WS_FIRST_BATCH_SIZE);
    for(int i = 0; i < LD_BOARD_LED_SHOW_WS_FIRST_BATCH_SIZE; i++) {
        err = ws2812b_show(&ws2812b_devs[i]);
        if(err != ESP_OK) {
            // Log error but continue to try updating other LEDs
            ESP_LOGE(TAG, "WS2812B transmission start failed: device=%d err=%s", i, esp_err_to_name(err));
            ret = err;  // Latch the error code
        }
    }

    // 3. Wait for WS2812B transmission to complete
    ESP_LOGV(TAG, "transmission phase 2: wait for WS2812B range=[0,%d)", LD_BOARD_LED_SHOW_WS_FIRST_BATCH_SIZE);
    for(int i = 0; i < LD_BOARD_LED_SHOW_WS_FIRST_BATCH_SIZE; i++) {
        err = ws2812b_wait_done(&ws2812b_devs[i]);
        if(err != ESP_OK) {
            ESP_LOGE(TAG, "WS2812B transmission wait failed: device=%d err=%s", i, esp_err_to_name(err));
            ret = err;
        }
    }

    // 2. Trigger PCA9955B transmission (Synchronous/Blocking)
    ESP_LOGV(TAG, "transmission phase 3: flush PCA9955B range=[0,%d)", LD_BOARD_LED_SHOW_PCA_FIRST_BATCH_SIZE);
    for(int i = 0; i < LD_BOARD_LED_SHOW_PCA_FIRST_BATCH_SIZE; i++) {
        if(!pca_enable[i]) {
            ESP_LOGV(TAG, "PCA9955B transmission skipped: device=%d reason=disabled", i);
            continue;
        }

        err = pca9955b_show(&pca9955b_devs[i]);
        if(err != ESP_OK) {
            ESP_LOGE(TAG, "PCA9955B transmission failed: device=%d err=%s", i, esp_err_to_name(err));
            ret = err;
        }
    }

    // 1. Trigger WS2812B transmission (Asynchronous/Non-blocking)
    ESP_LOGV(TAG, "transmission phase 4: start WS2812B range=[%d,%d)", LD_BOARD_LED_SHOW_WS_FIRST_BATCH_SIZE, LD_BOARD_WS2812B_NUM);
    for(int i = LD_BOARD_LED_SHOW_WS_FIRST_BATCH_SIZE; i < LD_BOARD_WS2812B_NUM; i++) {
        err = ws2812b_show(&ws2812b_devs[i]);
        if(err != ESP_OK) {
            // Log error but continue to try updating other LEDs
            ESP_LOGE(TAG, "WS2812B transmission start failed: device=%d err=%s", i, esp_err_to_name(err));
            ret = err;  // Latch the error code
        }
    }

    // 3. Wait for WS2812B transmission to complete
    ESP_LOGV(TAG, "transmission phase 5: wait for WS2812B range=[%d,%d)", LD_BOARD_LED_SHOW_WS_FIRST_BATCH_SIZE, LD_BOARD_WS2812B_NUM);
    for(int i = LD_BOARD_LED_SHOW_WS_FIRST_BATCH_SIZE; i < LD_BOARD_WS2812B_NUM; i++) {
        err = ws2812b_wait_done(&ws2812b_devs[i]);
        if(err != ESP_OK) {
            ESP_LOGE(TAG, "WS2812B transmission wait failed: device=%d err=%s", i, esp_err_to_name(err));
            ret = err;
        }
    }

    // 2. Trigger PCA9955B transmission (Synchronous/Blocking)
    ESP_LOGV(TAG, "transmission phase 6: flush PCA9955B range=[%d,%d)", LD_BOARD_LED_SHOW_PCA_FIRST_BATCH_SIZE, LD_BOARD_PCA9955B_NUM);
    for(int i = LD_BOARD_LED_SHOW_PCA_FIRST_BATCH_SIZE; i < LD_BOARD_PCA9955B_NUM; i++) {
        if(!pca_enable[i]) {
            ESP_LOGV(TAG, "PCA9955B transmission skipped: device=%d reason=disabled", i);
            continue;
        }

        err = pca9955b_show(&pca9955b_devs[i]);
        if(err != ESP_OK) {
            ESP_LOGE(TAG, "PCA9955B transmission failed: device=%d err=%s", i, esp_err_to_name(err));
            ret = err;
        }
    }

#if LD_CFG_SHOW_TIME_PER_FRAME
    uint64_t end = esp_timer_get_time();
    ESP_LOGD(TAG, "LED frame transmission duration_us=%llu", (end - start));
#endif

    if(ret == ESP_OK) {
        ESP_LOGV(TAG, "LED frame transmission completed");
    } else {
        ESP_LOGW(TAG, "LED frame transmission completed with partial output: %s", esp_err_to_name(ret));
    }

    // Return the last error encountered, or ESP_OK if all went well
    return ret;
}

esp_err_t LedController::deinit() {
    ESP_LOGI(TAG, "LED controller deinitialization started");

    // 1. Free WS2812B Devices
    for(int i = 0; i < LD_BOARD_WS2812B_NUM; i++) {
        ESP_RETURN_ON_ERROR(ws2812b_del(&ws2812b_devs[i]), TAG, "WS2812B deinitialization failed: device=%d", i);
    }

    // 2. Free PCA9955B Devices
    for(int i = 0; i < LD_BOARD_PCA9955B_NUM; i++) {
        if(!pca_enable[i]) {
            continue;
        }
        ESP_RETURN_ON_ERROR(pca9955b_del(&pca9955b_devs[i]), TAG, "PCA9955B deinitialization failed: device=%d", i);
        pca_enable[i] = false;
    }

    // 3. Free I2C Bus
    if(bus_handle != NULL) {
        esp_err_t err = i2c_del_master_bus(bus_handle);
        if(err != ESP_OK) {
            ESP_LOGW(TAG, "I2C bus deletion failed during LED controller deinitialization: %s", esp_err_to_name(err));
        }
        bus_handle = NULL;  // Prevent double-free if deinit is called again
    }

    ESP_LOGI(TAG, "LED controller deinitialized");
    return ESP_OK;
}

esp_err_t LedController::fill(grb8_t color) {
    esp_err_t ret = ESP_OK;
    esp_err_t err = ESP_OK;
    ESP_LOGV(TAG, "filling LED buffers: r=%u g=%u b=%u", color.r, color.g, color.b);

    // 1. Fill WS2812B Strips
    for(int i = 0; i < LD_BOARD_WS2812B_NUM; i++) {
        // Ensure the device handle is valid before operation
        err = ws2812b_fill(&ws2812b_devs[i], color);

        if(err != ESP_OK) {
            ESP_LOGW(TAG, "WS2812B buffer fill failed; output will be partial: device=%d err=%s", i, esp_err_to_name(err));
            ret = err;
        }
    }

    // 2. Fill PCA9955B Chips
    for(int i = 0; i < LD_BOARD_PCA9955B_NUM; i++) {
        if(!pca_enable[i]) {
            continue;
        }

        // Ensure the device handle is valid
        err = pca9955b_fill(&pca9955b_devs[i], color);

        if(err != ESP_OK) {
            ESP_LOGW(TAG, "PCA9955B buffer fill failed; output will be partial: device=%d err=%s", i, esp_err_to_name(err));
            ret = err;
        }
    }

    // Return ESP_OK only if all devices succeeded, otherwise return the last error code
    if(ret == ESP_OK) {
        ESP_LOGV(TAG, "LED buffer fill completed");
    } else {
        ESP_LOGW(TAG, "LED buffer fill completed with partial output: %s", esp_err_to_name(ret));
    }
    return ret;
}

esp_err_t LedController::black_out() {
    // 1. Clear internal buffers to Black (0, 0, 0)
    ESP_LOGD(TAG, "blackout started");

    esp_err_t ret = fill(GRB_BLACK);

    if(ret != ESP_OK) {
        ESP_LOGW(TAG, "blackout buffer fill was incomplete: %s", esp_err_to_name(ret));
    }

    // 2. Flush changes to hardware immediately
    esp_err_t ret_show = show();
    if(ret_show != ESP_OK) {
        ESP_LOGW(TAG, "blackout transmission failed: %s", esp_err_to_name(ret_show));
    }

    // 3. Return the first error encountered
    if(ret != ESP_OK) {
        return ret;
    }

    ESP_LOGD(TAG, "blackout completed");
    return ret_show;
}

void LedController::print_buffer() {
    ESP_LOGV(TAG, "WS2812B buffer dump requested: strips=%d", LD_BOARD_WS2812B_NUM);
    for(int i = 0; i < LD_BOARD_WS2812B_NUM; i++) {
        ws2812b_print_buffer(&ws2812b_devs[i]);
    }
}
