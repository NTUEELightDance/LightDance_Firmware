#pragma once

#include <stdint.h>

/**
 * @file ld_led_types.h
 * @brief Shared LED color types, constants, and constructors.
 */

/**
 * @brief 8-bit GRB color.
 *
 * Note: field order is GRB (not RGB).
 */
typedef struct {
    uint8_t g, r, b;
} grb8_t;

/**
 * @brief 8-bit HSV color.
 *
 * Hue is encoded in [0, 1535] (6 sectors * 256).
 */
typedef struct {
    uint16_t h; /**< Hue, range 0..1535 */
    uint8_t s, v;
} hsv8_t;

/**
 * @brief Supported LED hardware backends.
 */
typedef enum {
    LED_WS2812B = 0,
    LED_PCA9955B,
} led_type_t;

/* Common GRB constants */
static const grb8_t GRB_BLACK = {.g = 0, .r = 0, .b = 0};
static const grb8_t GRB_WHITE = {.g = 255, .r = 255, .b = 255};

static const grb8_t GRB_RED = {.g = 0, .r = 255, .b = 0};
static const grb8_t GRB_GREEN = {.g = 255, .r = 0, .b = 0};
static const grb8_t GRB_BLUE = {.g = 0, .r = 0, .b = 255};

static const grb8_t GRB_YELLOW = {.g = 255, .r = 255, .b = 0};
static const grb8_t GRB_CYAN = {.g = 255, .r = 0, .b = 255};
static const grb8_t GRB_PURPLE = {.g = 0, .r = 128, .b = 255};

/**
 * @brief Construct a GRB color from RGB input order.
 */
static inline grb8_t grb8(uint8_t r, uint8_t g, uint8_t b) {
    return (grb8_t){.g = g, .r = r, .b = b};
}

/* Common HSV constants */
static const hsv8_t HSV_BLACK = {.h = 0, .s = 0, .v = 0};
static const hsv8_t HSV_WHITE = {.h = 0, .s = 0, .v = 255};

static const hsv8_t HSV_RED = {.h = 0, .s = 255, .v = 255};
static const hsv8_t HSV_GREEN = {.h = 576, .s = 255, .v = 255};  // 96*6
static const hsv8_t HSV_BLUE = {.h = 960, .s = 255, .v = 255};   // 160*6

static const hsv8_t HSV_YELLOW = {.h = 384, .s = 255, .v = 255};   // 64*6
static const hsv8_t HSV_CYAN = {.h = 768, .s = 255, .v = 255};     // 128*6
static const hsv8_t HSV_PURPLE = {.h = 1152, .s = 255, .v = 255};  // 192*6

/**
 * @brief Construct an HSV color.
 *
 * Caller is responsible for keeping h in [0, 1535].
 */
static inline hsv8_t hsv8(uint16_t h, uint8_t s, uint8_t v) {
    return (hsv8_t){.h = h, .s = s, .v = v};
}
