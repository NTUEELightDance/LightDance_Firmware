#include "readframe.h"
#include "frame_reader.h"
#include "control_reader.h"

#include <stdlib.h>
#include <string.h>
#include "esp_log.h"
#include "ff.h"
#include "esp_err.h"

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "ld_board.h"

/* ========================================================= */
ch_info_t ch_info_snapshot;

static const char* TAG = "READFRAME";

/* ================= runtime state ================= */

static table_frame_t frame_buf; /* single internal buffer */

static SemaphoreHandle_t sem_free;  /* buffer writable */
static SemaphoreHandle_t sem_ready; /* buffer readable */

static TaskHandle_t pt_task = NULL;

static bool inited = false;
static bool running = false;
static bool eof_reached = false;
static bool has_error = false;

/* ================= SD task command ================= */

typedef enum {
    CMD_NONE = 0,
    CMD_RESET,
} sd_cmd_t;

static sd_cmd_t cmd = CMD_NONE;
static frame_reader_status g_frame_status = {
    .err = ESP_OK,
    .fr  = FR_OK,
    .br  = 0
};

/* ================= SD mount ================= */

#include "driver/sdmmc_host.h"
#include "esp_vfs_fat.h"
#include "sdmmc_cmd.h"



static void pt_reader_task(void* arg) {
    while(running) {

        //wait until buffer free 
        if(xSemaphoreTake(sem_free, portMAX_DELAY) != pdTRUE)
            continue;

        /* ---- command handling ---- */
        if (cmd == CMD_RESET) {
            frame_reader_reset(); //correction
            eof_reached = false;
            has_error = false;
            cmd = CMD_NONE;
            g_frame_status.err = ESP_OK;
            g_frame_status.fr  = FR_OK;
            g_frame_status.br  = 0;
            xSemaphoreGive(sem_free);
            continue;
        }
        if (has_error){
            xSemaphoreGive(sem_free);
            vTaskDelay(pdMS_TO_TICKS(20));
            continue;

        }
        if (eof_reached){
            vTaskDelay(pdMS_TO_TICKS(10));
            continue;
        }

        // read one frame 
        frame_reader_status st = {0};
        esp_err_t err = frame_reader_read(&frame_buf, &st);
        g_frame_status = st;

        if(err == ESP_ERR_FRAME_EOF) {
            ESP_LOGI(TAG, "EOF reached");
            eof_reached = true;
            
            xSemaphoreGive(sem_ready);
            continue;
        }

        if(err != ESP_OK) {
            ESP_LOGE(TAG, "frame_reader_read failed: %s", esp_err_to_name(err));
            has_error = true;
            xSemaphoreGive(sem_ready);
            continue;
        }

        /* buffer ready */
        xSemaphoreGive(sem_ready);
    }

    ESP_LOGI(TAG, "pt_reader_task exit");
    pt_task = NULL;
    vTaskDelete(NULL);
}



esp_err_t frame_system_init(const char* control_path, const char* frame_path) {
    esp_err_t err;

    if(inited){
        ESP_LOGE(TAG, "frame system already initialized");
        return ESP_ERR_INVALID_STATE;
    }


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

    running = true;
    cmd     = CMD_NONE;
    eof_reached = false;
    has_error = false;

    xTaskCreate(pt_reader_task, "sd_reader", 16384, NULL, 5, &pt_task);

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
        return ESP_ERR_FRAME_EOF;

    if(xSemaphoreTake(sem_ready, portMAX_DELAY) != pdTRUE){
        ESP_LOGE(TAG, "Failed to take sem_ready");
        return ESP_ERR_TIMEOUT;
    }
    esp_err_t err = g_frame_status.err;

    if (err == ESP_OK) {
        memcpy(playerbuffer, &frame_buf, sizeof(table_frame_t));
        xSemaphoreGive(sem_free);
        return ESP_OK;
    }

    xSemaphoreGive(sem_free);
    return err;
}

/* ---- reset to frame 0 ---- */

esp_err_t frame_reset(void) {
    if(!inited){
        ESP_LOGE(TAG, "frame system not initialized");
        return ESP_ERR_INVALID_STATE;
    }

    /* drain ready semaphore */
    while(xSemaphoreTake(sem_ready, 0) == pdTRUE) {}
    
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

    if(pt_task) {
        xSemaphoreGive(sem_free);
        vTaskDelay(pdMS_TO_TICKS(10));
    }

    if(sem_free)
        vSemaphoreDelete(sem_free);
    if(sem_ready)
        vSemaphoreDelete(sem_ready);

    frame_reader_deinit();

    sem_free = sem_ready = NULL;
    pt_task = NULL;
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


