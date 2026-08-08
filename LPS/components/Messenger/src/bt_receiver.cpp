/*
 * bt_receiver.cpp
 */

#include "bt_receiver.h"

#include "bt_hci.hpp"
#include "bt_scheduler.hpp"
#include "esp_log.h"

static const char* TAG = "BT_RX";

esp_err_t bt_receiver_init(const bt_receiver_config_t* config) {
    esp_err_t err = bt_scheduler_init(config);
    if(err != ESP_OK) {
        ESP_LOGE(TAG, "receiver initialization failed while creating the scheduler: %s", esp_err_to_name(err));
        return err;
    }

    bt_hci_controller_init(bt_scheduler_vhci_callback());
    ESP_LOGI(TAG, "Bluetooth receiver initialized");
    return ESP_OK;
}

esp_err_t bt_receiver_start(void) {
    if(bt_scheduler_is_running())
        return ESP_OK;

    bt_hci_start_scan();
    bt_scheduler_start();
    ESP_LOGI(TAG, "Bluetooth receiver started; scanning enabled");
    return ESP_OK;
}

esp_err_t bt_receiver_stop(void) {
    bt_scheduler_request_stop();
    bt_hci_stop_scan();
    bt_scheduler_stop_timers();
    ESP_LOGI(TAG, "Bluetooth receiver stopped; scanning disabled");
    return ESP_OK;
}

esp_err_t bt_receiver_deinit(void) {
    if(bt_scheduler_is_running()) {
        bt_receiver_stop();
    }

    bt_hci_unregister_callback();
    bt_scheduler_deinit();
    bt_hci_controller_deinit();

    ESP_LOGI(TAG, "Bluetooth receiver deinitialized");
    return ESP_OK;
}
