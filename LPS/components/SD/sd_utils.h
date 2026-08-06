#ifndef SPIFFS_UTILS_H
#define SPIFFS_UTILS_H

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Mount the SPIFFS partition at /spiffs
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t mount_spiffs(void);

/**
 * @brief Unmount SPIFFS
 */
void unmount_spiffs(void);

#ifdef __cplusplus
}
#endif

#endif // SPIFFS_UTILS_H
