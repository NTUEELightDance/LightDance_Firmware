#include "ld_nvs.h"

#include "esp_check.h"
#include "esp_log.h"
#include "nvs.h"
#include "nvs_flash.h"

static const char* TAG = "LD_NVS";
static const char* NVS_NAMESPACE = "ld_config";
static const char* NVS_KEY_CONTROL = "ld_control";
static const char* NVS_KEY_FRAME = "ld_frame";
static const char* NVS_KEY_LIGHT_TABLE_SIZE = "ltable_size";

esp_err_t ld_nvs_init(void) {
    esp_err_t err = nvs_flash_init();

    if(err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_LOGW(TAG, "NVS partition requires recovery: %s; erasing partition", esp_err_to_name(err));
        ESP_RETURN_ON_ERROR(nvs_flash_erase(), TAG, "NVS partition erase failed");
        err = nvs_flash_init();
    }

    ESP_RETURN_ON_ERROR(err, TAG, "NVS flash initialization failed");

    ESP_LOGI(TAG, "NVS flash ready: namespace=%s", NVS_NAMESPACE);

    return ESP_OK;
}

esp_err_t ld_nvs_set_u8(const char* key, uint8_t val) {
    ESP_RETURN_ON_FALSE(key != NULL, ESP_ERR_INVALID_ARG, TAG, "set u8 rejected: key is NULL");

    nvs_handle_t handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &handle);
    ESP_RETURN_ON_ERROR(err, TAG, "NVS namespace open failed: namespace=%s mode=readwrite", NVS_NAMESPACE);

    err = nvs_set_u8(handle, key, val);
    if(err != ESP_OK) {
        nvs_close(handle);
        ESP_RETURN_ON_ERROR(err, TAG, "NVS value write failed: namespace=%s key=%s", NVS_NAMESPACE, key);
    }

    err = nvs_commit(handle);
    nvs_close(handle);
    ESP_RETURN_ON_ERROR(err, TAG, "NVS commit failed: namespace=%s key=%s", NVS_NAMESPACE, key);

    ESP_LOGI(TAG, "value stored: namespace=%s key=%s value=%u", NVS_NAMESPACE, key, (unsigned)val);

    return ESP_OK;
}

esp_err_t ld_nvs_get_u8(const char* key, uint8_t* val) {
    ESP_RETURN_ON_FALSE(key != NULL, ESP_ERR_INVALID_ARG, TAG, "get u8 rejected: key is NULL");
    ESP_RETURN_ON_FALSE(val != NULL, ESP_ERR_INVALID_ARG, TAG, "get u8 rejected: output pointer is NULL");

    nvs_handle_t handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READONLY, &handle);
    ESP_RETURN_ON_ERROR(err, TAG, "NVS namespace open failed: namespace=%s mode=readonly", NVS_NAMESPACE);

    err = nvs_get_u8(handle, key, val);
    nvs_close(handle);
    ESP_RETURN_ON_ERROR(err, TAG, "NVS value read failed: namespace=%s key=%s", NVS_NAMESPACE, key);

    return ESP_OK;
}

static esp_err_t ld_nvs_set_u32(const char* key, uint32_t val) {
    ESP_RETURN_ON_FALSE(key != NULL, ESP_ERR_INVALID_ARG, TAG, "set u32 rejected: key is NULL");

    nvs_handle_t handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &handle);
    ESP_RETURN_ON_ERROR(err, TAG, "NVS namespace open failed: namespace=%s mode=readwrite", NVS_NAMESPACE);

    err = nvs_set_u32(handle, key, val);
    if(err != ESP_OK) {
        nvs_close(handle);
        ESP_RETURN_ON_ERROR(err, TAG, "NVS value write failed: namespace=%s key=%s", NVS_NAMESPACE, key);
    }

    err = nvs_commit(handle);
    nvs_close(handle);
    ESP_RETURN_ON_ERROR(err, TAG, "NVS commit failed: namespace=%s key=%s", NVS_NAMESPACE, key);

    ESP_LOGI(TAG, "value stored: namespace=%s key=%s value=%lu", NVS_NAMESPACE, key, (unsigned long)val);

    return ESP_OK;
}

static esp_err_t ld_nvs_get_u32(const char* key, uint32_t* val) {
    ESP_RETURN_ON_FALSE(key != NULL, ESP_ERR_INVALID_ARG, TAG, "get u32 rejected: key is NULL");
    ESP_RETURN_ON_FALSE(val != NULL, ESP_ERR_INVALID_ARG, TAG, "get u32 rejected: output pointer is NULL");

    nvs_handle_t handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READONLY, &handle);
    ESP_RETURN_ON_ERROR(err, TAG, "NVS namespace open failed: namespace=%s mode=readonly", NVS_NAMESPACE);

    err = nvs_get_u32(handle, key, val);
    nvs_close(handle);
    ESP_RETURN_ON_ERROR(err, TAG, "NVS value read failed: namespace=%s key=%s", NVS_NAMESPACE, key);

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
