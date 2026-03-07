#pragma once
#include "esp_err.h"
#include "ff.h"
#include <stddef.h>

esp_err_t sd_writer_init(const char *path);
esp_err_t sd_writer_write(const void *data, size_t len);
void sd_writer_close(void);