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
 * @brief Return min(a, b).
 */
static inline uint8_t u8_min(uint8_t a, uint8_t b) {
    return (a < b) ? a : b;
}

/**
 * @brief Return max(a, b).
 */
static inline uint8_t u8_max(uint8_t a, uint8_t b) {
    return (a > b) ? a : b;
}

/**
 * @brief Clamp x into [lo, hi].
 *
 * If lo > hi, bounds are swapped first.
 */
static inline uint8_t u8_clamp(uint8_t x, uint8_t lo, uint8_t hi) {
    if(lo > hi) {
        uint8_t tmp = lo;
        lo = hi;
        hi = tmp;
    }
    if(x < lo)
        return lo;
    if(x > hi)
        return hi;
    return x;
}

/**
 * @brief Return absolute difference |a - b|.
 */
static inline uint8_t u8_abs_diff(uint8_t a, uint8_t b) {
    return (a >= b) ? (uint8_t)(a - b) : (uint8_t)(b - a);
}

/**
 * @brief Saturating add in [0, 255].
 */
static inline uint8_t u8_add_sat(uint8_t a, uint8_t b) {
    uint16_t s = (uint16_t)a + (uint16_t)b;
    return (s > 255u) ? 255u : (uint8_t)s;
}

/**
 * @brief Saturating subtract in [0, 255].
 */
static inline uint8_t u8_sub_sat(uint8_t a, uint8_t b) {
    return (a >= b) ? (uint8_t)(a - b) : 0u;
}

/**
 * @brief Rounded average of two 8-bit values.
 */
static inline uint8_t u8_avg2_round(uint8_t a, uint8_t b) {
    return (uint8_t)(((uint16_t)a + (uint16_t)b + 1u) >> 1);
}

/**
 * @brief Compute rounded num / den, where num is 16-bit.
 *
 * Returns 0 if den is 0.
 */
static inline uint8_t u8_div_round_u16(uint16_t num, uint8_t den) {
    if(den == 0u)
        return 0u;
    return (uint8_t)((num + (uint16_t)(den / 2u)) / den);
}

/**
 * @brief Linearly map x from [in_lo, in_hi] to [out_lo, out_hi].
 *
 * Input is clamped to the input range. If in_lo == in_hi, returns out_lo.
 */
static inline uint8_t u8_map_linear(uint8_t x, uint8_t in_lo, uint8_t in_hi, uint8_t out_lo, uint8_t out_hi) {
    if(in_lo == in_hi)
        return out_lo;

    if(in_lo < in_hi) {
        if(x <= in_lo)
            return out_lo;
        if(x >= in_hi)
            return out_hi;
    } else {
        if(x >= in_lo)
            return out_lo;
        if(x <= in_hi)
            return out_hi;
    }

    int32_t in_span = (int32_t)in_hi - (int32_t)in_lo;
    int32_t out_span = (int32_t)out_hi - (int32_t)out_lo;
    int32_t dx = (int32_t)x - (int32_t)in_lo;
    int32_t y = (int32_t)out_lo + (dx * out_span) / in_span;

    if(y < 0)
        return 0u;
    if(y > 255)
        return 255u;
    return (uint8_t)y;
}

/**
 * @brief Return 255 - x.
 */
static inline uint8_t u8_inv(uint8_t x) {
    return (uint8_t)(255u - x);
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
