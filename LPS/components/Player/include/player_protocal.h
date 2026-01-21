#pragma once

#include <stdint.h>

#define NOTIFICATION_UPDATE (1 << 0)
#define NOTIFICATION_EVENT (1 << 1)

typedef enum {
    EVENT_PLAY,
    EVENT_TEST,
    EVENT_PAUSE,
    EVENT_RESET,
    EVENT_RELEASE,
    EVENT_LOAD,
    EVENT_EXIT,
} event_t;

struct Event {
    event_t type;

    union {
        uint32_t data;

        struct {
            uint8_t mode;
            uint8_t r;
            uint8_t g;
            uint8_t b;
        } test_data;
    };
};

#include "esp_err.h"

#include "BoardConfig.h"
#include "color.h"

typedef struct {
    grb8_t ws2812b[WS2812B_NUM][WS2812B_MAX_PIXEL_NUM];
    grb8_t pca9955b[PCA9955B_CH_NUM];
} frame_data;

typedef struct {
    uint64_t timestamp;
    bool fade;
    frame_data data;
} table_frame_t;