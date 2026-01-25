#pragma once

#include "board.h"
#include "led_types.h"

typedef struct {
    grb8_t ws2812b[WS2812B_NUM][WS2812B_MAX_PIXEL_NUM];
    grb8_t pca9955b[PCA9955B_CH_NUM];
} frame_data;

typedef struct {
    uint64_t timestamp;
    bool fade;
    frame_data data;
} table_frame_t;