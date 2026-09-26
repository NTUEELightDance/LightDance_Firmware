#include "readframe.h"
#include "control_reader.h"
#include "frame_reader.h"

#include <string.h>

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "ld_board.h"

/* ========================================================= */
ch_info_t ch_info_snapshot;

static const char* TAG = "FRAME_SYSTEM";

/* ================= runtime state ================= */

static table_frame_t frame_buf; /* single internal buffer */

static SemaphoreHandle_t sem_free;  /* buffer writable */
static SemaphoreHandle_t sem_ready; /* buffer readable */

static TaskHandle_t pt_task = NULL;

static bool inited = false;
static bool running = false;
static bool eof_reached = false;
static bool has_error = false;

/* ================= PT task command ================= */

typedef enum {
    CMD_NONE = 0,
    CMD_RESET,
    CMD_SEEK,
} pt_cmd_t;

typedef struct {
    esp_err_t err;
} frame_status_t;

static volatile frame_status_t g_frame_status = {.err = ESP_OK};

static volatile pt_cmd_t cmd = CMD_NONE;
static volatile uint32_t cmd_seek_frame_idx = 0;
static volatile uint32_t reader_epoch = 0;

static void set_reader_state(esp_err_t status) {
    eof_reached = false;
    has_error = false;
    g_frame_status.err = status;
}

static esp_err_t schedule_reader_command(pt_cmd_t next_cmd, uint32_t frame_idx) {
    while(xSemaphoreTake(sem_ready, 0) == pdTRUE) {}

    reader_epoch++;
    cmd_seek_frame_idx = frame_idx;
    cmd = next_cmd;
    set_reader_state(ESP_FAIL);
    xSemaphoreGive(sem_free);

    return ESP_OK;
}

/* ================= PT reader task ================= */

static void pt_reader_task(void* arg) {
    (void)arg;

    while(true) {
        if(xSemaphoreTake(sem_free, portMAX_DELAY) != pdTRUE) {
            continue;
        }

        if(!running) {
            break;
        }

        if(cmd == CMD_RESET) {
            g_frame_status.err = frame_reader_reset();
            cmd = CMD_NONE;

            if(g_frame_status.err != ESP_OK) {
                has_error = true;
                xSemaphoreGive(sem_ready);
                continue;
            }

            eof_reached = false;
            has_error = false;
            memset(&frame_buf, 0, sizeof(frame_buf));
            g_frame_status.err = ESP_OK;
            xSemaphoreGive(sem_free);
            continue;
        }

        if(cmd == CMD_SEEK) {
            g_frame_status.err = frame_reader_seek(cmd_seek_frame_idx);
            cmd = CMD_NONE;

            if(g_frame_status.err != ESP_OK) {
                has_error = true;
                xSemaphoreGive(sem_ready);
                continue;
            }

            eof_reached = false;
            has_error = false;
            memset(&frame_buf, 0, sizeof(frame_buf));
            g_frame_status.err = ESP_OK;
            xSemaphoreGive(sem_free);
            continue;
        }

        if(has_error) {
            xSemaphoreGive(sem_ready);
            continue;
        }

        if(eof_reached) {
            g_frame_status.err = ESP_ERR_NOT_FOUND;
            xSemaphoreGive(sem_ready);
            continue;
        }

        uint32_t read_epoch = reader_epoch;
        esp_err_t err = frame_reader_read(&frame_buf);

        if(!running) {
            break;
        }

        /* Drop any in-flight frame produced before a reset/seek command landed. */
        if(read_epoch != reader_epoch || cmd != CMD_NONE) {
            xSemaphoreGive(sem_free);
            continue;
        }

        g_frame_status.err = err;

        if(err == ESP_ERR_NOT_FOUND) {
            ESP_LOGI(TAG, "frame stream reached end of file");
            eof_reached = true;
            xSemaphoreGive(sem_ready);
            continue;
        }
        if(err == ESP_FAIL) {
            ESP_LOGE(TAG, "frame reader stopped after an unrecoverable I/O error");
            xSemaphoreGive(sem_ready);
            running = false;  // stop the task loop
            continue;
        }

        if(err != ESP_OK) {
            ESP_LOGE(TAG, "frame reader entered error state: %s", esp_err_to_name(err));
            has_error = true;
            xSemaphoreGive(sem_ready);
            continue;
        }

        xSemaphoreGive(sem_ready);
    }

    ESP_LOGI(TAG, "frame reader task stopped");
    pt_task = NULL;
    vTaskDelete(NULL);
}

/* ================= public API ================= */

esp_err_t frame_system_init(const char* control_path, const char* frame_path) {
    esp_err_t err;

    if(inited) {
        ESP_LOGW(TAG, "initialization request ignored: frame system is already initialized");
        return ESP_ERR_INVALID_STATE;
    }

    err = get_channel_info(control_path, &ch_info);
    if(err != ESP_OK) {
        ESP_LOGE(TAG, "control file load failed: path=%s err=%s", control_path, esp_err_to_name(err));
        return err;
    }
    ch_info_snapshot = ch_info;

    err = frame_reader_init(frame_path);
    if(err != ESP_OK) {
        ESP_LOGE(TAG, "frame file initialization failed: path=%s err=%s", frame_path, esp_err_to_name(err));
        control_reader_clear();
        return err;
    }

    sem_free = xSemaphoreCreateBinary();
    sem_ready = xSemaphoreCreateBinary();

    if(!sem_free || !sem_ready) {
        ESP_LOGE(TAG, "frame buffer semaphore creation failed: free_sem=%s ready_sem=%s", sem_free ? "ok" : "failed", sem_ready ? "ok" : "failed");
        frame_reader_deinit();
        control_reader_clear();
        if(sem_free) {
            vSemaphoreDelete(sem_free);
        }
        if(sem_ready) {
            vSemaphoreDelete(sem_ready);
        }
        sem_free = NULL;
        sem_ready = NULL;
        return ESP_ERR_NO_MEM;
    }

    xSemaphoreGive(sem_free);

    memset(&frame_buf, 0, sizeof(frame_buf));
    set_reader_state(ESP_OK);
    running = true;
    cmd = CMD_NONE;
    cmd_seek_frame_idx = 0;
    reader_epoch = 0;

    if(xTaskCreate(pt_reader_task, "pt_reader", 16384, NULL, 5, &pt_task) != pdPASS) {
        ESP_LOGE(TAG, "frame reader task creation failed: stack_size=16384 priority=5");
        running = false;
        frame_reader_deinit();
        control_reader_clear();
        vSemaphoreDelete(sem_free);
        vSemaphoreDelete(sem_ready);
        sem_free = NULL;
        sem_ready = NULL;
        pt_task = NULL;
        return ESP_ERR_NO_MEM;
    }

    inited = true;

    ESP_LOGI(TAG, "frame system initialized: control_path=%s frame_path=%s", control_path, frame_path);
    return ESP_OK;
}

esp_err_t read_frame(table_frame_t* playerbuffer) {
    if(!inited) {
        ESP_LOGE(TAG, "frame read rejected: frame system is not initialized");
        return ESP_ERR_INVALID_STATE;
    }
    if(!playerbuffer) {
        ESP_LOGE(TAG, "frame read rejected: output buffer is NULL");
        return ESP_ERR_INVALID_ARG;
    }

    if(xSemaphoreTake(sem_ready, portMAX_DELAY) != pdTRUE) {
        ESP_LOGE(TAG, "frame read failed: ready semaphore could not be acquired");
        return ESP_FAIL;
    }
    if(!running) {
        ESP_LOGE(TAG, "frame read failed: reader task is not running");
        return ESP_FAIL;
    }

    esp_err_t err = g_frame_status.err;

    if(err == ESP_OK) {
        memcpy(playerbuffer, &frame_buf, sizeof(table_frame_t));
        xSemaphoreGive(sem_free);
        return ESP_OK;
    }

    xSemaphoreGive(sem_free);
    return err;
}

esp_err_t read_frame_seek(uint64_t time_ms) {
    if(!inited) {
        ESP_LOGE(TAG, "seek rejected: frame system is not initialized");
        return ESP_ERR_INVALID_STATE;
    }

    uint32_t frame_idx = 0;
    uint32_t current_ts = 0;
    uint32_t next_ts = 0;
    uint32_t frame_num = 0;
    esp_err_t err = control_reader_find_seek_frame_idx(time_ms, &frame_idx);
    if(err != ESP_OK) {
        ESP_LOGE(TAG, "seek target lookup failed: requested_ms=%llu err=%s", (unsigned long long)time_ms, esp_err_to_name(err));
        return err;
    }

    frame_num = control_reader_frame_count();
    err = control_reader_get_timestamp(frame_idx, &current_ts);
    if(err != ESP_OK) {
        ESP_LOGE(TAG, "seek timestamp lookup failed: frame_index=%lu err=%s", (unsigned long)frame_idx, esp_err_to_name(err));
        return err;
    }

    if((frame_idx + 1U) < frame_num && control_reader_get_timestamp(frame_idx + 1U, &next_ts) == ESP_OK) {
        ESP_LOGI(TAG,
                 "seek scheduled: requested_ms=%llu frames=%lu frame_index=%lu interval_ms=[%lu,%lu]",
                 (unsigned long long)time_ms,
                 (unsigned long)frame_num,
                 (unsigned long)frame_idx,
                 (unsigned long)current_ts,
                 (unsigned long)next_ts);
    } else {
        ESP_LOGI(TAG,
                 "seek scheduled: requested_ms=%llu frames=%lu frame_index=%lu interval_ms=[%lu,EOF]",
                 (unsigned long long)time_ms,
                 (unsigned long)frame_num,
                 (unsigned long)frame_idx,
                 (unsigned long)current_ts);
    }

    return schedule_reader_command(CMD_SEEK, frame_idx);
}

esp_err_t frame_reset(void) {
    if(!inited) {
        ESP_LOGE(TAG, "reset rejected: frame system is not initialized");
        return ESP_ERR_INVALID_STATE;
    }

    return schedule_reader_command(CMD_RESET, 0);
}

esp_err_t frame_system_deinit(void) {
    if(!inited) {
        return ESP_ERR_INVALID_STATE;
    }

    running = false;

    if(sem_free) {
        xSemaphoreGive(sem_free);
    }

    for(int i = 0; i < 50 && pt_task != NULL; i++) {
        vTaskDelay(pdMS_TO_TICKS(10));
    }

    frame_reader_deinit();
    control_reader_clear();

    if(sem_free) {
        vSemaphoreDelete(sem_free);
    }
    if(sem_ready) {
        vSemaphoreDelete(sem_ready);
    }
    sem_free = NULL;
    sem_ready = NULL;

    inited = false;
    memset(&frame_buf, 0, sizeof(frame_buf));
    set_reader_state(ESP_OK);
    cmd = CMD_NONE;
    cmd_seek_frame_idx = 0;
    reader_epoch = 0;
    pt_task = NULL;

    ESP_LOGI(TAG, "frame system deinitialized");

    return ESP_OK;
}

bool is_eof_reached(void) {
    if(!inited) {
        return false;
    }
    return eof_reached;
}
