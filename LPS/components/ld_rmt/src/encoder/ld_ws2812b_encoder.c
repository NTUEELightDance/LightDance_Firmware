#include "encoder/ld_ws2812b_encoder.h"

#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#include "driver/rmt_encoder.h"
#include "esp_attr.h"
#include "esp_check.h"
#include "esp_err.h"

static const char* TAG = "ld_ws2812b_enc";

#define LD_RMT_MAX_DUR_TICKS (32767U)

static inline uint16_t ns_to_ticks_u16(uint32_t ns, uint32_t resolution_hz) {
    // ticks = round(ns * res / 1e9)
    uint64_t t = (uint64_t)ns * (uint64_t)resolution_hz + 500000000ULL;
    t /= 1000000000ULL;

    if(t == 0)
        t = 1;

    return (uint16_t)t;
}

static inline uint16_t us_to_ticks_u16(uint16_t us, uint32_t resolution_hz) {
    // ticks = us * (res / 1e6)
    uint64_t t = (uint64_t)us * (uint64_t)resolution_hz;
    t /= 1000000ULL;

    if(t == 0)
        t = 1;

    return (uint16_t)t;
}

typedef struct {
    rmt_encoder_t base; /*!< RMT encoder base interface (must be first) */

    rmt_encoder_handle_t bytes_encoder; /*!< Byte->bit waveform encoder (T0/T1) */
    rmt_encoder_handle_t copy_encoder;  /*!< Copy encoder for reset symbol */

    uint8_t state;               /*!< 0: encode bytes, 1: append reset */
    rmt_symbol_word_t reset_sym; /*!< Single low symbol chunk */
} ld_ws2812b_encoder_t;

static IRAM_ATTR size_t ld_ws2812b_encode(rmt_encoder_t* encoder, rmt_channel_handle_t channel, const void* primary_data, size_t data_size, rmt_encode_state_t* ret_state) {
    ld_ws2812b_encoder_t* e = __containerof(encoder, ld_ws2812b_encoder_t, base);

    rmt_encoder_handle_t bytes_encoder = e->bytes_encoder; /*!< Bit waveform encoder for pixel bytes */
    rmt_encoder_handle_t copy_encoder = e->copy_encoder;   /*!< Encoder used to send reset symbol */

    /* Session state from sub-encoder calls */
    rmt_encode_state_t session_state = RMT_ENCODING_RESET; /*!< Tracks progress and errors */

    rmt_encode_state_t state = RMT_ENCODING_RESET; /*!< Output flags to return */
    size_t encoded_symbols = 0;

    switch(e->state) {
        case 0: /*!< Encode pixel bytes */
            encoded_symbols += bytes_encoder->encode(bytes_encoder, channel, primary_data, data_size, &session_state);
            if(session_state & RMT_ENCODING_COMPLETE) {
                e->state = 1; /*!< Move to reset stage */
            }
            if(session_state & RMT_ENCODING_MEM_FULL) {
                state |= RMT_ENCODING_MEM_FULL;
                break; /*!< Abort on RMT symbol memory full */
            }
            /* fall through */

        case 1: /*!< Send WS2812B reset symbol */
            encoded_symbols += copy_encoder->encode(copy_encoder, channel, &e->reset_sym, sizeof(e->reset_sym), &session_state);
            if(session_state & RMT_ENCODING_COMPLETE) {
                e->state = RMT_ENCODING_RESET; /*!< Reset FSM */
                state |= RMT_ENCODING_COMPLETE;
            }
            if(session_state & RMT_ENCODING_MEM_FULL) {
                state |= RMT_ENCODING_MEM_FULL; /*!< Re-arm reset on next call */
            }
            break;
    }

    *ret_state = state;
    return encoded_symbols;
}

static esp_err_t ld_ws2812b_reset(rmt_encoder_t* encoder) {
    if(encoder == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    ld_ws2812b_encoder_t* e = __containerof(encoder, ld_ws2812b_encoder_t, base);

    e->state = 0;

    if(e->bytes_encoder) {
        ESP_RETURN_ON_ERROR(e->bytes_encoder->reset(e->bytes_encoder), TAG, "bytes_encoder reset failed");
    }
    if(e->copy_encoder) {
        ESP_RETURN_ON_ERROR(e->copy_encoder->reset(e->copy_encoder), TAG, "copy_encoder reset failed");
    }

    return ESP_OK;
}

static esp_err_t ld_ws2812b_del(rmt_encoder_t* encoder) {
    if(encoder == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    ld_ws2812b_encoder_t* e = __containerof(encoder, ld_ws2812b_encoder_t, base);

    if(e->bytes_encoder) {
        e->bytes_encoder->del(e->bytes_encoder);
        e->bytes_encoder = NULL;
    }
    if(e->copy_encoder) {
        e->copy_encoder->del(e->copy_encoder);
        e->copy_encoder = NULL;
    }

    free(e);
    return ESP_OK;
}

esp_err_t ld_rmt_new_ws2812b_encoder(const ld_ws2812b_encoder_config_t* cfg, rmt_encoder_handle_t* ret_encoder) {
    ESP_RETURN_ON_FALSE(cfg && ret_encoder, ESP_ERR_INVALID_ARG, TAG, "null arg");
    ESP_RETURN_ON_FALSE(cfg->resolution_hz > 0, ESP_ERR_INVALID_ARG, TAG, "resolution_hz=0");
    ESP_RETURN_ON_FALSE(cfg->reset_us > 0, ESP_ERR_INVALID_ARG, TAG, "reset_us=0");

    *ret_encoder = NULL;

    ld_ws2812b_encoder_t* e = (ld_ws2812b_encoder_t*)calloc(1, sizeof(*e));
    ESP_RETURN_ON_FALSE(e, ESP_ERR_NO_MEM, TAG, "no mem");

    // Bind composite encoder callbacks
    e->base.encode = ld_ws2812b_encode;
    e->base.reset = ld_ws2812b_reset;
    e->base.del = ld_ws2812b_del;

    e->bytes_encoder = NULL;
    e->copy_encoder = NULL;
    e->state = 0;

    // ---- Create bytes encoder ----
    rmt_bytes_encoder_config_t bytes_cfg = {
        .bit0 =
            {
                .level0 = 1,
                .duration0 = ns_to_ticks_u16(cfg->t0h_ns, cfg->resolution_hz),
                .level1 = 0,
                .duration1 = ns_to_ticks_u16(cfg->t0l_ns, cfg->resolution_hz),
            },
        .bit1 =
            {
                .level0 = 1,
                .duration0 = ns_to_ticks_u16(cfg->t1h_ns, cfg->resolution_hz),
                .level1 = 0,
                .duration1 = ns_to_ticks_u16(cfg->t1l_ns, cfg->resolution_hz),
            },
    };
    bytes_cfg.flags.msb_first = cfg->msb_first ? 1 : 0;

    esp_err_t err = rmt_new_bytes_encoder(&bytes_cfg, &e->bytes_encoder);
    if(err != ESP_OK) {
        ld_ws2812b_del(&e->base);
        return err;
    }

    // ---- Create copy encoder (for reset symbol) ----
    rmt_copy_encoder_config_t copy_cfg = {};
    err = rmt_new_copy_encoder(&copy_cfg, &e->copy_encoder);
    if(err != ESP_OK) {
        ld_ws2812b_del(&e->base);
        return err;
    }

    // ---- Prepare reset symbol (single low pulse) ----
    uint16_t reset_ticks = us_to_ticks_u16(cfg->reset_us, cfg->resolution_hz);

    e->reset_sym = (rmt_symbol_word_t){
        .level0 = 0,
        .duration0 = reset_ticks,
        .level1 = 0,
        .duration1 = 0,
    };

    *ret_encoder = &e->base;
    return ESP_OK;
}