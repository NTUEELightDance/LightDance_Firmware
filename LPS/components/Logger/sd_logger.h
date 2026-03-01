#pragma once

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t sd_logger_init(const char* log_path);

void sd_logger_deinit(void);

int sd_log_printf(const char* format, ...);

#ifdef __cplusplus
}
#endif