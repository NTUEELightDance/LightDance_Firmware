#pragma once
#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"
#include "frame.h"
#include "led_types.h"
#include "player_protocal.h"

typedef struct {
    uint8_t of_num;
    uint8_t strip_num;
    const uint8_t* led_num;  // [strip_num]
} frame_layout_t;

esp_err_t frame_reader_init(const char* path, const frame_layout_t* layout);
esp_err_t frame_reader_read(table_frame_t* out);  // reads timestamp from frame.dat
esp_err_t frame_reader_seek(uint32_t offset);
uint32_t frame_reader_frame_size(void);
void frame_reader_deinit(void);
