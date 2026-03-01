#include "readframe.h"
#include "frame_reader.h"

#include <stdlib.h>
#include <string.h>
#include "esp_log.h"
#include "ff.h"

#include "control_reader.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "ld_board.h"

/* ========================================================= */
ch_info_t ch_info_snapshot;

static const char* TAG = "readframe";

/* ================= runtime state ================= */

static table_frame_t frame_buf; /* single internal buffer */

static SemaphoreHandle_t sem_free;  /* buffer writable */
static SemaphoreHandle_t sem_ready; /* buffer readable */

static TaskHandle_t sd_task = NULL;

static bool inited = false;
static bool running = false;
static bool eof_reached = false;

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

static esp_err_t mount_sdcard(void) {
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

    esp_err_t ret = esp_vfs_fat_sdmmc_mount("/sd", &host, &slot_config, &mount_config, &g_sd_card);
    if(ret != ESP_OK) {
        ESP_LOGE(TAG, "SD mount failed (%s)", esp_err_to_name(ret));
        return ret;
    }

    return ESP_OK;
}

/* ================= SD reader task ================= */

static void sd_reader_task(void* arg) {
    while(running) {

        /* wait until buffer free */
        if(xSemaphoreTake(sem_free, portMAX_DELAY) != pdTRUE)
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

        if(err == ESP_ERR_NOT_FOUND) {
            ESP_LOGI(TAG, "EOF reached");
            eof_reached = true;
            
            xSemaphoreGive(sem_ready);
            continue;
        }

        if(err != ESP_OK) {
            ESP_LOGE(TAG, "frame_reader_read failed: %s", esp_err_to_name(err));
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

esp_err_t frame_system_init(const char* control_path, const char* frame_path) {
    esp_err_t err;

    if(inited){
        ESP_LOGE(TAG, "frame system already initialized");
        return ESP_ERR_INVALID_STATE;
    }

    /* ---------- 0. mount SD ---------- */
    err = mount_sdcard();
    if(err != ESP_OK)
        return err;

    /* ---------- 1. load control.dat -> ch_info ---------- */
    err = get_channel_info(control_path, &ch_info);
    if(err != ESP_OK) {
        ESP_LOGE(TAG, "get_channel_info failed: %s", esp_err_to_name(err));
        return err;
    }
    ch_info_snapshot = ch_info;  // snapshot

    /* ---------- 2. init frame reader ---------- */
    err = frame_reader_init(frame_path);
    if(err != ESP_OK) {
        ESP_LOGE(TAG, "frame_reader_init failed: %s", esp_err_to_name(err));
        return err;
    }

    /* ---------- 3. semaphores ---------- */
    sem_free = xSemaphoreCreateBinary();
    sem_ready = xSemaphoreCreateBinary();

    if(!sem_free || !sem_ready) {
        ESP_LOGE(TAG, "Failed to create semaphores");
        frame_reader_deinit();
        return ESP_ERR_NO_MEM;
    }

    xSemaphoreGive(sem_free); /* buffer initially free */

    /* ---------- 4. runtime ---------- */
    running = true;
    cmd     = CMD_NONE;
    eof_reached = false;

    /* ---------- 5. create SD reader task ---------- */
    xTaskCreate(sd_reader_task, "sd_reader", 16384, NULL, 5, &sd_task);

    inited = true;

    ESP_LOGI(TAG, "frame system initialized (new channel_info model)");
    return ESP_OK;
}

/* ---- sequential read ---- */

esp_err_t read_frame(table_frame_t* playerbuffer) {
    if(!inited){
        ESP_LOGE(TAG, "frame system not initialized");
        return ESP_ERR_INVALID_STATE;
    }
    if(!playerbuffer){
        ESP_LOGE(TAG, "playerbuffer is NULL");
        return ESP_ERR_INVALID_ARG;
    }
    if (eof_reached) 
        return ESP_ERR_NOT_FOUND;

    if(xSemaphoreTake(sem_ready, portMAX_DELAY) != pdTRUE){
        ESP_LOGE(TAG, "Failed to take sem_ready");
        return ESP_FAIL;
    }

    if(!running){
        ESP_LOGE(TAG, "frame system not running");
        return ESP_ERR_INVALID_STATE;
    }

    memcpy(playerbuffer, &frame_buf, sizeof(table_frame_t));

    xSemaphoreGive(sem_free);
    return ESP_OK;
}

/* ---- reset to frame 0 ---- */

esp_err_t frame_reset(void) {
    if(!inited){
        ESP_LOGE(TAG, "frame system not initialized");
        return ESP_ERR_INVALID_STATE;
    }

    /* drain ready semaphore */
    while(xSemaphoreTake(sem_ready, 0) == pdTRUE) {}
    
    running = true;
    eof_reached = false;
    cmd = CMD_RESET;
    xSemaphoreGive(sem_free);
    return ESP_OK;
}

/* ---- deinit frame system ---- */

esp_err_t frame_system_deinit(void) {
    if(!inited) {
        ESP_LOGW(TAG, "frame_system_deinit called when not initialized");
        return ESP_ERR_INVALID_STATE;
    }

    running = false;

    if(sd_task) {
        xSemaphoreGive(sem_free);
        vTaskDelay(pdMS_TO_TICKS(10));
    }

    if(sem_free)
        vSemaphoreDelete(sem_free);
    if(sem_ready)
        vSemaphoreDelete(sem_ready);

    frame_reader_deinit();

    sem_free = sem_ready = NULL;
    sd_task = NULL;
    inited = false;
    eof_reached = false;

    ESP_LOGI(TAG, "frame system deinit");
    return ESP_OK;
}

/* ---- end of file ---- */

bool is_eof_reached(void) {
    if (!inited) {
        return false;
    }
    return eof_reached;
}

/* ---- get sd card id ---- */
int get_sd_card_id(void) {
    if(g_sd_card == NULL) {
        return 0;
    }
    
    char volume_label[20];
    FRESULT res = f_getlabel("0:", volume_label, NULL);
    
    if(res != FR_OK || volume_label[0] == '\0') {
        return 0;
    }
    if(strncmp(volume_label, "LPS", 3) != 0) {
        return 0;
    }
    
    char* num_str = volume_label + 3;
    int id = atoi(num_str);
    
    if(id >= 1 && id <= 31) {
        return id;
    }
    
    return 0;
}
