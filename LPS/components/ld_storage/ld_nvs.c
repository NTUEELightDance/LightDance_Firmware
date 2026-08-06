#include "ld_nvs.h"

#include "esp_check.h"
#include "esp_log.h"
#include "nvs.h"
#include "nvs_flash.h"

static const char* TAG = "ld_nvs";
static const char* NVS_NAMESPACE = "ld_config";
static const char* NVS_KEY_CONTROL = "ld_control";
static const char* NVS_KEY_FRAME = "ld_frame";
static const char* NVS_KEY_LIGHT_TABLE_SIZE = "ltable_size";

esp_err_t ld_nvs_init(void) {
    esp_err_t err = nvs_flash_init();

    if(err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_RETURN_ON_ERROR(nvs_flash_erase(), TAG, "failed to erase nvs flash");
        err = nvs_flash_init();
    }

    ESP_RETURN_ON_ERROR(err, TAG, "failed to init nvs flash");

    return ESP_OK;
}

esp_err_t ld_nvs_set_u8(const char* key, uint8_t val) {
    ESP_RETURN_ON_FALSE(key != NULL, ESP_ERR_INVALID_ARG, TAG, "key is NULL");

    nvs_handle_t handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &handle);
    ESP_RETURN_ON_ERROR(err, TAG, "failed to open nvs namespace");

    err = nvs_set_u8(handle, key, val);
    if(err != ESP_OK) {
        nvs_close(handle);
        ESP_RETURN_ON_ERROR(err, TAG, "failed to set %s", key);
    }

    err = nvs_commit(handle);
    nvs_close(handle);
    ESP_RETURN_ON_ERROR(err, TAG, "failed to commit %s", key);

    ESP_LOGI(TAG, "set %s = %u", key, (unsigned)val);

    return ESP_OK;
}

esp_err_t ld_nvs_get_u8(const char* key, uint8_t* val) {
    ESP_RETURN_ON_FALSE(key != NULL, ESP_ERR_INVALID_ARG, TAG, "key is NULL");
    ESP_RETURN_ON_FALSE(val != NULL, ESP_ERR_INVALID_ARG, TAG, "val is NULL");

    nvs_handle_t handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READONLY, &handle);
    ESP_RETURN_ON_ERROR(err, TAG, "failed to open nvs namespace");

    err = nvs_get_u8(handle, key, val);
    nvs_close(handle);
    ESP_RETURN_ON_ERROR(err, TAG, "failed to get %s", key);

    return ESP_OK;
}

static esp_err_t ld_nvs_set_u32(const char* key, uint32_t val) {
    ESP_RETURN_ON_FALSE(key != NULL, ESP_ERR_INVALID_ARG, TAG, "key is NULL");

    nvs_handle_t handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &handle);
    ESP_RETURN_ON_ERROR(err, TAG, "failed to open nvs namespace");

    err = nvs_set_u32(handle, key, val);
    if(err != ESP_OK) {
        nvs_close(handle);
        ESP_RETURN_ON_ERROR(err, TAG, "failed to set %s", key);
    }

    err = nvs_commit(handle);
    nvs_close(handle);
    ESP_RETURN_ON_ERROR(err, TAG, "failed to commit %s", key);

    ESP_LOGI(TAG, "set %s = %lu", key, (unsigned long)val);

    return ESP_OK;
}

static esp_err_t ld_nvs_get_u32(const char* key, uint32_t* val) {
    ESP_RETURN_ON_FALSE(key != NULL, ESP_ERR_INVALID_ARG, TAG, "key is NULL");
    ESP_RETURN_ON_FALSE(val != NULL, ESP_ERR_INVALID_ARG, TAG, "val is NULL");

    nvs_handle_t handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READONLY, &handle);
    ESP_RETURN_ON_ERROR(err, TAG, "failed to open nvs namespace");

    err = nvs_get_u32(handle, key, val);
    nvs_close(handle);
    ESP_RETURN_ON_ERROR(err, TAG, "failed to get %s", key);

    return ESP_OK;
}

esp_err_t ld_nvs_set_player_id(uint8_t player_id) {
    return ld_nvs_set_u8(LD_NVS_KEY_PLAYER_ID, player_id);
}

esp_err_t ld_nvs_get_player_id(uint8_t* player_id) {
    return ld_nvs_get_u8(LD_NVS_KEY_PLAYER_ID, player_id);
}

esp_err_t ld_nvs_set_control_size(uint32_t control_size) {
    return ld_nvs_set_u32(NVS_KEY_CONTROL, control_size);
}

esp_err_t ld_nvs_get_control_size(uint32_t* control_size) {
    return ld_nvs_get_u32(NVS_KEY_CONTROL, control_size);
}

esp_err_t ld_nvs_set_frame_size(uint32_t frame_size) {
    return ld_nvs_set_u32(NVS_KEY_FRAME, frame_size);
}

esp_err_t ld_nvs_get_frame_size(uint32_t* frame_size) {
    return ld_nvs_get_u32(NVS_KEY_FRAME, frame_size);
}

esp_err_t ld_nvs_set_light_table_size(uint32_t light_table_size) {
    return ld_nvs_set_u32(NVS_KEY_LIGHT_TABLE_SIZE, light_table_size);
}

esp_err_t ld_nvs_get_light_table_size(uint32_t* light_table_size) {
    return ld_nvs_get_u32(NVS_KEY_LIGHT_TABLE_SIZE, light_table_size);
}
