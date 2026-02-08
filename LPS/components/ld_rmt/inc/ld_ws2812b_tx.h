#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "driver/gpio.h"
#include "esp_err.h"

#include "encoder/ld_ws2812b_encoder.h"

#ifdef __cplusplus
extern "C" {
#endif

#define LD_WS2812B_TX_DEFAULT_WAIT_DONE_TIMEOUT_MS 10
#define LD_WS2812B_RMT_RESOLUTION_HZ (10 * 1000 * 1000)

typedef struct ld_ws2812b_tx_t ld_ws2812b_tx_t;
typedef ld_ws2812b_tx_t* ld_ws2812b_tx_handle_t;

typedef struct {
    gpio_num_t gpio_num;             /*!< Output GPIO */
    uint32_t resolution_hz;          /*!< RMT resolution, e.g., 10 MHz */
    uint32_t mem_block_symbols;      /*!< RMT symbol memory blocks, e.g., 64 */
    uint32_t trans_queue_depth;      /*!< TX queue depth, e.g., 4 */
    ld_ws2812b_encoder_config_t enc; /*!< Protocol timing */
} ld_ws2812b_tx_config_t;

static inline ld_ws2812b_tx_config_t ld_ws2812b_tx_config_t_default(void) {
    ld_ws2812b_tx_config_t cfg = {
        .gpio_num = GPIO_NUM_NC,                        // caller must set
        .resolution_hz = LD_WS2812B_RMT_RESOLUTION_HZ,  // 10 MHz (common)
        .mem_block_symbols = 64,                        // common baseline
        .trans_queue_depth = 4,                         // common baseline
        .enc = ld_ws2812b_encoder_config_default(LD_WS2812B_RMT_RESOLUTION_HZ),
    };
    return cfg;
}

esp_err_t ld_ws2812b_tx_init(const ld_ws2812b_tx_config_t* cfg, ld_ws2812b_tx_handle_t* out);
esp_err_t ld_ws2812b_tx_deinit(ld_ws2812b_tx_handle_t h);

esp_err_t ld_ws2812b_tx_transmit_bytes(ld_ws2812b_tx_handle_t h, const uint8_t* bytes, size_t len);
esp_err_t ld_ws2812b_tx_wait_done(ld_ws2812b_tx_handle_t h, uint32_t timeout_ms);

bool ld_ws2812b_tx_busy(ld_ws2812b_tx_handle_t h);
gpio_num_t ld_ws2812b_tx_gpio(ld_ws2812b_tx_handle_t h);

#ifdef __cplusplus
}
#endif