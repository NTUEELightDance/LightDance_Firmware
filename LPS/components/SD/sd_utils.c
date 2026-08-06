#include "sd_utils.h"

#include "esp_log.h"
#include "esp_spiffs.h"

static const char* TAG = "SPIFFS";
static bool g_spiffs_mounted = false;

esp_err_t mount_spiffs(void) {
    if(g_spiffs_mounted) {
        return ESP_OK;
    }

    const esp_vfs_spiffs_conf_t config = {
        .base_path = "/spiffs",
        .partition_label = "storage",
        .max_files = 5,
        .format_if_mount_failed = true,
    };

    esp_err_t ret = esp_vfs_spiffs_register(&config);
    if(ret != ESP_OK) {
        ESP_LOGE(TAG, "SPIFFS mount failed (%s)", esp_err_to_name(ret));
        return ret;
    }

    size_t total = 0;
    size_t used = 0;
    ret = esp_spiffs_info(config.partition_label, &total, &used);
    if(ret != ESP_OK) {
        ESP_LOGE(TAG, "failed to query SPIFFS usage (%s)", esp_err_to_name(ret));
        esp_vfs_spiffs_unregister(config.partition_label);
        return ret;
    }

    g_spiffs_mounted = true;
    ESP_LOGI(TAG, "mounted at %s, total=%u, used=%u", config.base_path, (unsigned)total, (unsigned)used);
    return ESP_OK;
}

void unmount_spiffs(void) {
    if(g_spiffs_mounted) {
        esp_vfs_spiffs_unregister("storage");
        g_spiffs_mounted = false;
    }
}

