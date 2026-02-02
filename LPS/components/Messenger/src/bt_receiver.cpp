/*
 * bt_receiver.cpp
 */

#include "bt_receiver.h"
#include <stdio.h>
#include <string.h>
#include "driver/gpio.h"
#include "esp_bt.h"
#include "esp_log.h"
#include "esp_rom_sys.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"

#include "esp_system.h"
#include "esp_vfs_fat.h"
#include "player.hpp"

// --- HCI Command Definitions ---
#define HCI_H4_CMD_PREAMBLE_SIZE (4)
#define HCI_GRP_HOST_CONT_BASEBAND_CMDS (0x03 << 10)
#define HCI_GRP_BLE_CMDS (0x08 << 10)
#define HCI_RESET (0x0003 | HCI_GRP_HOST_CONT_BASEBAND_CMDS)
#define HCI_SET_EVT_MASK (0x0001 | HCI_GRP_HOST_CONT_BASEBAND_CMDS)
#define HCI_BLE_WRITE_SCAN_PARAM (0x000B | HCI_GRP_BLE_CMDS)
#define HCI_BLE_WRITE_SCAN_ENABLE (0x000C | HCI_GRP_BLE_CMDS)
#define HCIC_PARAM_SIZE_SET_EVENT_MASK (8)
#define HCIC_PARAM_SIZE_BLE_WRITE_SCAN_PARAM (7)
#define HCIC_PARAM_SIZE_BLE_WRITE_SCAN_ENABLE (2)
#define CMD_TYPE_ACK 0x08

#define UINT16_TO_STREAM(p, u16)        \
    {                                   \
        *(p)++ = (uint8_t)(u16);        \
        *(p)++ = (uint8_t)((u16) >> 8); \
    }
#define UINT8_TO_STREAM(p, u8) \
    { *(p)++ = (uint8_t)(u8); }
#define ARRAY_TO_STREAM(p, a, len)     \
    {                                  \
        int ijk;                       \
        for(ijk = 0; ijk < len; ijk++) \
            *(p)++ = (uint8_t)a[ijk];  \
    }

enum { H4_TYPE_COMMAND = 1, H4_TYPE_ACL = 2, H4_TYPE_SCO = 3, H4_TYPE_EVENT = 4 };

static const char* TAG = "BT_RECEIVER";

// --- Internal Static Variables ---
static bt_receiver_config_t s_config;
static QueueHandle_t s_adv_queue = NULL;
static TaskHandle_t s_task_handle = NULL;
static bool s_is_running = false;
static uint8_t hci_cmd_buf[128];

typedef struct {
    volatile uint8_t target_cmd;
    volatile uint64_t target_mask;
    volatile uint8_t data[3];
} bt_action_context_t;

#define MAX_CONCURRENT_ACTIONS 16

// --- Timer Slot Structure ---
typedef struct {
    esp_timer_handle_t timer_handle;
    bt_action_context_t ctx;
} action_slot_t;

static action_slot_t s_slots[MAX_CONCURRENT_ACTIONS];
static struct {
    uint8_t cmd_id;
    uint8_t cmd_type;
    uint32_t original_delay;
    int64_t lock_timestamp;
} s_last_locked_cmd = {0, 0, 0, 0};
// ==========================================
// Part 1: HCI Helper Functions
// ==========================================

static uint16_t make_cmd_reset(uint8_t* buf) {
    UINT8_TO_STREAM(buf, H4_TYPE_COMMAND);
    UINT16_TO_STREAM(buf, HCI_RESET);
    UINT8_TO_STREAM(buf, 0);
    return HCI_H4_CMD_PREAMBLE_SIZE;
}

static uint16_t make_cmd_set_evt_mask(uint8_t* buf, uint8_t* evt_mask) {
    UINT8_TO_STREAM(buf, H4_TYPE_COMMAND);
    UINT16_TO_STREAM(buf, HCI_SET_EVT_MASK);
    UINT8_TO_STREAM(buf, HCIC_PARAM_SIZE_SET_EVENT_MASK);
    ARRAY_TO_STREAM(buf, evt_mask, HCIC_PARAM_SIZE_SET_EVENT_MASK);
    return HCI_H4_CMD_PREAMBLE_SIZE + HCIC_PARAM_SIZE_SET_EVENT_MASK;
}

static uint16_t make_cmd_ble_set_scan_params(uint8_t* buf,
                                             uint8_t scan_type,
                                             uint16_t scan_interval,
                                             uint16_t scan_window,
                                             uint8_t own_addr_type,
                                             uint8_t filter_policy) {
    UINT8_TO_STREAM(buf, H4_TYPE_COMMAND);
    UINT16_TO_STREAM(buf, HCI_BLE_WRITE_SCAN_PARAM);
    UINT8_TO_STREAM(buf, HCIC_PARAM_SIZE_BLE_WRITE_SCAN_PARAM);
    UINT8_TO_STREAM(buf, scan_type);
    UINT16_TO_STREAM(buf, scan_interval);
    UINT16_TO_STREAM(buf, scan_window);
    UINT8_TO_STREAM(buf, own_addr_type);
    UINT8_TO_STREAM(buf, filter_policy);
    return HCI_H4_CMD_PREAMBLE_SIZE + HCIC_PARAM_SIZE_BLE_WRITE_SCAN_PARAM;
}

static uint16_t make_cmd_ble_set_scan_enable(uint8_t* buf, uint8_t scan_enable, uint8_t filter_duplicates) {
    UINT8_TO_STREAM(buf, H4_TYPE_COMMAND);
    UINT16_TO_STREAM(buf, HCI_BLE_WRITE_SCAN_ENABLE);
    UINT8_TO_STREAM(buf, HCIC_PARAM_SIZE_BLE_WRITE_SCAN_ENABLE);
    UINT8_TO_STREAM(buf, scan_enable);
    UINT8_TO_STREAM(buf, filter_duplicates);
    return HCI_H4_CMD_PREAMBLE_SIZE + HCIC_PARAM_SIZE_BLE_WRITE_SCAN_ENABLE;
}

static uint16_t make_cmd_ble_set_adv_data(uint8_t *buf, uint8_t data_len, uint8_t *p_data) {
    UINT8_TO_STREAM(buf, H4_TYPE_COMMAND);
    UINT16_TO_STREAM(buf, 0x2008); // HCI_BLE_WRITE_ADV_DATA
    UINT8_TO_STREAM(buf, 32);      // HCI Param Length (Fixed 32)
    UINT8_TO_STREAM(buf, data_len); // Adv Data Length
    ARRAY_TO_STREAM(buf, p_data, data_len);
    uint8_t pad_len = 31 - data_len;
    for (int i = 0; i < pad_len; i++) {
        UINT8_TO_STREAM(buf, 0);
    }
    return HCI_H4_CMD_PREAMBLE_SIZE + 1 + 31;
}

static void hci_cmd_send_ble_adv_enable(uint8_t enable) {
    uint8_t buf[128];
    uint8_t *p = buf;
    UINT8_TO_STREAM(p, H4_TYPE_COMMAND);
    UINT16_TO_STREAM(p, 0x200A); // HCI_BLE_WRITE_ADV_ENABLE
    UINT8_TO_STREAM(p, 1);
    UINT8_TO_STREAM(p, enable);
    esp_vhci_host_send_packet(buf, p - buf);
}

static void hci_cmd_send_ble_set_adv_param_ack(void) {
    uint8_t buf[128];
    uint8_t *p = buf;
    // Standard Interval: 200ms (320 * 0.625)
    uint16_t interval = 48; 
    
    UINT8_TO_STREAM(p, H4_TYPE_COMMAND);
    UINT16_TO_STREAM(p, 0x2006); // HCI_BLE_WRITE_ADV_PARAMS
    UINT8_TO_STREAM(p, 15);
    UINT16_TO_STREAM(p, interval); // Min
    UINT16_TO_STREAM(p, interval); // Max
    
    // Use ADV_IND (Type 0)
    UINT8_TO_STREAM(p, 0); 
    
    UINT8_TO_STREAM(p, 0); // Own addr type
    UINT8_TO_STREAM(p, 0); // Peer addr type
    UINT8_TO_STREAM(p, 0); UINT8_TO_STREAM(p, 0); UINT8_TO_STREAM(p, 0); UINT8_TO_STREAM(p, 0); UINT8_TO_STREAM(p, 0); UINT8_TO_STREAM(p, 0); 
    UINT8_TO_STREAM(p, 0x07); // Channel Map
    UINT8_TO_STREAM(p, 0); // Filter Policy
    
    esp_vhci_host_send_packet(buf, p - buf);
}

typedef struct {
    uint8_t my_id;
    uint8_t cmd_id;
    uint8_t cmd_type;
    uint32_t delay_val;
    uint8_t state;
} ack_task_params_t;

static void send_ack_task(void *arg) {
    ack_task_params_t *params = (ack_task_params_t *)arg;
    
    // Stop Scanning First, Release RF for TX.
    uint8_t scan_buf[32];
    make_cmd_ble_set_scan_enable(scan_buf, 0, 0); // Disable Scan
    esp_vhci_host_send_packet(scan_buf, 6);
    esp_rom_delay_us(5000);

    ESP_LOGD(TAG, ">>> ACK START: ID=%d, CMD=%d (Scan Stopped)", params->my_id, params->cmd_id);

    // Set Params
    hci_cmd_send_ble_set_adv_param_ack();
    vTaskDelay(pdMS_TO_TICKS(20));
    
    // Build Data
    uint8_t raw_data[31];
    uint8_t idx = 0;
    
    raw_data[idx++] = 2; raw_data[idx++] = 0x01; raw_data[idx++] = 0x06;
    
    // Length = 12
    raw_data[idx++] = 12; 
    raw_data[idx++] = 0xFF; raw_data[idx++] = 0xFF; raw_data[idx++] = 0xFF; // Type + ID
    raw_data[idx++] = CMD_TYPE_ACK; // 0x08
    
    raw_data[idx++] = params->my_id;
    raw_data[idx++] = params->cmd_id;
    raw_data[idx++] = params->cmd_type;
    
    raw_data[idx++] = (params->delay_val >> 24) & 0xFF;
    raw_data[idx++] = (params->delay_val >> 16) & 0xFF;
    raw_data[idx++] = (params->delay_val >> 8) & 0xFF;
    raw_data[idx++] = (params->delay_val) & 0xFF;

    raw_data[idx++] = params->state;
    
    uint8_t hci_buf[128];
    uint16_t pkt_len = make_cmd_ble_set_adv_data(hci_buf, idx, raw_data);
    esp_vhci_host_send_packet(hci_buf, pkt_len);
    vTaskDelay(pdMS_TO_TICKS(20));

    // Start Adv
    hci_cmd_send_ble_adv_enable(1);
    //origin: 50
    vTaskDelay(pdMS_TO_TICKS(100));
    
    // Stop Adv
    hci_cmd_send_ble_adv_enable(0);
    ESP_LOGD(TAG, ">>> ACK STOPPED. Resuming Scan.");

    // Resume Scanning
    make_cmd_ble_set_scan_enable(scan_buf, 1, 0);
    esp_vhci_host_send_packet(scan_buf, 6);

    free(params);
    vTaskDelete(NULL);
}

// ==========================================
// Part 2: Fast RX Driver & ISR (Unchanged)
// ==========================================
static void IRAM_ATTR fast_parse_and_trigger(uint8_t* data, uint16_t len) {
    int64_t now_us = esp_timer_get_time();
    if(data[0] != 0x04 || data[1] != 0x3E || data[3] != 0x02) return;

    uint8_t num_reports = data[4];
    uint8_t* payload = &data[5];

    for(int i = 0; i < num_reports; i++) {
        uint8_t data_len = payload[8];
        uint8_t* adv_data = &payload[9];
        int8_t rssi = payload[9 + data_len];

        uint8_t offset = 0;
        while(offset < data_len) {
            uint8_t ad_len = adv_data[offset++];
            if(ad_len == 0) break;
            uint8_t ad_type = adv_data[offset++];

            if(ad_type == 0xFF && ad_len >= 16) {
                uint16_t target_id = s_config.manufacturer_id;
                if(adv_data[offset] == (target_id & 0xFF) && adv_data[offset + 1] == ((target_id >> 8) & 0xFF)) {
                    uint64_t rcv_mask = 0;
                    for(int k = 0; k < 8; k++) rcv_mask |= ((uint64_t)adv_data[offset + 3 + k] << (k * 8));

                    bool is_target = true;
                    if(s_config.my_player_id >= 0) {
                        if(!((rcv_mask >> s_config.my_player_id) & 1ULL)) is_target = false;
                    }

                    if(is_target) {
                        uint8_t rcv_cmd_id = (adv_data[offset + 2] >> 4) & 0x0F;
                        uint8_t rcv_cmd = adv_data[offset + 2] & 0x0F;
                        uint32_t rcv_delay = (adv_data[offset + 11] << 24) | (adv_data[offset + 12] << 16) | (adv_data[offset + 13] << 8) | (adv_data[offset + 14]);
                        uint32_t rcv_prep_time = (adv_data[offset + 15] << 24) | (adv_data[offset + 16] << 16) | (adv_data[offset + 17] << 8) | (adv_data[offset + 18]);
                        uint8_t rcv_data[3] = {adv_data[offset + 19], adv_data[offset + 20], adv_data[offset + 21]};

                        ble_rx_packet_t pkt;
                        pkt.cmd_id = rcv_cmd_id;
                        pkt.cmd_type = rcv_cmd;
                        pkt.target_mask = rcv_mask;
                        pkt.delay_val = rcv_delay;
                        pkt.prep_time = rcv_prep_time;
                        pkt.data[0] = rcv_data[0];
                        pkt.data[1] = rcv_data[1];
                        pkt.data[2] = rcv_data[2];
                        pkt.rssi = rssi;
                        pkt.rx_time_us = now_us;

                        BaseType_t xHigherPriorityTaskWoken = pdFALSE;
                        xQueueSendFromISR(s_adv_queue, &pkt, &xHigherPriorityTaskWoken);
                        if(xHigherPriorityTaskWoken) portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
                        return;
                    }
                }
            }
            offset += (ad_len - 1);
        }
        payload += (10 + data_len + 1);
    }
}

static int host_rcv_pkt(uint8_t* data, uint16_t len) {
    fast_parse_and_trigger(data, len);
    return ESP_OK;
}
static void controller_rcv_pkt_ready(void) {}
static esp_vhci_host_callback_t vhci_host_cb = {controller_rcv_pkt_ready, host_rcv_pkt};
static void trigger_ack_task(uint8_t my_id, uint8_t cmd_id, uint8_t cmd_type, uint32_t delay_val, uint8_t state) {
    ack_task_params_t *params = (ack_task_params_t *)malloc(sizeof(ack_task_params_t));
    if (params) {
        params->my_id = my_id;
        params->cmd_id = cmd_id;
        params->cmd_type = cmd_type;
        params->delay_val = delay_val;
        params->state = state;
        xTaskCreate(send_ack_task, "ack_task", 4096, params, 5, NULL);
    }
}
static void IRAM_ATTR timer_timeout_cb(void* arg) {
    action_slot_t* slot = (action_slot_t*)arg;
    uint8_t cmd = slot->ctx.target_cmd;
    uint8_t test_data[3];
    memcpy(test_data, (const uint8_t*)slot->ctx.data, 3);
    ESP_LOGD(TAG,">> [ACTION TRIGGERED] CMD: 0x%02X\n", cmd);
    switch(cmd) {
        case 0x01:
            Player::getInstance().play();
            break;
        case 0x02:
            Player::getInstance().pause();
            break;
        case 0x03:
            Player::getInstance().reset();
            break;
        case 0x04:
            Player::getInstance().release();
            break;
        case 0x05:
            Player::getInstance().load();
            break;
        case 0x06:
            if (test_data[0] == 0 && test_data[1] == 0 && test_data[2] == 0) {
                // Player::getInstance().xxxAPI();
            } else {
                Player::getInstance().test(test_data[0], test_data[1], test_data[2]);
            }
            break;
        // --- Cancel Command ---
        case 0x07:
            esp_timer_stop(s_slots[test_data[0]].timer_handle);
            ESP_LOGD(TAG, "CMD 0x%02X Canceled! CMD_ID = %d", s_slots[test_data[0]].ctx.target_cmd, test_data[0]);
            break;
        case 0x08:{
            int8_t state = Player::getInstance().getState();
            int64_t now = esp_timer_get_time();
            int64_t target_time = s_last_locked_cmd.lock_timestamp + s_last_locked_cmd.original_delay;
            int32_t remaining_us = (int32_t)(target_time - now);
            if (remaining_us < 0) remaining_us = 0;
            // Safety Delay
            vTaskDelay(pdMS_TO_TICKS(150));
            trigger_ack_task(s_config.my_player_id, 
                             s_last_locked_cmd.cmd_id, 
                             s_last_locked_cmd.cmd_type, 
                             (uint32_t)remaining_us,
                             state);
            break;
        }
        default:
            break;
    }
}

static void sync_process_task(void* arg) {
    ble_rx_packet_t pkt;
    int last_processed_id = -1;
    uint8_t current_cmd_id = 0;
    uint8_t current_cmd = 0;
    uint64_t current_mask = 0;
    uint8_t current_data[3] = {0, 0, 0};
    int64_t sum_target = 0;
    int count = 0;
    bool collecting = false;
    bool window_expired = false;
    int64_t window_start_time = 0;

    ESP_LOGI(TAG, "Sync Task Running...");

    while(s_is_running) {
        if(xQueueReceive(s_adv_queue, &pkt, pdMS_TO_TICKS(10)) == pdTRUE) {
            int64_t now = esp_timer_get_time();
            if(pkt.cmd_id == last_processed_id) continue;

            if(!collecting) {
                collecting = true;
                current_cmd_id = pkt.cmd_id;
                current_cmd = pkt.cmd_type;
                current_mask = pkt.target_mask;
                current_data[0] = pkt.data[0];
                current_data[1] = pkt.data[1];
                current_data[2] = pkt.data[2];
                sum_target = 0;
                count = 0;
                window_start_time = now;
                window_expired = false;
            }
            else{
                if (pkt.cmd_id == current_cmd_id) {
                    if(now < (window_start_time + s_config.sync_window_us)) {
                        sum_target += (pkt.rx_time_us + pkt.delay_val);
                        count++;
                    }
                } else {
                    window_expired = true;
                    if(count > 0) {
                        int64_t final_target = sum_target / count;
                        int64_t wait_us = final_target - now;
                        if(wait_us > 500) {
                            action_slot_t* target_slot = &s_slots[current_cmd_id];
                            target_slot->ctx.target_cmd = current_cmd;
                            target_slot->ctx.target_mask = current_mask;
                            memcpy((void*)target_slot->ctx.data, current_data, 3);
                            esp_timer_stop(target_slot->timer_handle);
                            esp_timer_start_once(target_slot->timer_handle, wait_us);
                            last_processed_id = current_cmd_id;
                            if (current_cmd != 0x08) {
                                s_last_locked_cmd.cmd_id = current_cmd_id;
                                s_last_locked_cmd.cmd_type = current_cmd;
                                s_last_locked_cmd.original_delay = (uint32_t)wait_us;
                                s_last_locked_cmd.lock_timestamp = esp_timer_get_time();
                            }
                            ESP_LOGD(TAG, "CMD 0x%02X Locked! Delay: %lld us", current_cmd, wait_us);
                            // trigger_ack_task(s_config.my_player_id, current_cmd_id, current_cmd, (uint32_t)wait_us);
                        }
                    }
                    collecting = true;
                    current_cmd_id = pkt.cmd_id;
                    current_cmd = pkt.cmd_type;
                    current_mask = pkt.target_mask;
                    current_data[0] = pkt.data[0];
                    current_data[1] = pkt.data[1];
                    current_data[2] = pkt.data[2];
                    window_start_time = now;
                    window_expired = false;
                    sum_target = (pkt.rx_time_us + pkt.delay_val);
                    count = 1;
                }
            }
        }
        if(collecting && !window_expired) {
            int64_t now = esp_timer_get_time();
            if(now >= (window_start_time + s_config.sync_window_us)) {
                window_expired = true;
                if(count > 0) {
                     int64_t final_target = sum_target / count;
                     int64_t wait_us = final_target - now;
                     if(wait_us > 500) {
                         action_slot_t* target_slot = &s_slots[current_cmd_id];
                         if(target_slot != NULL) {
                             target_slot->ctx.target_cmd = current_cmd;
                             target_slot->ctx.target_mask = current_mask;
                             memcpy((void*)target_slot->ctx.data, current_data, 3);
                             esp_timer_stop(target_slot->timer_handle);
                             esp_timer_start_once(target_slot->timer_handle, wait_us);
                             last_processed_id = current_cmd_id;
                             
                             ESP_LOGD(TAG, "CMD 0x%02X Locked (Timeout)! Delay: %lld us", current_cmd, wait_us);
                            //  trigger_ack_task(s_config.my_player_id, current_cmd_id, current_cmd, (uint32_t)wait_us);
                         }
                     }
                }
                collecting = false;
            }
        }
    }
    vTaskDelete(NULL);
}

esp_err_t bt_receiver_init(const bt_receiver_config_t* config) {
    if(!config) return ESP_ERR_INVALID_ARG;
    s_config = *config;
    s_adv_queue = xQueueCreate(s_config.queue_size, sizeof(ble_rx_packet_t));
    if(!s_adv_queue) return ESP_ERR_NO_MEM;
    esp_timer_create_args_t timer_args = {
        .callback = &timer_timeout_cb,
        .arg = NULL,
        .dispatch_method = ESP_TIMER_TASK,
        .name = "bt_slot_tmr",
        .skip_unhandled_events = false
    };
    for(int i = 0; i < MAX_CONCURRENT_ACTIONS; i++) {
        timer_args.arg = (void*)&s_slots[i];
        esp_timer_create(&timer_args, &s_slots[i].timer_handle);
    }
    esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
    esp_bt_controller_init(&bt_cfg);
    esp_bt_controller_enable(ESP_BT_MODE_BLE);
    esp_vhci_host_register_callback(&vhci_host_cb);
    return ESP_OK;
}

esp_err_t bt_receiver_start(void) {
    if(s_is_running) return ESP_OK;
    uint16_t sz = make_cmd_reset(hci_cmd_buf);
    esp_vhci_host_send_packet(hci_cmd_buf, sz);
    vTaskDelay(pdMS_TO_TICKS(20));
    uint8_t mask[8] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x20};
    sz = make_cmd_set_evt_mask(hci_cmd_buf, mask);
    esp_vhci_host_send_packet(hci_cmd_buf, sz);
    vTaskDelay(pdMS_TO_TICKS(20));
    sz = make_cmd_ble_set_scan_params(hci_cmd_buf, 0x00, 0x0F, 0x0F, 0x00, 0x00);
    esp_vhci_host_send_packet(hci_cmd_buf, sz);
    vTaskDelay(pdMS_TO_TICKS(20));
    sz = make_cmd_ble_set_scan_enable(hci_cmd_buf, 1, 0);
    esp_vhci_host_send_packet(hci_cmd_buf, sz);
    s_is_running = true;
    xTaskCreatePinnedToCore(sync_process_task, "bt_rx_task", 4096, NULL, 5, &s_task_handle, 1);
    ESP_LOGI(TAG, "Receiver Started");
    return ESP_OK;
}

esp_err_t bt_receiver_stop(void) {
    s_is_running = false;
    uint16_t sz = make_cmd_ble_set_scan_enable(hci_cmd_buf, 0, 0);
    esp_vhci_host_send_packet(hci_cmd_buf, sz);
    for(int i = 0; i < MAX_CONCURRENT_ACTIONS; i++) {
        if(s_slots[i].timer_handle) esp_timer_stop(s_slots[i].timer_handle);
    }
    return ESP_OK;
}