#pragma once

#include <stdint.h>

#include "esp_err.h"

#define LD_NVS_KEY_PLAYER_ID "PLAYER_ID"

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t ld_nvs_init(void);

esp_err_t ld_nvs_set_u8(const char* key, uint8_t val);
esp_err_t ld_nvs_get_u8(const char* key, uint8_t* val);

esp_err_t ld_nvs_set_player_id(uint8_t player_id);
esp_err_t ld_nvs_get_player_id(uint8_t* player_id);
esp_err_t ld_nvs_set_control_size(uint32_t control_size);
esp_err_t ld_nvs_get_control_size(uint32_t* control_size);
esp_err_t ld_nvs_set_frame_size(uint32_t frame_size);
esp_err_t ld_nvs_get_frame_size(uint32_t* frame_size);

esp_err_t ld_nvs_set_light_table_size(uint32_t light_table_size);
esp_err_t ld_nvs_get_light_table_size(uint32_t* light_table_size);

#ifdef __cplusplus
}
#endif
