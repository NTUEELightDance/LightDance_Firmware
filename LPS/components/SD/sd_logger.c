#include "sd_logger.h"
#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "unistd.h"

#define BUFFER_SIZE (4 * 1024)  // ring buffer size
#define TEMP_BUFFER_SIZE 256    // single logger size
#define MUTEX_WAIT_TICKS pdMS_TO_TICKS(1)
static const char* TAG = "FILE_LOG";

typedef struct {
    char data[BUFFER_SIZE];  // data buffer
    uint32_t head;           // write position
    uint32_t tail;           // read position
    SemaphoreHandle_t mutex;
    FILE* file;
    bool running;
    TaskHandle_t task;
} ring_buffer_t;

static ring_buffer_t* g_buf = NULL;
static vprintf_like_t orig_vprintf = NULL;

static void flush_buffer(void) {
    if(!g_buf->file || g_buf->head == g_buf->tail)
        return;

    size_t written = 0;
    if(g_buf->head > g_buf->tail) {
        written = fwrite(&g_buf->data[g_buf->tail], 1, g_buf->head - g_buf->tail, g_buf->file);
    } else {
        written = fwrite(&g_buf->data[g_buf->tail], 1, BUFFER_SIZE - g_buf->tail, g_buf->file);
        written += fwrite(g_buf->data, 1, g_buf->head, g_buf->file);
    }

    if(written > 0) {
        g_buf->tail = (g_buf->tail + written) % BUFFER_SIZE;
        fflush(g_buf->file);
        fsync(fileno(g_buf->file));
    }
}

static int ring_buffer_write(const char* fmt, va_list args) {
    if(!g_buf || !g_buf->running)
        return 0;

    // not warning or error: into ring buffer
    char temp[TEMP_BUFFER_SIZE];
    int len = vsnprintf(temp, sizeof(temp), fmt, args);
    if(len <= 0)
        return 0;
    size_t write_len = (size_t)len < sizeof(temp) ? (size_t)len : sizeof(temp) - 1U;

    if(xSemaphoreTake(g_buf->mutex, MUTEX_WAIT_TICKS) != pdTRUE) {
        return 0;
    }

    for(size_t i = 0; i < write_len; i++) {
        uint32_t next = (g_buf->head + 1) % BUFFER_SIZE;
        g_buf->data[g_buf->head] = temp[i];
        g_buf->head = next;
    }

    xSemaphoreGive(g_buf->mutex);
    return len;
}

static void flush_task(void* arg) {
    while(g_buf->running) {
        if(xSemaphoreTake(g_buf->mutex, MUTEX_WAIT_TICKS) == pdTRUE) {
            flush_buffer();
            xSemaphoreGive(g_buf->mutex);
        }

        vTaskDelay(50);
    }
    vTaskDelete(NULL);
}

esp_err_t sd_log_init() {
    ESP_LOGI(TAG, "persistent logger initialization started: buffer_size=%u bytes", (unsigned)BUFFER_SIZE);
    g_buf = heap_caps_malloc(sizeof(ring_buffer_t), MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);

    if(!g_buf) {
        ESP_LOGE(TAG, "persistent logger allocation failed: size=%u bytes", (unsigned)sizeof(ring_buffer_t));
        return ESP_ERR_NO_MEM;
    }
    memset(g_buf, 0, sizeof(ring_buffer_t));

    g_buf->mutex = xSemaphoreCreateMutex();
    if(!g_buf->mutex) {
        ESP_LOGE(TAG, "persistent logger mutex creation failed");
        free(g_buf);
        return ESP_ERR_NO_MEM;
    }

    // update logger name while reset
    char path[32];
    int index = 1;
    struct stat st;
    while(1) {
        snprintf(path, sizeof(path), "/spiffs/log%d.log", index);
        if(stat(path, &st) != 0) {
            break;
        }
        index++;
    }
    g_buf->file = fopen(path, "w+");
    if(!g_buf->file) {
        ESP_LOGE(TAG, "log file open failed: path=%s errno=%d (%s)", path, errno, strerror(errno));
        vSemaphoreDelete(g_buf->mutex);
        free(g_buf);
        return ESP_FAIL;
    }
    fflush(g_buf->file);
    g_buf->running = true;
    ESP_LOGI(TAG, "persistent log file opened: path=%s", path);

    if(xTaskCreate(flush_task, "sd_logger_flush", 2048, NULL, 1, &g_buf->task) != pdPASS) {
        ESP_LOGE(TAG, "persistent logger flush task creation failed: stack_size=2048 priority=1");
        fclose(g_buf->file);
        vSemaphoreDelete(g_buf->mutex);
        free(g_buf);
        g_buf = NULL;
        return ESP_ERR_NO_MEM;
    }
    orig_vprintf = esp_log_set_vprintf(ring_buffer_write);

    vTaskDelay(pdMS_TO_TICKS(100));
    return ESP_OK;
}

esp_err_t sd_log_deinit(void) {
    if(!g_buf)
        return ESP_ERR_INVALID_STATE;

    g_buf->running = false;
    vTaskDelay(pdMS_TO_TICKS(50));

    if(g_buf->file) {
        if(xSemaphoreTake(g_buf->mutex, MUTEX_WAIT_TICKS) == pdTRUE) {
            flush_buffer();
            xSemaphoreGive(g_buf->mutex);
        }
        fclose(g_buf->file);
    }

    esp_log_set_vprintf(orig_vprintf);

    if(g_buf->mutex)
        vSemaphoreDelete(g_buf->mutex);
    free(g_buf);
    g_buf = NULL;

    return ESP_OK;
}

esp_err_t sd_log_flush(void) {
    if(!g_buf || !g_buf->running)
        return ESP_ERR_INVALID_STATE;

    if(xSemaphoreTake(g_buf->mutex, MUTEX_WAIT_TICKS) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }
    flush_buffer();
    xSemaphoreGive(g_buf->mutex);

    return ESP_OK;
}
