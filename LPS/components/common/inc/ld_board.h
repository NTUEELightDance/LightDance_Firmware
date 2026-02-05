#pragma once

#include <stdint.h>

#include "driver/gpio.h"

#define WS2812B_NUM 8
#define WS2812B_MAX_PIXEL_NUM 100

#define PCA9955B_NUM 8
#define PCA9955B_CH_NUM (5 * PCA9955B_NUM)

typedef struct {
    /** @brief PCA9955B IC I2C addresses for LED channels 0–39 */
    union {
        uint8_t i2c_addrs[PCA9955B_NUM];

        struct {
            uint8_t pca9955b_0;  // ch 0~4
            uint8_t pca9955b_1;  // ch 5~9
            uint8_t pca9955b_2;  // ch 10~14
            uint8_t pca9955b_3;  // ch 15~19
            uint8_t pca9955b_4;  // ch 20~24
            uint8_t pca9955b_5;  // ch 25~29
            uint8_t pca9955b_6;  // ch 30~34
            uint8_t pca9955b_7;  // ch 35~39
        };
    };

    /** @brief WS2812B RMT output GPIO pins for LED strip channels 40–47 */
    union {
        gpio_num_t rmt_pins[WS2812B_NUM];

        struct {
            gpio_num_t ws2812b_0;  // ch40
            gpio_num_t ws2812b_1;  // ch41
            gpio_num_t ws2812b_2;  // ch42
            gpio_num_t ws2812b_3;  // ch43
            gpio_num_t ws2812b_4;  // ch44
            gpio_num_t ws2812b_5;  // ch45
            gpio_num_t ws2812b_6;  // ch46
            gpio_num_t ws2812b_7;  // ch47
        };
    };

} hw_config_t;

typedef struct {
    union {
        uint16_t pixel_counts[WS2812B_NUM + PCA9955B_CH_NUM]; /**< Flat array of all pixel counts */

        struct {
            uint16_t rmt_strips[WS2812B_NUM];   /**< WS2812B strip pixel counts */
            uint16_t i2c_leds[PCA9955B_CH_NUM]; /**< PCA9955B RGB LED pixel counts */
        };
    };
} ch_info_t;

extern const hw_config_t BOARD_HW_CONFIG;
extern ch_info_t ch_info;
