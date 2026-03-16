#pragma once
#include "esp_err.h"
#include "ff.h"


/* base offset for frame errors */
#define ESP_ERR_FRAME_BASE        (ESP_ERR_INVALID_STATE + 0x100)

#define ESP_ERR_FRAME_EOF         (ESP_ERR_FRAME_BASE + 1)
#define ESP_ERR_FRAME_IO          (ESP_ERR_FRAME_BASE + 2)
#define ESP_ERR_FRAME_CORRUPT     (ESP_ERR_FRAME_BASE + 3)
#define ESP_ERR_FRAME_TIMEOUT     (ESP_ERR_FRAME_BASE + 4)