#include "bt_ack.hpp"

#include <stdlib.h>

#include "bt_hci.hpp"
#include "bt_protocol.hpp"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char* TAG = "BT_RECEIVER";

typedef struct {
    uint8_t my_id;
    uint8_t cmd_id;
    uint8_t cmd_type;
    uint32_t delay_val;
    uint8_t state;
} ack_task_params_t;

static void send_ack_task(void* arg) {
    ack_task_params_t* params = (ack_task_params_t*)arg;
    vTaskDelay(pdMS_TO_TICKS(150)); // Safety Delay
    // Stop Scanning First, Release RF for TX.
    bt_hci_set_scan_enabled(0);
    vTaskDelay(pdMS_TO_TICKS(5));
    ESP_LOGD(TAG, ">>> ACK START: ID=%d, CMD=%d (Scan Stopped)", params->my_id, params->cmd_id);

    // Set Params
    bt_hci_set_ack_adv_params();
    vTaskDelay(pdMS_TO_TICKS(20));

    // Build Payload Data
    uint8_t raw_data[31];
    uint8_t data_len = bt_protocol_build_ack_adv_data(params->my_id,
                                                      params->cmd_id,
                                                      params->cmd_type,
                                                      params->delay_val,
                                                      params->state,
                                                      raw_data);

    bt_hci_set_adv_data(data_len, raw_data);
    vTaskDelay(pdMS_TO_TICKS(20));

    // Start Adv
    bt_hci_set_adv_enabled(1);
    vTaskDelay(pdMS_TO_TICKS(300)); // Broadcast for 300ms

    // Stop Adv
    bt_hci_set_adv_enabled(0);
    ESP_LOGD(TAG, ">>> ACK STOPPED. Resuming Scan.");

    // Resume Scanning
    bt_hci_set_scan_enabled(1);

    free(params);
    vTaskDelete(NULL);
}

void bt_ack_trigger(uint8_t my_id,
                    uint8_t cmd_id,
                    uint8_t cmd_type,
                    uint32_t delay_val,
                    uint8_t state) {
    ack_task_params_t* params = (ack_task_params_t*)malloc(sizeof(ack_task_params_t));
    if (params) {
        params->my_id = my_id;
        params->cmd_id = cmd_id;
        params->cmd_type = cmd_type;
        params->delay_val = delay_val;
        params->state = state;
        xTaskCreate(send_ack_task, "ack_task", 4096, params, 5, NULL);
    }
}
