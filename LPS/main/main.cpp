#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"

#include "bt_receiver.h"
#include "esp_err.h"
#include "ld_board.h"
#include "ld_config.h"
#include "ld_gamma_lut.h"
#include "ld_nvs.h"

#include "esp_system.h"
#include "player.hpp"
#include "readframe.h"
#include "sd_logger.h"
#include "sd_utils.h"
#include "tcp_client.h"

#include <stdio.h>
#include <string.h>

static const char* TAG = "APP";
static const char* SYS_CMD_TAG = "SYS_CMD";

static constexpr UBaseType_t SYS_CMD_QUEUE_LENGTH = 10;
static constexpr uint32_t STARTUP_INDICATOR_MS = 500;

// Shared by the Bluetooth scheduler and file downloader.
QueueHandle_t sys_cmd_queue = NULL;

static void print_restart_reason() {
    esp_reset_reason_t reason = esp_reset_reason();
    const char* reason_name = "UNKNOWN";
    const char* reason_desc = "Reset reason is not recognized.";
    bool is_error = false;
    bool is_warning = false;

    switch(reason) {
        case ESP_RST_UNKNOWN:
            reason_name = "UNKNOWN";
            reason_desc = "Reset reason could not be determined.";
            is_warning = true;
            break;
        case ESP_RST_POWERON:
            reason_name = "POWERON";
            reason_desc = "Normal power-on reset.";
            break;
        case ESP_RST_EXT:
            reason_name = "EXT";
            reason_desc = "External reset signal triggered reboot.";
            break;
        case ESP_RST_SW:
            reason_name = "SW";
            reason_desc = "Software requested a restart.";
            break;
        case ESP_RST_PANIC:
            reason_name = "PANIC";
            reason_desc = "System rebooted after a fatal exception.";
            is_error = true;
            break;
        case ESP_RST_INT_WDT:
            reason_name = "INT_WDT";
            reason_desc = "Interrupt watchdog timeout.";
            is_error = true;
            break;
        case ESP_RST_TASK_WDT:
            reason_name = "TASK_WDT";
            reason_desc = "Task watchdog timeout.";
            is_error = true;
            break;
        case ESP_RST_WDT:
            reason_name = "WDT";
            reason_desc = "Other watchdog triggered a reset.";
            is_error = true;
            break;
        case ESP_RST_DEEPSLEEP:
            reason_name = "DEEPSLEEP";
            reason_desc = "Wake-up from deep sleep.";
            break;
        case ESP_RST_BROWNOUT:
            reason_name = "BROWNOUT";
            reason_desc = "Power supply voltage dropped too low.";
            is_error = true;
            break;
        case ESP_RST_SDIO:
            reason_name = "SDIO";
            reason_desc = "Reset triggered by SDIO subsystem.";
            is_warning = true;
            break;
        case ESP_RST_USB:
            reason_name = "USB";
            reason_desc = "Reset triggered by USB subsystem.";
            break;
        case ESP_RST_JTAG:
            reason_name = "JTAG";
            reason_desc = "Reset triggered via JTAG.";
            break;
        case ESP_RST_EFUSE:
            reason_name = "EFUSE";
            reason_desc = "eFuse related reset.";
            is_error = true;
            break;
        case ESP_RST_PWR_GLITCH:
            reason_name = "PWR_GLITCH";
            reason_desc = "Power glitch detected.";
            is_error = true;
            break;
        case ESP_RST_CPU_LOCKUP:
            reason_name = "CPU_LOCKUP";
            reason_desc = "CPU lockup detected.";
            is_error = true;
            break;
        default:
            is_warning = true;
            break;
    }

    if(is_error) {
        ESP_LOGE(TAG, "restart reason=%s (%d): %s", reason_name, reason, reason_desc);
    } else if(is_warning) {
        ESP_LOGW(TAG, "restart reason=%s (%d): %s", reason_name, reason, reason_desc);
    } else {
        ESP_LOGI(TAG, "restart reason=%s (%d): %s", reason_name, reason, reason_desc);
    }
}

/* * Background task to handle system-level commands asynchronously.
 * Receives messages from BLE receiver or TCP client.
 */
static void sys_cmd_task(void* arg) {
    (void)arg;
    sys_cmd_t msg;

    ESP_LOGI(SYS_CMD_TAG, "system command task started");

    while(1) {
        if(xQueueReceive(sys_cmd_queue, &msg, portMAX_DELAY) == pdTRUE) {
            switch(msg) {
                case UPLOAD:
                    ESP_LOGI(SYS_CMD_TAG, "command received: UPLOAD; entering update mode");
                    // Stop playback and turn LEDs green to indicate update mode
                    if(Player::getInstance().getState() != 1)
                        Player::getInstance().stop();
                    Player::getInstance().test(0, 128, 0);

                    // Trigger the background TCP OTA update task
                    tcp_client_start_update_task();
                    break;

                case RESET:
                    ESP_LOGI(SYS_CMD_TAG, "command received: RESET; rebooting in 1000 ms");
                    vTaskDelay(pdMS_TO_TICKS(1000));
                    ESP_LOGI(SYS_CMD_TAG, "flushing persistent log before restart");
                    vTaskDelay(pdMS_TO_TICKS(100));  // Give time for log to be written to buffer
#if LD_CFG_ENABLE_LOGGER
                    sd_log_flush();
#endif
                    esp_restart();
                    break;

                case UPLOAD_SUCCESS:
                    ESP_LOGI(SYS_CMD_TAG, "update task reported completion; scheduling restart");
                    Player::getInstance().stop();  // Turn off LEDs before reboot
                    vTaskDelay(pdMS_TO_TICKS(1000));
                    // Reset after successful upload to apply new content
                    {
                        sys_cmd_t reset_cmd = RESET;
                        if(xQueueSend(sys_cmd_queue, &reset_cmd, 0) != pdTRUE) {
                            ESP_LOGE(SYS_CMD_TAG, "restart command enqueue failed after update completion");
                        }
                    }

                    break;

                default:
                    ESP_LOGW(SYS_CMD_TAG, "ignoring unknown command: value=%d", (int)msg);
                    break;
            }
        }
    }
}

static void configure_default_channels(void) {
    memset(&ch_info, 0, sizeof(ch_info));

    for(int i = 0; i < LD_BOARD_WS2812B_NUM; i++) {
        ch_info.rmt_strips[i] = LD_BOARD_WS2812B_MAX_PIXEL_NUM;
    }
    for(int i = 0; i < LD_BOARD_PCA9955B_CH_NUM; i++) {
        ch_info.i2c_leds[i] = 1;
    }

    ESP_LOGD(TAG, "default channel configuration applied: ws2812b_strips=%d pixels_per_strip=%d pca9955b_channels=%d", LD_BOARD_WS2812B_NUM, LD_BOARD_WS2812B_MAX_PIXEL_NUM, LD_BOARD_PCA9955B_CH_NUM);
}

static esp_err_t init_system_command_dispatcher(void) {
    if(sys_cmd_queue != NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    sys_cmd_queue = xQueueCreate(SYS_CMD_QUEUE_LENGTH, sizeof(sys_cmd_t));
    if(sys_cmd_queue == NULL) {
        ESP_LOGE(TAG, "system command queue creation failed: length=%u", (unsigned)SYS_CMD_QUEUE_LENGTH);
        return ESP_ERR_NO_MEM;
    }

    if(xTaskCreate(sys_cmd_task, "sys_cmd_task", 4096, NULL, 5, NULL) != pdPASS) {
        ESP_LOGE(TAG, "system command task creation failed: stack_size=4096 priority=5");
        vQueueDelete(sys_cmd_queue);
        sys_cmd_queue = NULL;
        return ESP_ERR_NO_MEM;
    }

    return ESP_OK;
}

#if LD_CFG_ENABLE_BT
static int load_player_id(bool nvs_ready) {
    if(!nvs_ready) {
        ESP_LOGW(TAG, "player ID unavailable because NVS is not ready; using broadcast ID: fallback=0");
        return 0;
    }

    uint8_t stored_player_id = 0;
    esp_err_t err = ld_nvs_get_player_id(&stored_player_id);
    if(err != ESP_OK) {
        ESP_LOGW(TAG, "player ID could not be loaded; using broadcast ID: fallback=0 err=%s", esp_err_to_name(err));
        return 0;
    }

    int player_id = stored_player_id;
    if(player_id < 1 || player_id > 31) {
        ESP_LOGW(TAG, "stored player ID is invalid; using broadcast ID: stored=%d valid_range=1..31 fallback=0", player_id);
        return 0;
    }

    ESP_LOGI(TAG, "player ID loaded: id=%d", player_id);
    return player_id;
}

static void log_player_id_banner(int player_id) {
    const char* mode = player_id == 0 ? "BROADCAST" : "DEDICATED";

    ESP_LOGI(TAG, "================================================");
    ESP_LOGI(TAG, "              PLAYER ID: %02d (%s)", player_id, mode);
    ESP_LOGI(TAG, "================================================");
}

static esp_err_t init_bluetooth_receiver(int player_id) {
    const bt_receiver_config_t rx_cfg = {
        .feedback_gpio_num = -1,
        .manufacturer_id = 0xFFFF,
        .my_player_id = player_id,
        .sync_window_us = 500000,
        .queue_size = 20,
    };

    esp_err_t err = bt_receiver_init(&rx_cfg);
    if(err != ESP_OK) {
        return err;
    }

    return bt_receiver_start();
}
#endif

/* * Main application initialization task.
 * Sets up file systems, hardware configs, player, and communication modules.
 */
static void app_task(void* arg) {
    (void)arg;

    bool storage_ready = false;
    bool logger_ready = false;
    bool nvs_ready = false;
    bool frame_ready = false;
    bool player_ready = false;
    bool command_dispatcher_ready = false;
    bool bluetooth_ready = false;

    ESP_LOGI(TAG, "application initialization started");
    ESP_LOGD(TAG, "app task stack high-water mark=%u words", uxTaskGetStackHighWaterMark(NULL));

    // 1. Bring up persistent storage before its consumers (logger and frame reader).
    esp_err_t err = mount_spiffs();
    if(err == ESP_OK) {
        storage_ready = true;
        ESP_LOGI(TAG, "persistent storage ready");
    } else {
        ESP_LOGE(TAG, "persistent storage initialization failed: %s", esp_err_to_name(err));
    }

#if LD_CFG_ENABLE_LOGGER
    // 2. Install the file logger before emitting boot diagnostics.
    if(storage_ready) {
        err = sd_log_init();
        if(err == ESP_OK) {
            logger_ready = true;
            ESP_LOGI(TAG, "persistent logging enabled");
        } else {
            ESP_LOGW(TAG, "persistent logging unavailable; continuing with console log: %s", esp_err_to_name(err));
        }
    } else {
        ESP_LOGW(TAG, "persistent logging skipped because storage is unavailable");
    }
#endif

    // 3. Record the previous reset after persistent logging is available.
    print_restart_reason();

    // 4. NVS must be ready before the Bluetooth controller and player identity.
    err = ld_nvs_init();
    nvs_ready = err == ESP_OK;
    if(!nvs_ready) {
        ESP_LOGE(TAG, "NVS initialization failed: %s", esp_err_to_name(err));
    }
#if LD_CFG_ENABLE_BT
    const int player_id = load_player_id(nvs_ready);
    log_player_id_banner(player_id);
#endif

    // 5. Prepare color conversion before the Player can render a frame.
    calc_gamma_lut();
    ESP_LOGD(TAG, "gamma lookup table ready");

    // 6. Load runtime channel topology and frame data before LED/Player init.
#if LD_CFG_ENABLE_PT
    if(storage_ready) {
        err = frame_system_init("/spiffs/control.dat", "/spiffs/frame.dat");
        frame_ready = err == ESP_OK;
        if(frame_ready) {
            ESP_LOGI(TAG, "frame system ready");
        } else {
            ESP_LOGE(TAG, "frame system initialization failed: %s", esp_err_to_name(err));
        }
    } else {
        ESP_LOGE(TAG, "frame system initialization skipped because storage is unavailable");
    }

    if(!frame_ready) {
        configure_default_channels();
        ESP_LOGW(TAG, "default channel configuration enabled for diagnostics; pattern playback is unavailable");
    }
#else
    configure_default_channels();
    ESP_LOGI(TAG, "pattern reader disabled; using default channel configuration");
#endif

    ESP_LOGD(TAG, "stack high-water mark after frame initialization=%u words", uxTaskGetStackHighWaterMark(NULL));

    // 7. Start the core runtime before any external command producer.
    err = Player::getInstance().init();
    player_ready = err == ESP_OK;
    if(!player_ready) {
        ESP_LOGE(TAG, "player initialization failed: %s", esp_err_to_name(err));
    } else {
        ESP_LOGI(TAG, "player initialized");
    }

    // 8. Create the command consumer before Bluetooth can enqueue commands.
    err = init_system_command_dispatcher();
    command_dispatcher_ready = err == ESP_OK;
    if(command_dispatcher_ready) {
        ESP_LOGI(TAG, "system command dispatcher ready");
    }

#if LD_CFG_ENABLE_BT
    // 9. Bluetooth starts last among command sources.
    err = init_bluetooth_receiver(player_id);
    bluetooth_ready = err == ESP_OK;
    if(!bluetooth_ready) {
        ESP_LOGE(TAG, "Bluetooth receiver start failed: %s", esp_err_to_name(err));
    } else {
        ESP_LOGI(TAG, "Bluetooth receiver ready");
    }
#endif

    // 10. Start the local diagnostic command source after the Player is ready.
    console_test();

    // 11. Use a short blue pulse to indicate successful Player startup.
    if(player_ready) {
        err = Player::getInstance().test(0, 0, 128);
        if(err == ESP_OK) {
            vTaskDelay(pdMS_TO_TICKS(STARTUP_INDICATOR_MS));
            err = Player::getInstance().stop();
        }
        if(err != ESP_OK) {
            ESP_LOGW(TAG, "startup indicator could not be completed: %s", esp_err_to_name(err));
        }
    }

    ESP_LOGI(TAG,
             "application initialization completed: storage=%s logger=%s nvs=%s frames=%s player=%s commands=%s bluetooth=%s",
             storage_ready ? "ready" : "failed",
             LD_CFG_ENABLE_LOGGER ? (logger_ready ? "ready" : "failed") : "disabled",
             nvs_ready ? "ready" : "failed",
             LD_CFG_ENABLE_PT ? (frame_ready ? "ready" : "fallback") : "disabled",
             player_ready ? "ready" : "failed",
             command_dispatcher_ready ? "ready" : "failed",
             LD_CFG_ENABLE_BT ? (bluetooth_ready ? "ready" : "failed") : "disabled");
    vTaskDelete(NULL);
}

/* ESP-IDF Entry Point */
extern "C" void app_main(void) {
    if(xTaskCreate(app_task, "app_task", 16384, NULL, 5, NULL) != pdPASS) {
        ESP_LOGE(TAG, "application task creation failed: stack_size=16384 priority=5");
    }
}
