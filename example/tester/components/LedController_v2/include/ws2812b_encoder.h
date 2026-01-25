#pragma once

#include "driver/rmt_encoder.h"

#include "BoardConfig.h"

/**
 * @brief Create a composite RMT encoder for WS2812B pixel driving.
 *
 * Allocates the encoder container, binds the RMT callback interface, initializes
 * a byte waveform encoder (bit0/bit1 timing) and a copy encoder (for reset symbol),
 * and precomputes the WS2812B reset symbol.
 *
 * Resulting encoder instance resides on heap and must be deleted via
 * rmt_encoder->del() or rmt_del_encoder().
 *
 * @param ret_encoder  Pointer to receive the created RMT encoder handle
 * @return ESP_OK on success, or driver/memory error
 */
esp_err_t rmt_new_encoder(rmt_encoder_handle_t* ret_encoder);
