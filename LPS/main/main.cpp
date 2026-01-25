#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "bt_receiver.h"
#include "esp_err.h"
#include "nvs_flash.h"

#include "player.hpp"
#include "readframe.h"

static const char* TAG = "APP";

static void app_task(void* arg) {
    // (void)arg;
    ESP_LOGI(TAG, "app_task start, HWM=%u", uxTaskGetStackHighWaterMark(NULL));

#if SD_ENABLE
    esp_err_t err = frame_system_init("0:/control.dat", "0:/frame.dat");
    ESP_LOGI(TAG, "frame_system_init=%s", esp_err_to_name(err));
    ESP_LOGI(TAG, "HWM after frame_system_init=%u", uxTaskGetStackHighWaterMark(NULL));

    if(err != ESP_OK) {
        ESP_LOGE(TAG, "init failed, stop here");
        vTaskDelay(portMAX_DELAY);
    }
#endif

    // console_test();  // 進入 REPL（通常不會 return）
    cal_gamma_lut();
    Player::getInstance().init();
    console_test();

#if BT_ENABLE
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
#endif

    vTaskDelete(NULL);
}

extern "C" void app_main(void) {
    xTaskCreate(app_task, "app_task", 16384, NULL, 5, NULL);
    // app_main return 讓 main task 結束，不再承擔後續 stack 壓力
}
