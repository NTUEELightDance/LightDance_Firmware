#pragma once

#include <math.h>
#include <stdint.h>

inline uint8_t u8_max3(uint8_t a, uint8_t b, uint8_t c) {
    uint8_t m = (a > b) ? a : b;
    return (m > c) ? m : c;
}

inline uint8_t u8_min3(uint8_t a, uint8_t b, uint8_t c) {
    uint8_t m = (a < b) ? a : b;
    return (m < c) ? m : c;
}

// (x * y) / 255 with rounding, inputs 0..255 => output 0..255
inline uint8_t mul255_u8(uint8_t x, uint8_t y) {
    return (uint8_t)(((uint16_t)x * (uint16_t)y + 127) / 255);
}

inline uint8_t lerp_u8(uint8_t start, uint8_t end, uint8_t t) {
    uint16_t val = (uint16_t)start * (255 - t) + (uint16_t)end * t;
    return (uint8_t)((val + 127) / 255);
}