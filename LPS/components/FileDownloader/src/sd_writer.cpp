#include "sd_writer.h"
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include "esp_log.h"

static const char* TAG = "FILE_WRITER";
static FILE* file = NULL;
static bool file_opened = false;

/* ========= API ========= */

esp_err_t sd_writer_init(const char* file_path) {
    if(file_path == NULL) {
        ESP_LOGE(TAG, "file open rejected: path is NULL");
        return ESP_ERR_INVALID_ARG;
    }

    if(file_opened) {
        ESP_LOGW(TAG, "file open request ignored because another output file is already open: requested_path=%s", file_path);
        return ESP_OK;
    }

    file = fopen(file_path, "wb");
    if(!file) {
        ESP_LOGE(TAG, "file open failed: path=%s mode=wb errno=%d (%s)", file_path, errno, strerror(errno));
        return ESP_FAIL;
    }

    file_opened = true;
    ESP_LOGI(TAG, "output file opened: path=%s", file_path);
    return ESP_OK;
}

esp_err_t sd_writer_write(const void* data, size_t len) {
    if(!file_opened) {
        ESP_LOGE(TAG, "file write rejected: no output file is open requested_bytes=%u", (unsigned)len);
        return ESP_ERR_INVALID_STATE;
    }
    if(data == NULL && len > 0) {
        ESP_LOGE(TAG, "file write rejected: data pointer is NULL requested_bytes=%u", (unsigned)len);
        return ESP_ERR_INVALID_ARG;
    }

    size_t bw = fwrite(data, 1, len, file);
    if(bw != len) {
        ESP_LOGE(TAG, "file write failed: written=%u requested=%u errno=%d (%s)", (unsigned)bw, (unsigned)len, errno, strerror(errno));
        return ESP_FAIL;
    }

    return ESP_OK;
}

void sd_writer_close(void) {
    if(file_opened) {
        if(fflush(file) != 0) {
            ESP_LOGE(TAG, "file flush failed before close: errno=%d (%s)", errno, strerror(errno));
        }
        if(fclose(file) != 0) {
            ESP_LOGE(TAG, "file close failed: errno=%d (%s)", errno, strerror(errno));
        }
        file = NULL;
        file_opened = false;
        ESP_LOGI(TAG, "output file closed");
    }
}
