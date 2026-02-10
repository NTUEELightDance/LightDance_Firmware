#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "ld_board.h"
#include "ld_led_types.h"

/**
 * @file ld_frame.h
 * @brief Shared frame payload structures across reader/player/controller.
 */

/**
 * @brief Pixel payload for one logical frame.
 */
typedef struct {
    /** Per-channel pixels for PCA9955B outputs. */
    grb8_t pca9955b[PCA9955B_CH_NUM];
    /** Per-strip pixels for WS2812B outputs. */
    grb8_t ws2812b[WS2812B_NUM][WS2812B_MAX_PIXEL_NUM];
} frame_data;

/**
 * @brief Time-tagged frame entry loaded from pattern tables.
 */
typedef struct {
    /** Playback timestamp in microseconds or stream-defined unit. */
    uint64_t timestamp;
    /** Whether transition to this frame should use fading. */
    bool fade;
    /** Full frame payload. */
    frame_data data;
} table_frame_t;
