#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "driver/rmt_encoder.h"
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    uint32_t resolution_hz; /*!< RMT resolution used to convert ns/us into ticks */

    // WS2812B timing (nanoseconds)
    uint32_t t0h_ns;
    uint32_t t0l_ns;
    uint32_t t1h_ns;
    uint32_t t1l_ns;

    // Reset time (microseconds)
    uint32_t reset_us;

    // Bit order for bytes encoder
    bool msb_first; /*!< Usually true for WS2812B */
} ld_ws2812b_encoder_config_t;

static inline ld_ws2812b_encoder_config_t ld_ws2812b_encoder_config_default(uint32_t resolution_hz) {
    return (ld_ws2812b_encoder_config_t){
        .resolution_hz = resolution_hz,
        .t0h_ns = 400,
        .t0l_ns = 850,
        .t1h_ns = 800,
        .t1l_ns = 450,
        .reset_us = 100,
        .msb_first = true,
    };
}

esp_err_t ld_rmt_new_ws2812b_encoder(const ld_ws2812b_encoder_config_t* cfg, rmt_encoder_handle_t* ret_encoder);

#ifdef __cplusplus
}
#endif