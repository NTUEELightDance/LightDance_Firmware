#pragma once

#include <stdint.h>

#include "driver/gpio.h"

/**
 * @file ld_board.h
 * @brief Board-level LED topology and runtime channel configuration.
 */

#ifdef __cplusplus
extern "C" {
#endif

/** Number of WS2812B strips driven by RMT. */
#define LD_BOARD_WS2812B_NUM 8
/** Per-strip compile-time maximum pixel capacity. */
#define LD_BOARD_WS2812B_MAX_PIXEL_NUM 100

/** Number of PCA9955B chips on the I2C bus. */
#define LD_BOARD_PCA9955B_NUM 8
/** RGB outputs per PCA9955B chip. */
#define LD_BOARD_PCA9955B_RGB_PER_IC 5
/** Total PCA9955B RGB channels. */
#define LD_BOARD_PCA9955B_CH_NUM (LD_BOARD_PCA9955B_RGB_PER_IC * LD_BOARD_PCA9955B_NUM)

/**
 * @brief Immutable hardware mapping for the current board.
 */
typedef struct {
    /** @brief PCA9955B I2C addresses for channels 0..39 (5 channels per IC). */
    union {
        uint8_t i2c_addrs[LD_BOARD_PCA9955B_NUM];

        struct {
            uint8_t pca9955b_0;  /**< channels 0..4 */
            uint8_t pca9955b_1;  /**< channels 5..9 */
            uint8_t pca9955b_2;  /**< channels 10..14 */
            uint8_t pca9955b_3;  /**< channels 15..19 */
            uint8_t pca9955b_4;  /**< channels 20..24 */
            uint8_t pca9955b_5;  /**< channels 25..29 */
            uint8_t pca9955b_6;  /**< channels 30..34 */
            uint8_t pca9955b_7;  /**< channels 35..39 */
        };
    };

    /** @brief WS2812B GPIO pins for strip channels 40..47. */
    union {
        gpio_num_t rmt_pins[LD_BOARD_WS2812B_NUM];

        struct {
            gpio_num_t ws2812b_0;  /**< channel 40 */
            gpio_num_t ws2812b_1;  /**< channel 41 */
            gpio_num_t ws2812b_2;  /**< channel 42 */
            gpio_num_t ws2812b_3;  /**< channel 43 */
            gpio_num_t ws2812b_4;  /**< channel 44 */
            gpio_num_t ws2812b_5;  /**< channel 45 */
            gpio_num_t ws2812b_6;  /**< channel 46 */
            gpio_num_t ws2812b_7;  /**< channel 47 */
        };
    };

} hw_config_t;

/**
 * @brief Runtime channel payload description used by reader/player/controller.
 */
typedef struct {
    union {
        uint16_t pixel_counts[LD_BOARD_WS2812B_NUM + LD_BOARD_PCA9955B_CH_NUM]; /**< Flat view of all channel counts. */

        struct {
            uint16_t rmt_strips[LD_BOARD_WS2812B_NUM];   /**< Pixel count per WS2812B strip. */
            uint16_t i2c_leds[LD_BOARD_PCA9955B_CH_NUM]; /**< Pixel count per PCA9955B RGB channel. */
        };
    };
} ch_info_t;

/** Global immutable board mapping. */
extern const hw_config_t BOARD_HW_CONFIG;
/** Global mutable channel payload configuration. */
extern ch_info_t ch_info;

#ifdef __cplusplus
}
#endif
