#include "sd_utils.h"

#include "esp_log.h"
#include "esp_spiffs.h"

static const char* TAG = "SPIFFS";
static bool g_spiffs_mounted = false;

esp_err_t mount_spiffs(void) {
    if(g_spiffs_mounted) {
        ESP_LOGD(TAG, "mount skipped: SPIFFS is already mounted");
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
        ESP_LOGE(
            TAG, "mount failed: partition=%s path=%s format_on_failure=%s err=%s", config.partition_label, config.base_path, config.format_if_mount_failed ? "true" : "false", esp_err_to_name(ret));
        return ret;
    }

    size_t total = 0;
    size_t used = 0;
    ret = esp_spiffs_info(config.partition_label, &total, &used);
    if(ret != ESP_OK) {
        ESP_LOGE(TAG, "usage query failed; unmounting partition=%s: %s", config.partition_label, esp_err_to_name(ret));
        esp_vfs_spiffs_unregister(config.partition_label);
        return ret;
    }

    g_spiffs_mounted = true;
    ESP_LOGI(TAG, "mounted: partition=%s path=%s used=%u bytes total=%u bytes", config.partition_label, config.base_path, (unsigned)used, (unsigned)total);
    return ESP_OK;
}

void unmount_spiffs(void) {
    if(g_spiffs_mounted) {
        esp_vfs_spiffs_unregister("storage");
        g_spiffs_mounted = false;
        ESP_LOGI(TAG, "unmounted: partition=storage");
    }
}
