#pragma once

#include <stdint.h>

/**
 * @file ld_math_u8.h
 * @brief Small integer math helpers for 8-bit color pipelines.
 */

/**
 * @brief Return max(a, b, c).
 */
static inline uint8_t u8_max3(uint8_t a, uint8_t b, uint8_t c) {
    uint8_t m = (a > b) ? a : b;
    return (m > c) ? m : c;
}

/**
 * @brief Return min(a, b, c).
 */
static inline uint8_t u8_min3(uint8_t a, uint8_t b, uint8_t c) {
    uint8_t m = (a < b) ? a : b;
    return (m < c) ? m : c;
}

/**
 * @brief Compute rounded (x * y) / 255.
 *
 * Inputs and output are all 0..255.
 */
static inline uint8_t mul255_u8(uint8_t x, uint8_t y) {
    return (uint8_t)(((uint16_t)x * (uint16_t)y + 127) / 255);
}

/**
 * @brief Linear interpolation in 8-bit domain.
 *
 * t is in [0, 255], where 0 means start and 255 means end.
 */
static inline uint8_t lerp_u8(uint8_t start, uint8_t end, uint8_t t) {
    uint16_t val = (uint16_t)start * (255 - t) + (uint16_t)end * t;
    return (uint8_t)((val + 127) / 255);
}
