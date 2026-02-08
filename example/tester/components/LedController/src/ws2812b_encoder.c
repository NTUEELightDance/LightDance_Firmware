#include "ws2812b_encoder.h"

#include "esp_attr.h"

#define WS2812B_RESOLUTION 10000000 /*!< RMT tick resolution: 10 MHz (1 tick = 0.1 us) */

#define RMT_BYTES_ENCODER_CONFIG_DEFAULT()                                                            \
    {                                                                                                 \
        .bit0 =                                                                                       \
            {                                                                                         \
                .level0 = 1,                                                                          \
                .duration0 = (uint32_t)(0.4 * (WS2812B_RESOLUTION) / 1000000), /*!< T0H ≈ 0.4 us */   \
                .level1 = 0,                                                                          \
                .duration1 = (uint32_t)(0.85 * (WS2812B_RESOLUTION) / 1000000), /*!< T0L ≈ 0.85 us */ \
            },                                                                                        \
        .bit1 =                                                                                       \
            {                                                                                         \
                .level0 = 1,                                                                          \
                .duration0 = (uint32_t)(0.8 * (WS2812B_RESOLUTION) / 1000000), /*!< T1H ≈ 0.8 us */   \
                .level1 = 0,                                                                          \
                .duration1 = (uint32_t)(0.45 * (WS2812B_RESOLUTION) / 1000000), /*!< T1L ≈ 0.45 us */ \
            },                                                                                        \
        .flags =                                                                                      \
            {                                                                                         \
                .msb_first = 1, /*!< Encode MSB first (GRB order compliance) */                       \
            },                                                                                        \
    }

/*! Reset code length for WS2812B: ≥ 50 us (converted to 10 MHz RMT ticks) */
#define WS2812B_RESET_TICKS (WS2812B_RESOLUTION / 1000000 * 50 / 2)

#define WS2812B_RESET_CODE_DEFAULT()                                     \
    ((rmt_symbol_word_t){                                                \
        .level0 = 0,                      /*!< Hold low during reset */  \
        .duration0 = WS2812B_RESET_TICKS, /*!< Reset duration (ticks) */ \
        .level1 = 0,                                                     \
        .duration1 = WS2812B_RESET_TICKS,                                \
    })

/**
 * @brief Composite RMT encoder container for WS2812B LED driving.
 *
 * Contains sub-encoders for byte encoding and buffer copy, internal state,
 * and a precomputed reset symbol.
 */
typedef struct {
    rmt_encoder_t base;           /*!< RMT encoder base interface */
    rmt_encoder_t* bytes_encoder; /*!< Encoder for bit-level 0/1 waveform (T0H/T0L/T1H/T1L) */
    rmt_encoder_t* copy_encoder;  /*!< Optional encoder for fast buffer copy */
    int state;                    /*!< Internal FSM state of encoder */
    rmt_symbol_word_t reset_code; /*!< RMT reset symbol for WS2812B timing */
} encoder_t;

/**
 * @brief RMT encoder callback to translate raw GRB/RGB buffer into RMT symbols.
 *
 * Internal FSM:
 *   state 0 → encode pixel bytes
 *   state 1 → transmit reset symbol
 *
 * @param rmt_encoder   Pointer to base RMT encoder interface
 * @param rmt_channel   Target RMT TX channel
 * @param buffer        Raw pixel byte buffer
 * @param buffer_size   Size of pixel buffer (bytes)
 * @param ret_state     Pointer to return encoding result flags
 * @return Number of encoded RMT symbols
 */
static IRAM_ATTR size_t
encode(rmt_encoder_t* rmt_encoder, rmt_channel_handle_t rmt_channel, const void* buffer, size_t buffer_size, rmt_encode_state_t* ret_state) {
    /* Cast to local encoder container */
    encoder_t* ws2812b_encoder = __containerof(rmt_encoder, encoder_t, base);

    /* Sub-encoder handles */
    rmt_encoder_handle_t bytes_encoder = ws2812b_encoder->bytes_encoder; /*!< Bit waveform encoder for pixel bytes */
    rmt_encoder_handle_t copy_encoder = ws2812b_encoder->copy_encoder;   /*!< Encoder used to send reset symbol */

    /* Session state from sub-encoder calls */
    rmt_encode_state_t session_state = RMT_ENCODING_RESET; /*!< Tracks progress and errors */

    rmt_encode_state_t state = RMT_ENCODING_RESET; /*!< Output flags to return */
    size_t encoded_symbols = 0;

    switch(ws2812b_encoder->state) {
        case 0: /*!< Encode pixel bytes */
            encoded_symbols += bytes_encoder->encode(bytes_encoder, rmt_channel, buffer, buffer_size, &session_state);
            if(session_state & RMT_ENCODING_COMPLETE) {
                ws2812b_encoder->state = 1; /*!< Move to reset stage */
            }
            if(session_state & RMT_ENCODING_MEM_FULL) {
                state |= RMT_ENCODING_MEM_FULL;
                break; /*!< Abort on RMT symbol memory full */
            }
            /* fall through */

        case 1: /*!< Send WS2812B reset symbol */
            encoded_symbols +=
                copy_encoder->encode(copy_encoder, rmt_channel, &ws2812b_encoder->reset_code, sizeof(ws2812b_encoder->reset_code), &session_state);
            if(session_state & RMT_ENCODING_COMPLETE) {
                ws2812b_encoder->state = RMT_ENCODING_RESET; /*!< Reset FSM */
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

/**
 * @brief Delete a composite RMT encoder instance and its sub-encoders.
 *
 * Deinitializes pixel byte encoder and reset copy encoder, frees heap memory,
 * and returns the last non-OK error (if any). Safe to call multiple times.
 *
 * @param rmt_encoder  Pointer to the base RMT encoder interface to delete
 * @return ESP_OK, or the last RMT sub-encoder delete error
 */
esp_err_t del_encoder(rmt_encoder_t* rmt_encoder) {
    esp_err_t ret = ESP_OK;
    esp_err_t last_error = ESP_OK; /*!< Track last non-OK error */

    if(rmt_encoder == NULL) {
        return ESP_ERR_INVALID_ARG; /*!< Encoder handle must not be NULL */
    }

    /* Recover local container */
    encoder_t* ws2812b_encoder = __containerof(rmt_encoder, encoder_t, base);

    /* Delete pixel byte encoder */
    if(ws2812b_encoder->bytes_encoder) {
        ret = rmt_del_encoder(ws2812b_encoder->bytes_encoder);
        if(ret != ESP_OK) {
            last_error = ret;
        }
    }

    /* Delete reset copy encoder */
    if(ws2812b_encoder->copy_encoder) {
        ret = rmt_del_encoder(ws2812b_encoder->copy_encoder);
        if(ret != ESP_OK) {
            last_error = ret;
        }
    }

    /* Free composite encoder */
    free(ws2812b_encoder);
    return last_error;
}

/**
 * @brief Reset a composite WS2812B RMT encoder to its initial FSM state.
 *
 * Invokes reset on the pixel byte encoder and reset-symbol encoder, and
 * restores the internal encoding FSM to RMT_ENCODING_RESET (state 0).
 *
 * @param rmt_encoder  Pointer to base RMT encoder interface
 * @return ESP_OK
 */
esp_err_t encoder_reset(rmt_encoder_t* rmt_encoder) {
    if(rmt_encoder == NULL) {
        return ESP_ERR_INVALID_ARG; /*!< Encoder must not be NULL */
    }

    /* Recover local encoder container */
    encoder_t* ws2812b_encoder = __containerof(rmt_encoder, encoder_t, base);

    /* Reset sub-encoders if present */
    if(ws2812b_encoder->bytes_encoder) {
        rmt_encoder_reset(ws2812b_encoder->bytes_encoder); /*!< Reset pixel byte waveform encoder */
    }
    if(ws2812b_encoder->copy_encoder) {
        rmt_encoder_reset(ws2812b_encoder->copy_encoder); /*!< Reset WS2812B reset-symbol encoder */
    }

    /* Restore encoding FSM */
    ws2812b_encoder->state = RMT_ENCODING_RESET; /*!< Set to initial encoding state */
    return ESP_OK;
}

esp_err_t rmt_new_encoder(rmt_encoder_handle_t* ret_encoder) {
    esp_err_t ret = ESP_OK;
    if(ret_encoder == NULL) {
        return ESP_ERR_INVALID_ARG; /*!< Output handle must not be NULL */
    }

    *ret_encoder = NULL; /*!< Clear output before allocation */

    /* Allocate composite encoder container */
    encoder_t* ws2812b_encoder = (encoder_t*)calloc(1, sizeof(encoder_t));
    if(!ws2812b_encoder) {
        return ESP_ERR_NO_MEM; /*!< Heap allocation failed */
    }

    /* Bind RMT encoder interface callbacks */
    ws2812b_encoder->base.encode = encode;       /*!< Pixel byte encoding stage callback */
    ws2812b_encoder->base.del = del_encoder;     /*!< Cleanup callback for composite + sub-encoders */
    ws2812b_encoder->base.reset = encoder_reset; /*!< Reset callback to restore encoder FSM */

    /* Initialize sub-encoder pointers */
    ws2812b_encoder->bytes_encoder = NULL; /*!< Will hold bit waveform encoder */
    ws2812b_encoder->copy_encoder = NULL;  /*!< Will hold reset-symbol encoder */

    /* Create pixel byte waveform encoder (bit timing config) */
    rmt_bytes_encoder_config_t rmt_bytes_encoder_config = RMT_BYTES_ENCODER_CONFIG_DEFAULT();
    ret = rmt_new_bytes_encoder(&rmt_bytes_encoder_config, &ws2812b_encoder->bytes_encoder);
    if(ret != ESP_OK) {
        free(ws2812b_encoder);
        return ret;
    }

    /* Create copy encoder for reset symbol transmission */
    rmt_copy_encoder_config_t rmt_copy_encoder_config = {};
    ret = rmt_new_copy_encoder(&rmt_copy_encoder_config, &ws2812b_encoder->copy_encoder);
    if(ret != ESP_OK) {
        rmt_del_encoder(ws2812b_encoder->bytes_encoder);
        free(ws2812b_encoder);
        return ret;
    }

    /* Assign precomputed WS2812B reset symbol */
    ws2812b_encoder->reset_code = WS2812B_RESET_CODE_DEFAULT();

    *ret_encoder = &ws2812b_encoder->base; /*!< Return base interface pointer as encoder handle */
    return ESP_OK;
}
