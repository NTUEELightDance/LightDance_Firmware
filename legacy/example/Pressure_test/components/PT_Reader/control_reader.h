#pragma once
#include <stdint.h>
#include "esp_err.h"
#include "ld_board.h"

// esp_err_t control_reader_load(const char *path, control_info_t *out);
// void      control_reader_free(control_info_t *info);

esp_err_t get_channel_info(const char* control_path, ch_info_t* out);