#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "bt_receiver.h"
#include "esp_err.h"
#include "nvs_flash.h"

#include "player.hpp"
#include "readframe.h"
#include "sd_logger.h"

static const char* TAG = "APP";
static bool frame_sys_ready = false;

static void app_task(void* arg) {
    // (void)arg;
    ESP_LOGI(TAG, "app_task start, HWM=%u", uxTaskGetStackHighWaterMark(NULL));

#if LD_CFG_ENABLE_SD
    esp_err_t sd_err = frame_system_init("0:/control.dat", "0:/frame.dat");
    ESP_LOGI(TAG, "frame_system_init=%s", esp_err_to_name(sd_err));
    ESP_LOGI(TAG, "HWM after frame_system_init=%u", uxTaskGetStackHighWaterMark(NULL));

    vTaskDelay(pdMS_TO_TICKS(1000));

    if(sd_err != ESP_OK) {
        ESP_LOGE(TAG, "frame system init failed, halt");
        vTaskDelay(portMAX_DELAY);
        frame_sys_ready = false;
    } else {
        frame_sys_ready = true;

#if LD_CFG_ENABLE_LOGGER
        esp_err_t log_err = sd_logger_init("/sd/LOGGER.log");
        if(log_err != ESP_OK) {
            ESP_LOGE(TAG, "SD Logger init failed: %s", esp_err_to_name(log_err));
        }

        vTaskDelay(pdMS_TO_TICKS(1000));

#endif
    }

#endif

    calc_gamma_lut();

    /* ---- hardware config (temporary placement) ---- */
    for(int i = 0; i < LD_BOARD_WS2812B_NUM; i++) {
        ch_info.rmt_strips[i] = LD_BOARD_WS2812B_MAX_PIXEL_NUM;
    }
    for(int i = 0; i < LD_BOARD_PCA9955B_CH_NUM; i++) {
        ch_info.i2c_leds[i] = 1;
    }

    Player::getInstance().init();

    vTaskDelay(pdMS_TO_TICKS(1000));

#if LD_CFG_ENABLE_BT
    nvs_flash_init();
    bt_receiver_config_t rx_cfg = {
        .feedback_gpio_num = -1,
        .manufacturer_id = 0xFFFF,
        .my_player_id = 1,
        .sync_window_us = 500000,
        .queue_size = 20,
    };
    bt_receiver_init(&rx_cfg);
    bt_receiver_start();
#else
    console_test();
#endif

    vTaskDelete(NULL);
}

extern "C" void app_main(void) {
    xTaskCreate(app_task, "app_task", 16384, NULL, 5, NULL);
}
