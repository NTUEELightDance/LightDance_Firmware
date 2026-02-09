#include "readframe.h"
#include "frame_reader.h"
#include "player_protocal.h"

#include <string.h>
#include "esp_log.h"
#include "esp_err.h"

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "control_reader.h"
#include "board.h"
#include "frame.h"

/* ========================================================= */
ch_info_t ch_info_snapshot;

static const char* TAG = "readframe";

/* ================= runtime state ================= */

static table_frame_t frame_buf;        /* single internal buffer */

static SemaphoreHandle_t sem_free;     /* buffer writable */
static SemaphoreHandle_t sem_ready;    /* buffer readable */

static TaskHandle_t sd_task = NULL;

static bool inited  = false;
static bool running = false;

/* ================= SD task command ================= */

typedef enum {
    CMD_NONE = 0,
    CMD_RESET,
} sd_cmd_t;

static sd_cmd_t cmd = CMD_NONE;

/* ================= SD mount ================= */

#include "driver/sdmmc_host.h"
#include "esp_vfs_fat.h"
#include "sdmmc_cmd.h"

static sdmmc_card_t* g_sd_card = NULL;

static esp_err_t mount_sdcard(void)
{
    esp_vfs_fat_sdmmc_mount_config_t mount_config = {
        .format_if_mount_failed = false,
        .max_files = 5,
        .allocation_unit_size = 16 * 1024,
        .disk_status_check_enable = false,
        .use_one_fat = false,
    };

    sdmmc_host_t host = SDMMC_HOST_DEFAULT();
    host.flags = SDMMC_HOST_FLAG_4BIT;
    host.max_freq_khz = SDMMC_FREQ_HIGHSPEED;

    sdmmc_slot_config_t slot_config = SDMMC_SLOT_CONFIG_DEFAULT();
    slot_config.width = 4;
    slot_config.gpio_cd = GPIO_NUM_NC;
    slot_config.gpio_wp = GPIO_NUM_NC;
    slot_config.flags |= SDMMC_SLOT_FLAG_INTERNAL_PULLUP;

    esp_err_t ret = esp_vfs_fat_sdmmc_mount("/sd",
                                           &host,
                                           &slot_config,
                                           &mount_config,
                                           &g_sd_card);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "SD mount failed (%s)", esp_err_to_name(ret));
        return ret;
    }

    return ESP_OK;
}

/* ================= SD reader task ================= */

static void sd_reader_task(void* arg)
{
    while (running) {

        /* wait until buffer free */
        if (xSemaphoreTake(sem_free, portMAX_DELAY) != pdTRUE)
            continue;

        /* ---- command handling ---- */
        if (cmd == CMD_RESET) {
            frame_reader_reset(); //correction
            
            cmd = CMD_NONE;
            xSemaphoreGive(sem_free);
            continue;
        }

        /* ---- read one frame ---- */
        esp_err_t err = frame_reader_read(&frame_buf);

        if (err == ESP_ERR_NOT_FOUND) {
            ESP_LOGI(TAG, "EOF reached");
            running = false;
            xSemaphoreGive(sem_ready);
            break;
        }

        if (err != ESP_OK) {
            ESP_LOGE(TAG, "frame_reader_read failed: %s",
                     esp_err_to_name(err));
            running = false;
            xSemaphoreGive(sem_ready);
            break;
        }

        /* buffer ready */
        xSemaphoreGive(sem_ready);
    }

    ESP_LOGI(TAG, "sd_reader_task exit");
    vTaskDelete(NULL);
}

/* ================= public API ================= */

/* ---- initial frame system ---- */

esp_err_t frame_system_init(const char *control_path,
                            const char *frame_path)
{
    esp_err_t err;

    if (inited)
        return ESP_ERR_INVALID_STATE;

    /* ---------- 0. mount SD ---------- */
    err = mount_sdcard();
    if (err != ESP_OK)
        return err;

    /* ---------- 1. load control.dat -> ch_info ---------- */
    err = get_channel_info(control_path, &ch_info);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "get_channel_info failed: %s",
                 esp_err_to_name(err));
        return err;
    }
    ch_info_snapshot = ch_info;   // snapshot


    /* ---------- 2. init frame reader ---------- */
    err = frame_reader_init(frame_path);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "frame_reader_init failed: %s",
                 esp_err_to_name(err));
        return err;
    }

    /* ---------- 3. semaphores ---------- */
    sem_free  = xSemaphoreCreateBinary();
    sem_ready = xSemaphoreCreateBinary();

    if (!sem_free || !sem_ready) {
        frame_reader_deinit();
        return ESP_ERR_NO_MEM;
    }

    xSemaphoreGive(sem_free); /* buffer initially free */

    /* ---------- 4. runtime ---------- */
    running = true;
    cmd     = CMD_NONE;

    /* ---------- 5. create SD reader task ---------- */
    xTaskCreate(sd_reader_task,
                "sd_reader",
                16384,
                NULL,
                5,
                &sd_task);

    inited = true;

    ESP_LOGI(TAG, "frame system initialized (new channel_info model)");
    return ESP_OK;
}

/* ---- sequential read ---- */

esp_err_t read_frame(table_frame_t* playerbuffer)
{
    if (!inited)
        return ESP_ERR_INVALID_STATE;
    if (!playerbuffer)
        return ESP_ERR_INVALID_ARG;

    if (xSemaphoreTake(sem_ready, portMAX_DELAY) != pdTRUE)
        return ESP_FAIL;

    if (!running)
        return ESP_ERR_NOT_FOUND;

    memcpy(playerbuffer, &frame_buf, sizeof(table_frame_t));

    xSemaphoreGive(sem_free);
    return ESP_OK;
}

/* ---- reset to frame 0 ---- */

esp_err_t frame_reset(void)
{
    if (!inited)
        return ESP_ERR_INVALID_STATE;

    /* drain ready semaphore */
    while (xSemaphoreTake(sem_ready, 0) == pdTRUE) {}

    cmd = CMD_RESET;
    xSemaphoreGive(sem_free);
    return ESP_OK;
}

/* ---- deinit frame system ---- */

void frame_system_deinit(void)
{
    if (!inited)
        return;

    running = false;

    if (sd_task) {
        xSemaphoreGive(sem_free);
        vTaskDelay(pdMS_TO_TICKS(10));
    }

    if (sem_free)
        vSemaphoreDelete(sem_free);
    if (sem_ready)
        vSemaphoreDelete(sem_ready);

    frame_reader_deinit();

    sem_free = sem_ready = NULL;
    sd_task  = NULL;
    inited   = false;

    ESP_LOGI(TAG, "frame system deinit");
}

/* ---- get sd card id ---- */

const char* get_sd_card_id(void) {
    if (g_sd_card == NULL) {
        return "0"; // no card
    }
    static char card_id[33];  // 32 chars + null terminator
    
    snprintf(card_id, sizeof(card_id), "%02X%04X%08X",
             g_sd_card->cid.mfg_id,
             g_sd_card->cid.oem_id,
             g_sd_card->cid.serial);
    
    return card_id;
}