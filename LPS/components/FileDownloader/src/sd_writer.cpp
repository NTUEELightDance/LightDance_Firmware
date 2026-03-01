#include "sd_writer.h"
#include "esp_log.h"
#include "ff.h"

static const char *TAG = "sd_writer";
static FIL file;
static bool file_opened = false;

/* ========= API ========= */

esp_err_t sd_writer_init(const char *file_path)
{
    if (file_opened) {
        ESP_LOGW(TAG, "file already opened");
        return ESP_OK;
    }

    FRESULT fr = f_open(&file, file_path, FA_WRITE | FA_CREATE_ALWAYS);
    if (fr != FR_OK) {
        ESP_LOGE(TAG, "f_open failed (%d). Check if SD is mounted in main.", fr);
        return ESP_FAIL;
    }

    file_opened = true;
    ESP_LOGI(TAG, "file opened: %s", file_path);
    return ESP_OK;
}

esp_err_t sd_writer_write(const void *data, size_t len)
{
    if (!file_opened) return ESP_ERR_INVALID_STATE;

    UINT bw = 0;
    FRESULT fr = f_write(&file, data, len, &bw);

    if (fr != FR_OK || bw != len) {
        ESP_LOGE(TAG, "f_write failed fr=%d bw=%d/%d", fr, bw, len);
        return ESP_FAIL;
    }

    return ESP_OK;
}

void sd_writer_close(void)
{
    if (file_opened) {
        f_close(&file);
        file_opened = false;
        ESP_LOGI(TAG, "file closed");
    }
}