#pragma once

#include "driver/gpio.h"
#include "driver/rmt_encoder.h"
#include "driver/rmt_tx.h"

#include "ld_board.h"
#include "ld_config.h"
#include "ld_led_types.h"
#include "ws2812b_encoder.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    rmt_channel_handle_t rmt_channel; /*!< RMT TX channel handle for LED signal output */
    rmt_encoder_handle_t rmt_encoder; /*!< RMT encoder handle (bit timing + reset symbol) */

    gpio_num_t gpio_num; /*!< Number of the gpio pin */
    uint16_t pixel_num;  /*!< Number of pixels in the LED strip */
    uint8_t buffer[3 * LD_BOARD_WS2812B_MAX_PIXEL_NUM];
} ws2812b_dev_t;

/* Lifecycle */
esp_err_t ws2812b_init(ws2812b_dev_t* strip, gpio_num_t gpio_num, uint16_t pixel_num);
esp_err_t ws2812b_del(ws2812b_dev_t* strip);

/* Buffer update */
esp_err_t ws2812b_write_grb(ws2812b_dev_t* strip, const grb8_t* colors, uint16_t count);
esp_err_t ws2812b_set_pixel(ws2812b_dev_t* strip, int pixel_idx, grb8_t color);
esp_err_t ws2812b_fill(ws2812b_dev_t* strip, grb8_t color);
esp_err_t ws2812b_get_pixel(ws2812b_dev_t* strip, int pixel_idx, grb8_t* out_color);

/* Transmission */
esp_err_t ws2812b_show(ws2812b_dev_t* strip);
esp_err_t ws2812b_wait_done(ws2812b_dev_t* strip);

/* Debug */
esp_err_t ws2812b_print_buffer(ws2812b_dev_t* strip);

#ifdef __cplusplus
}
#endif
