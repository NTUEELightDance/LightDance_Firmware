#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
// #include "console.h"
#include "bt_receiver.h"
#include "nvs_flash.h"

extern "C" void app_main(void)
{
    nvs_flash_init();
    Player::getInstance().init();
    // start_console();
    bt_receiver_config_t rx_cfg = {
        .feedback_gpio_num = -1,
        .manufacturer_id = 0xFFFF,
        .my_player_id = 1,          
        .sync_window_us = 500000,
        .queue_size = 20
    };
    bt_receiver_init(&rx_cfg);
    bt_receiver_start();
}