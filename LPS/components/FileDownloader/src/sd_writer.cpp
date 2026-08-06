#include "sd_writer.h"
#include <errno.h>
#include <stdio.h>
#include "esp_log.h"

static const char *TAG = "sd_writer";
static FILE *file = NULL;
static bool file_opened = false;

/* ========= API ========= */

esp_err_t sd_writer_init(const char *file_path)
{
    if (file_opened) {
        ESP_LOGW(TAG, "file already opened");
        return ESP_OK;
    }

    file = fopen(file_path, "wb");
    if (!file) {
        ESP_LOGE(TAG, "fopen failed for %s (errno=%d). Check if SPIFFS is mounted.", file_path, errno);
        return ESP_FAIL;
    }

    file_opened = true;
    ESP_LOGI(TAG, "file opened: %s", file_path);
    return ESP_OK;
}

esp_err_t sd_writer_write(const void *data, size_t len)
{
    if (!file_opened) return ESP_ERR_INVALID_STATE;

    size_t bw = fwrite(data, 1, len, file);
    if (bw != len) {
        ESP_LOGE(TAG, "fwrite failed bw=%u/%u errno=%d", (unsigned)bw, (unsigned)len, errno);
        return ESP_FAIL;
    }

    return ESP_OK;
}

void sd_writer_close(void)
{
    if (file_opened) {
        fflush(file);
        fclose(file);
        file = NULL;
        file_opened = false;
        ESP_LOGI(TAG, "file closed");
    }
}
