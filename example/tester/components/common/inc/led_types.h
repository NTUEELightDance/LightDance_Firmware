#pragma once

#include <stdint.h>

typedef struct {
    uint8_t g, r, b;
} grb8_t;

typedef struct {
    uint16_t h;  // h: 0..1535
    uint8_t s, v;
} hsv8_t;

static const grb8_t GRB_BLACK = {.g = 0, .r = 0, .b = 0};
static const grb8_t GRB_WHITE = {.g = 255, .r = 255, .b = 255};

static const grb8_t GRB_RED = {.g = 0, .r = 255, .b = 0};
static const grb8_t GRB_GREEN = {.g = 255, .r = 0, .b = 0};
static const grb8_t GRB_BLUE = {.g = 0, .r = 0, .b = 255};

static const grb8_t GRB_YELLOW = {.g = 255, .r = 255, .b = 0};
static const grb8_t GRB_CYAN = {.g = 255, .r = 0, .b = 255};
static const grb8_t GRB_PURPLE = {.g = 0, .r = 128, .b = 255};

static inline grb8_t grb8(uint8_t r, uint8_t g, uint8_t b) {
    return (grb8_t){.g = g, .r = r, .b = b};
}

static const hsv8_t HSV_BLACK = {.h = 0, .s = 0, .v = 0};
static const hsv8_t HSV_WHITE = {.h = 0, .s = 0, .v = 255};

static const hsv8_t HSV_RED = {.h = 0, .s = 255, .v = 255};
static const hsv8_t HSV_GREEN = {.h = 576, .s = 255, .v = 255};  // 96*6
static const hsv8_t HSV_BLUE = {.h = 960, .s = 255, .v = 255};   // 160*6

static const hsv8_t HSV_YELLOW = {.h = 384, .s = 255, .v = 255};   // 64*6
static const hsv8_t HSV_CYAN = {.h = 768, .s = 255, .v = 255};     // 128*6
static const hsv8_t HSV_PURPLE = {.h = 1152, .s = 255, .v = 255};  // 192*6

static inline hsv8_t hsv8(uint16_t h, uint8_t s, uint8_t v) {
    return (hsv8_t){.h = h, .s = s, .v = v};
}