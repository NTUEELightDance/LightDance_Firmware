#pragma once
#include <stdint.h>
#include "esp_err.h"

typedef struct {
    uint16_t version;        // major/minor packed as 2 bytes (little-endian bytes in file)
    uint8_t  of_num;         // OF_num
    uint8_t  strip_num;      // Strip_num

    uint8_t  *led_num;       // [strip_num]
    uint32_t frame_num;      // Frame_num
    uint32_t *timestamps;    // [frame_num]  time_stamp[]
} control_info_t;

esp_err_t control_reader_load(const char *path, control_info_t *out);
void      control_reader_free(control_info_t *info);
