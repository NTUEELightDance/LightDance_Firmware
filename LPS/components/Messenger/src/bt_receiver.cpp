/*
 * bt_receiver.cpp
 * [Type(1)][ID(2)][CMD_ID+CMD(1)][Mask(8)][Delay(4)][Prep(4)][Data(3)] = Total 23 bytes
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

// --- Timer Slots Pool ---
static action_slot_t s_slots[MAX_CONCURRENT_ACTIONS];

// ==========================================
// Part 1: HCI Helper Functions (Private)
// ==========================================

// --- HCI Command Builders ---
static uint16_t make_cmd_reset(uint8_t* buf) {
    UINT8_TO_STREAM(buf, H4_TYPE_COMMAND);
    UINT16_TO_STREAM(buf, HCI_RESET);
    UINT8_TO_STREAM(buf, 0);
    return HCI_H4_CMD_PREAMBLE_SIZE;
}

// --- Set Event Mask to receive LE Meta Events ---
static uint16_t make_cmd_set_evt_mask(uint8_t* buf, uint8_t* evt_mask) {
    UINT8_TO_STREAM(buf, H4_TYPE_COMMAND);
    UINT16_TO_STREAM(buf, HCI_SET_EVT_MASK);
    UINT8_TO_STREAM(buf, HCIC_PARAM_SIZE_SET_EVENT_MASK);
    ARRAY_TO_STREAM(buf, evt_mask, HCIC_PARAM_SIZE_SET_EVENT_MASK);
    return HCI_H4_CMD_PREAMBLE_SIZE + HCIC_PARAM_SIZE_SET_EVENT_MASK;
}

// --- Set BLE Scan Parameters ---
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

// --- Set BLE Scan Enable ---
static uint16_t make_cmd_ble_set_scan_enable(uint8_t* buf, uint8_t scan_enable, uint8_t filter_duplicates) {
    UINT8_TO_STREAM(buf, H4_TYPE_COMMAND);
    UINT16_TO_STREAM(buf, HCI_BLE_WRITE_SCAN_ENABLE);
    UINT8_TO_STREAM(buf, HCIC_PARAM_SIZE_BLE_WRITE_SCAN_ENABLE);
    UINT8_TO_STREAM(buf, scan_enable);
    UINT8_TO_STREAM(buf, filter_duplicates);
    return HCI_H4_CMD_PREAMBLE_SIZE + HCIC_PARAM_SIZE_BLE_WRITE_SCAN_ENABLE;
}

// ==========================================
// Part 2: Fast RX Driver & ISR
// ==========================================

// --- Fast Parse and Trigger Function (Called from ISR) ---
static void IRAM_ATTR fast_parse_and_trigger(uint8_t* data, uint16_t len) {
    int64_t now_us = esp_timer_get_time();

    // --- Check Header: Event(0x04) -> LE Meta(0x3E) -> Adv Report(0x02) ---
    if(data[0] != 0x04 || data[1] != 0x3E || data[3] != 0x02)
        return;

    uint8_t num_reports = data[4];
    uint8_t* payload = &data[5];

    // --- Parse Each Report ---
    for(int i = 0; i < num_reports; i++) {
        uint8_t data_len = payload[8];
        uint8_t* adv_data = &payload[9];
        int8_t rssi = payload[9 + data_len];

        uint8_t offset = 0;
        while(offset < data_len) {
            uint8_t ad_len = adv_data[offset++];
            if(ad_len == 0)
                break;
            uint8_t ad_type = adv_data[offset++];

            // --- Check Manufacturer Specific Data (Type 0xFF) ---
            if(ad_type == 0xFF && ad_len >= 16) {
                uint16_t target_id = s_config.manufacturer_id;

                // --- Check Target ID ---
                if(adv_data[offset] == (target_id & 0xFF) && adv_data[offset + 1] == ((target_id >> 8) & 0xFF)) {

                    // --- Extract Target Mask (Offset 3~10) ---
                    uint64_t rcv_mask = 0;
                    for(int k = 0; k < 8; k++) {
                        rcv_mask |= ((uint64_t)adv_data[offset + 3 + k] << (k * 8));
                    }

                    // --- Check if this receiver is a target ---
                    bool is_target = true;
                    if(s_config.my_player_id >= 0) {
                        // --- Check Target Mask against my_player_id ---
                        if(!((rcv_mask >> s_config.my_player_id) & 1ULL)) {
                            is_target = false;
                        }
                    }

                    if(is_target) {
                        // Hardware Trigger (GPIO Debug)
                        // if (s_config.feedback_gpio_num >= 0) {
                        //     gpio_set_level((gpio_num_t)s_config.feedback_gpio_num, 1);
                        //     esp_rom_delay_us(1);
                        //     gpio_set_level((gpio_num_t)s_config.feedback_gpio_num, 0);
                        // }

                        // --- Parse Command ID and Command (Offset 2) ---
                        uint8_t rcv_cmd_id = (adv_data[offset + 2] >> 4) & 0x0F;
                        uint8_t rcv_cmd = adv_data[offset + 2] & 0x0F;

                        // --- Parse Delay (Offset 11 ~ 14) - Big Endian ---
                        uint32_t rcv_delay =
                            (adv_data[offset + 11] << 24) | (adv_data[offset + 12] << 16) | (adv_data[offset + 13] << 8) | (adv_data[offset + 14]);

                        // --- Parse Prep Time (Offset 15 ~ 18) - Big Endian ---
                        uint32_t rcv_prep_time =
                            (adv_data[offset + 15] << 24) | (adv_data[offset + 16] << 16) | (adv_data[offset + 17] << 8) | (adv_data[offset + 18]);

                        // --- Parse Data (Offset 19 ~ 21) ---
                        uint8_t rcv_data[3] = {adv_data[offset + 19], adv_data[offset + 20], adv_data[offset + 21]};

                        // --- Prepare Packet ---
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

                        // --- Send to Queue (From ISR) ---
                        BaseType_t xHigherPriorityTaskWoken = pdFALSE;
                        xQueueSendFromISR(s_adv_queue, &pkt, &xHigherPriorityTaskWoken);

                        if(xHigherPriorityTaskWoken) {
                            portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
                        }

                        return;
                    }
                }
            }
            offset += (ad_len - 1);
        }
        payload += (10 + data_len + 1);
    }
}

// --- VHCI Callbacks ---
static int host_rcv_pkt(uint8_t* data, uint16_t len) {
    fast_parse_and_trigger(data, len);
    return ESP_OK;
}
static void controller_rcv_pkt_ready(void) {}
static esp_vhci_host_callback_t vhci_host_cb = {controller_rcv_pkt_ready, host_rcv_pkt};

// ==========================================
// Part 3: Sync Logic & Timer
// ==========================================

// --- Timer Timeout Callback ---
static void IRAM_ATTR timer_timeout_cb(void* arg) {
    action_slot_t* slot = (action_slot_t*)arg;

    // --- Call Player API based on slot context ---
    uint8_t cmd = slot->ctx.target_cmd;
    uint8_t test_data[3];
    memcpy(test_data, (const uint8_t*)slot->ctx.data, 3);

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
            Player::getInstance().test(test_data[0], test_data[1], test_data[2]);
            break;
        // --- Cancel Command ---
        case 0x07: {
            esp_timer_stop(s_slots[test_data[0]].timer_handle);
            ESP_LOGI(TAG, "CMD 0x%02X Canceled! CMD_ID = %d", s_slots[test_data[0]].ctx.target_cmd, test_data[0]);
            break;
        }
        default:
            break;
    }
}

static void sync_process_task(void* arg) {
    // --- Variables for Processing ---
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
        // --- Receive Packets ---
        if(xQueueReceive(s_adv_queue, &pkt, pdMS_TO_TICKS(10)) == pdTRUE) {
            int64_t now = esp_timer_get_time();
            if(pkt.cmd_id == last_processed_id) {
                continue;
            }
            // --- New Command ID, Reset Collection ---
            if(!collecting || pkt.cmd_id != current_cmd_id) {
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

            // --- Collect Samples within Sync Window ---
            if(collecting && !window_expired) {
                if(now < (window_start_time + s_config.sync_window_us)) {
                    sum_target += (pkt.rx_time_us + pkt.delay_val);
                    count++;
                } else {
                    window_expired = true;
                    if(count > 0) {
                        int64_t final_target = sum_target / count;
                        int64_t wait_us = final_target - now;

                        // --- Assign to Timer Slot ---
                        if(wait_us > 500) {
                            action_slot_t* target_slot = &s_slots[current_cmd_id];
                            target_slot->ctx.target_cmd = current_cmd;
                            target_slot->ctx.target_mask = current_mask;
                            memcpy((void*)target_slot->ctx.data, current_data, 3);

                            esp_timer_stop(target_slot->timer_handle);
                            esp_timer_start_once(target_slot->timer_handle, wait_us);
                            last_processed_id = current_cmd_id;

                            ESP_LOGI(
                                TAG, "CMD 0x%02X Locked! Slot %d Used. Delay: %lld us (samples: %d)", current_cmd, current_cmd_id, wait_us, count);
                        } else {
                            ESP_LOGW(TAG, "CMD 0x%02X Missed! Latency too high.", current_cmd);
                        }
                    }

                    collecting = false;
                }
            }
        }

        // --- Check Sync Window Expiry ---
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
                            // --- Assign Context ---
                            target_slot->ctx.target_cmd = current_cmd;
                            target_slot->ctx.target_mask = current_mask;
                            target_slot->ctx.data[0] = current_data[0];
                            target_slot->ctx.data[1] = current_data[1];
                            target_slot->ctx.data[2] = current_data[2];
                            // --- Start Timer ---
                            esp_timer_stop(target_slot->timer_handle);
                            esp_timer_start_once(target_slot->timer_handle, wait_us);
                            last_processed_id = current_cmd_id;

                            ESP_LOGI(TAG, "CMD 0x%02X Locked! Slot %d Used. Delay: %lld us", current_cmd, current_cmd_id, wait_us);
                        } else {
                            ESP_LOGE(TAG, "No free slots for CMD 0x%02X!", current_cmd);
                        }

                    } else {
                        ESP_LOGW(TAG, "CMD 0x%02X Missed! Latency too high.", current_cmd);
                    }
                }
                collecting = false;
            }
        }
    }
    vTaskDelete(NULL);
}

// ==========================================
// Part 4: Public APIs
// ==========================================

// --- Initialize BT Receiver ---
esp_err_t bt_receiver_init(const bt_receiver_config_t* config) {
    if(!config)
        return ESP_ERR_INVALID_ARG;
    s_config = *config;

    // 1. Create Queue
    s_adv_queue = xQueueCreate(s_config.queue_size, sizeof(ble_rx_packet_t));
    if(!s_adv_queue)
        return ESP_ERR_NO_MEM;

    // 2. Setup GPIO (if used)
    // if (s_config.feedback_gpio_num >= 0) {
    //     gpio_config_t io_conf = {
    //         .pin_bit_mask = (1ULL << s_config.feedback_gpio_num),
    //         .mode = GPIO_MODE_OUTPUT,
    //         .pull_down_en = 0, .pull_up_en = 0, .intr_type = GPIO_INTR_DISABLE
    //     };
    //     gpio_config(&io_conf);
    //     gpio_set_level(s_config.feedback_gpio_num, 0);
    // }

    // 2. Create Timers
    esp_timer_create_args_t timer_args = {.callback = &timer_timeout_cb, .name = "bt_slot_tmr"};

    for(int i = 0; i < MAX_CONCURRENT_ACTIONS; i++) {
        timer_args.arg = (void*)&s_slots[i];
        esp_timer_create(&timer_args, &s_slots[i].timer_handle);
    }

    // 3. Init BT Controller
    esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
    esp_bt_controller_init(&bt_cfg);
    esp_bt_controller_enable(ESP_BT_MODE_BLE);
    esp_vhci_host_register_callback(&vhci_host_cb);

    return ESP_OK;
}

// --- Start BT Receiver ---
esp_err_t bt_receiver_start(void) {
    if(s_is_running)
        return ESP_OK;

    // --- Send HCI Commands ---
    uint16_t sz = make_cmd_reset(hci_cmd_buf);
    esp_vhci_host_send_packet(hci_cmd_buf, sz);
    vTaskDelay(pdMS_TO_TICKS(20));

    // --- Set Event Mask to receive LE Meta Events ---
    uint8_t mask[8] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x20};
    sz = make_cmd_set_evt_mask(hci_cmd_buf, mask);
    esp_vhci_host_send_packet(hci_cmd_buf, sz);
    vTaskDelay(pdMS_TO_TICKS(20));

    // --- Set Scan Parameters ---
    sz = make_cmd_ble_set_scan_params(hci_cmd_buf, 0x00, 0x0F, 0x0F, 0x00, 0x00);
    esp_vhci_host_send_packet(hci_cmd_buf, sz);
    vTaskDelay(pdMS_TO_TICKS(20));

    // --- Enable Scanning ---
    sz = make_cmd_ble_set_scan_enable(hci_cmd_buf, 1, 0);
    esp_vhci_host_send_packet(hci_cmd_buf, sz);

    // --- Start Sync Task ---
    s_is_running = true;
    xTaskCreatePinnedToCore(sync_process_task, "bt_rx_task", 4096, NULL, 5, &s_task_handle, 1);

    ESP_LOGI(TAG, "Receiver Started");
    return ESP_OK;
}

// --- Stop BT Receiver ---
esp_err_t bt_receiver_stop(void) {
    s_is_running = false;

    // Stop Scanning
    uint16_t sz = make_cmd_ble_set_scan_enable(hci_cmd_buf, 0, 0);
    esp_vhci_host_send_packet(hci_cmd_buf, sz);

    // Stop All Timers
    for(int i = 0; i < MAX_CONCURRENT_ACTIONS; i++) {
        if(s_slots[i].timer_handle) {
            esp_timer_stop(s_slots[i].timer_handle);
        }
    }

    return ESP_OK;
}