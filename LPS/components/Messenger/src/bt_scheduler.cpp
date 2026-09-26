#include "bt_scheduler.hpp"

#include <string.h>

#include "bt_ack.hpp"
#include "bt_protocol.hpp"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "player.hpp"

static const char* TAG = "BT_SCHED";

static bt_receiver_config_t s_config;
static QueueHandle_t s_adv_queue = NULL;
static TaskHandle_t s_task_handle = NULL;
static bool s_is_running = false;

typedef struct {
    volatile uint8_t target_cmd;
    volatile uint64_t target_mask;
    volatile uint8_t data[3];
    volatile uint64_t target_time_us;
} bt_action_context_t;

#define MAX_CONCURRENT_ACTIONS 16

typedef struct {
    esp_timer_handle_t timer_handle;
    bt_action_context_t ctx;
} action_slot_t;

static action_slot_t s_slots[MAX_CONCURRENT_ACTIONS];
static bool s_visual_ack_done[MAX_CONCURRENT_ACTIONS] = {false};
static esp_timer_handle_t s_led_timer = NULL;

static struct {
    uint8_t cmd_id;
    uint8_t cmd_type;
    uint32_t original_delay;
    int64_t lock_timestamp;
} s_last_locked_cmd = {0, 0, 0, 0};

static int host_rcv_pkt(uint8_t* data, uint16_t len) {
    ble_rx_packet_t pkt;
    if(bt_protocol_parse_hci_event(data, len, s_config.my_player_id, &pkt)) {
        BaseType_t xHigherPriorityTaskWoken = pdFALSE;
        xQueueSendFromISR(s_adv_queue, &pkt, &xHigherPriorityTaskWoken);
        if(xHigherPriorityTaskWoken)
            portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
    }
    return ESP_OK;
}

static void controller_rcv_pkt_ready(void) {}

static esp_vhci_host_callback_t vhci_host_cb = {controller_rcv_pkt_ready, host_rcv_pkt};

static void IRAM_ATTR led_timer_cb(void* arg) {
    if(Player::getInstance().getState() == 4) {
        Player::getInstance().stop();
    }
}

static void IRAM_ATTR timer_timeout_cb(void* arg) {
    action_slot_t* slot = (action_slot_t*)arg;
    uint8_t cmd = slot->ctx.target_cmd;
    uint8_t test_data[3];
    memcpy(test_data, (const uint8_t*)slot->ctx.data, 3);

    int slot_id = slot - s_slots;
    if(slot_id >= 0 && slot_id < MAX_CONCURRENT_ACTIONS) {
        s_visual_ack_done[slot_id] = false;
    }
    ESP_LOGD(TAG, "scheduled command triggered: slot=%d command=0x%02x", slot_id, cmd);

    switch(cmd) {
        case LPS_CMD_PLAY:
            Player::getInstance().play();
            break;
        case LPS_CMD_PAUSE:
            Player::getInstance().pause();
            break;
        case LPS_CMD_STOP:
            Player::getInstance().stop();
            break;
        case LPS_CMD_RELEASE:
            Player::getInstance().release();
            break;
        case LPS_CMD_TEST:
            if(test_data[0] == 0 && test_data[1] == 0 && test_data[2] == 0) {
                Player::getInstance().test();
            } else {
                Player::getInstance().test(test_data[0], test_data[1], test_data[2]);
            }
            break;
        case LPS_CMD_CANCEL: {
            uint8_t target_id = test_data[0];
            if(target_id < MAX_CONCURRENT_ACTIONS) {
                esp_timer_stop(s_slots[target_id].timer_handle);
                s_visual_ack_done[target_id] = false;

                if(s_slots[target_id].ctx.target_cmd == 0x01) {
                    esp_timer_stop(s_led_timer);
                    Player::getInstance().stop();
                }
                ESP_LOGD(TAG, "scheduled command canceled: slot=%u command=0x%02x", target_id, s_slots[target_id].ctx.target_cmd);
            }
        } break;
        case LPS_CMD_CHECK: {
            int8_t state = Player::getInstance().getState();
            int64_t now = esp_timer_get_time();
            int64_t target_time = s_last_locked_cmd.lock_timestamp + s_last_locked_cmd.original_delay;
            int32_t remaining_us = (int32_t)(target_time - now);
            if(remaining_us < 0)
                remaining_us = 0;

            bt_ack_trigger(s_config.my_player_id, s_last_locked_cmd.cmd_id, s_last_locked_cmd.cmd_type, (uint32_t)remaining_us, state);
            break;
        }
        case LPS_CMD_UPLOAD:
        case LPS_CMD_RESET:
            if(sys_cmd_queue != NULL) {
                sys_cmd_t msg = (sys_cmd_t)cmd;
                BaseType_t ret = xQueueSend(sys_cmd_queue, &msg, 0);
                if(ret == pdPASS) {
                    ESP_LOGD(TAG, "system command queued: command=0x%02x", cmd);
                } else {
                    ESP_LOGE(TAG, "system command enqueue failed: command=0x%02x reason=queue_full", cmd);
                }
            } else {
                ESP_LOGE(TAG, "system command enqueue failed: command=0x%02x reason=queue_not_initialized", cmd);
            }
            break;
        case LPS_CMD_SEEK:
            Player::getInstance().seek(slot->ctx.target_time_us);
            break;
        default:
            break;
    }
}

static void sync_process_task(void* arg) {
    ble_rx_packet_t pkt;
    uint8_t current_cmd_id = 0;
    uint8_t current_cmd = 0;
    uint64_t current_mask = 0;
    uint8_t current_data[3] = {0, 0, 0};
    uint32_t current_prep_time = 0;
    uint64_t current_target_time_us = 0;
    int64_t sum_rssi = 0;
    int64_t sum_target = 0;
    int count = 0;
    bool collecting = false;
    bool window_expired = false;
    int64_t window_start_time = 0;
    uint8_t current_mac[6] = {0};
    ESP_LOGI(TAG, "Bluetooth synchronization task started: window_us=%lu", (unsigned long)s_config.sync_window_us);

    while(s_is_running) {
        if(xQueueReceive(s_adv_queue, &pkt, pdMS_TO_TICKS(10)) == pdTRUE) {
            int64_t now = esp_timer_get_time();
            if(!collecting) {
                collecting = true;
                current_cmd_id = pkt.cmd_id;
                current_cmd = pkt.cmd_type;
                current_mask = pkt.target_mask;
                current_data[0] = pkt.data[0];
                current_data[1] = pkt.data[1];
                current_data[2] = pkt.data[2];
                current_prep_time = pkt.prep_time;
                current_target_time_us = pkt.target_time;
                sum_rssi = pkt.rssi;
                sum_target = (pkt.rx_time_us + pkt.delay_val);
                count = 1;
                window_start_time = now;
                window_expired = false;
                memcpy(current_mac, pkt.mac, 6);
            } else {
                if(pkt.cmd_id == current_cmd_id) {
                    if(now < (window_start_time + s_config.sync_window_us)) {
                        sum_target += (pkt.rx_time_us + pkt.delay_val);
                        sum_rssi += pkt.rssi;
                        count++;
                    }
                } else {
                    window_expired = true;
                    if(count > 0) {
                        int64_t final_target = sum_target / count;
                        int64_t wait_us = final_target - now;
                        int8_t avg_rssi = (int8_t)(sum_rssi / count);

                        if(wait_us > 100000) {
                            action_slot_t* target_slot = &s_slots[current_cmd_id];
                            target_slot->ctx.target_cmd = current_cmd;
                            target_slot->ctx.target_mask = current_mask;
                            target_slot->ctx.target_time_us = current_target_time_us;
                            memcpy((void*)target_slot->ctx.data, current_data, 3);

                            esp_timer_stop(target_slot->timer_handle);
                            esp_timer_start_once(target_slot->timer_handle, wait_us);

                            if(!s_visual_ack_done[current_cmd_id]) {
                                ESP_LOGI(TAG,
                                         "command synchronized: source=%02X:%02X:%02X:%02X:%02X:%02X slot=%u command=0x%02x avg_rssi_dbm=%d samples=%d execute_in_ms=%lld",
                                         current_mac[5],
                                         current_mac[4],
                                         current_mac[3],
                                         current_mac[2],
                                         current_mac[1],
                                         current_mac[0],
                                         current_cmd_id,
                                         current_cmd,
                                         avg_rssi,
                                         count,
                                         wait_us / 1000);
                                if(current_cmd == 0x01 && current_prep_time > 0) {
                                    Player::getInstance().test(255, 0, 0);
                                    esp_timer_stop(s_led_timer);
                                    esp_timer_start_once(s_led_timer, current_prep_time);
                                }
                                s_visual_ack_done[current_cmd_id] = true;
                            }

                            if(current_cmd != 0x07) {
                                s_last_locked_cmd.cmd_id = current_cmd_id;
                                s_last_locked_cmd.cmd_type = current_cmd;
                                s_last_locked_cmd.original_delay = (uint32_t)wait_us;
                                s_last_locked_cmd.lock_timestamp = esp_timer_get_time();
                            }
                        }
                    }
                    collecting = true;
                    current_cmd_id = pkt.cmd_id;
                    current_cmd = pkt.cmd_type;
                    current_mask = pkt.target_mask;
                    current_data[0] = pkt.data[0];
                    current_data[1] = pkt.data[1];
                    current_data[2] = pkt.data[2];
                    current_prep_time = pkt.prep_time;
                    current_target_time_us = pkt.target_time;
                    sum_rssi = pkt.rssi;
                    window_start_time = now;
                    window_expired = false;
                    memcpy(current_mac, pkt.mac, 6);
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
                    int8_t avg_rssi = (int8_t)(sum_rssi / count);

                    if(wait_us > 100000) {
                        action_slot_t* target_slot = &s_slots[current_cmd_id];
                        if(target_slot != NULL) {
                            target_slot->ctx.target_cmd = current_cmd;
                            target_slot->ctx.target_mask = current_mask;
                            target_slot->ctx.target_time_us = current_target_time_us;
                            memcpy((void*)target_slot->ctx.data, current_data, 3);

                            esp_timer_stop(target_slot->timer_handle);
                            esp_timer_start_once(target_slot->timer_handle, wait_us);

                            if(!s_visual_ack_done[current_cmd_id]) {
                                ESP_LOGI(TAG,
                                         "command synchronized: source=%02X:%02X:%02X:%02X:%02X:%02X slot=%u command=0x%02x avg_rssi_dbm=%d samples=%d execute_in_ms=%lld",
                                         current_mac[5],
                                         current_mac[4],
                                         current_mac[3],
                                         current_mac[2],
                                         current_mac[1],
                                         current_mac[0],
                                         current_cmd_id,
                                         current_cmd,
                                         avg_rssi,
                                         count,
                                         wait_us / 1000);
                                if(current_cmd == 0x01 && current_prep_time > 0) {
                                    Player::getInstance().test(255, 0, 0);
                                    esp_timer_stop(s_led_timer);
                                    esp_timer_start_once(s_led_timer, current_prep_time);
                                }
                                s_visual_ack_done[current_cmd_id] = true;
                            }
                            if(current_cmd != 0x07) {
                                s_last_locked_cmd.cmd_id = current_cmd_id;
                                s_last_locked_cmd.cmd_type = current_cmd;
                                s_last_locked_cmd.original_delay = (uint32_t)wait_us;
                                s_last_locked_cmd.lock_timestamp = esp_timer_get_time();
                            }
                        }
                    }
                }
                collecting = false;
            }
        }
    }
    ESP_LOGI(TAG, "Bluetooth synchronization task stopped");
    vTaskDelete(NULL);
}

esp_err_t bt_scheduler_init(const bt_receiver_config_t* config) {
    if(!config) {
        ESP_LOGE(TAG, "scheduler initialization rejected: configuration pointer is NULL");
        return ESP_ERR_INVALID_ARG;
    }
    s_config = *config;
    s_adv_queue = xQueueCreate(s_config.queue_size, sizeof(ble_rx_packet_t));
    if(!s_adv_queue) {
        ESP_LOGE(TAG, "advertisement queue creation failed: length=%u item_size=%u", s_config.queue_size, (unsigned)sizeof(ble_rx_packet_t));
        return ESP_ERR_NO_MEM;
    }

    esp_timer_create_args_t timer_args = {.callback = &timer_timeout_cb, .arg = NULL, .dispatch_method = ESP_TIMER_TASK, .name = "bt_slot_tmr", .skip_unhandled_events = false};
    for(int i = 0; i < MAX_CONCURRENT_ACTIONS; i++) {
        timer_args.arg = (void*)&s_slots[i];
        esp_err_t err = esp_timer_create(&timer_args, &s_slots[i].timer_handle);
        if(err != ESP_OK) {
            ESP_LOGE(TAG, "command timer creation failed: slot=%d err=%s", i, esp_err_to_name(err));
        }
    }

    esp_timer_create_args_t led_timer_args = {.callback = &led_timer_cb, .arg = NULL, .dispatch_method = ESP_TIMER_TASK, .name = "prep_led_tmr", .skip_unhandled_events = false};
    esp_err_t led_timer_err = esp_timer_create(&led_timer_args, &s_led_timer);
    if(led_timer_err != ESP_OK) {
        ESP_LOGE(TAG, "preparation LED timer creation failed: %s", esp_err_to_name(led_timer_err));
    }
    ESP_LOGI(TAG, "Bluetooth scheduler initialized: player_id=%d queue_length=%u slots=%d", s_config.my_player_id, s_config.queue_size, MAX_CONCURRENT_ACTIONS);
    return ESP_OK;
}

void bt_scheduler_start(void) {
    s_is_running = true;
    if(xTaskCreatePinnedToCore(sync_process_task, "bt_rx_task", 4096, NULL, 5, &s_task_handle, 1) != pdPASS) {
        s_is_running = false;
        ESP_LOGE(TAG, "Bluetooth synchronization task creation failed: stack_size=4096 priority=5 core=1");
    }
}

void bt_scheduler_request_stop(void) {
    s_is_running = false;
}

void bt_scheduler_stop_timers(void) {
    for(int i = 0; i < MAX_CONCURRENT_ACTIONS; i++) {
        if(s_slots[i].timer_handle)
            esp_timer_stop(s_slots[i].timer_handle);
    }
    if(s_led_timer)
        esp_timer_stop(s_led_timer);
}

void bt_scheduler_deinit(void) {
    if(s_task_handle) {
        vTaskDelete(s_task_handle);
        s_task_handle = NULL;
    }

    if(s_adv_queue) {
        vQueueDelete(s_adv_queue);
        s_adv_queue = NULL;
    }

    for(int i = 0; i < MAX_CONCURRENT_ACTIONS; i++) {
        if(s_slots[i].timer_handle) {
            esp_timer_delete(s_slots[i].timer_handle);
            s_slots[i].timer_handle = NULL;
        }
    }
}

bool bt_scheduler_is_running(void) {
    return s_is_running;
}

esp_vhci_host_callback_t* bt_scheduler_vhci_callback(void) {
    return &vhci_host_cb;
}
