#pragma once
#include <stdint.h>
#include "driver/gpio.h"

/**
 * @brief Number of WS2812B LED strip channels controlled by the firmware.
 */
#define WS2812B_NUM 1

/**
 * @brief Number of Max WS2812B LED per channels controlled by the firmware.
 */
#define WS2812B_MAX_PIXEL_NUM 300

/**
 * @brief Number of PCA9955B constant-current LED driver ICs on the I2C bus.
 */
#define PCA9955B_NUM 0

/**
 * @brief Total number of PCA9955B hardware LED output channels across all driver ICs.
 */
#define PCA9955B_CH_NUM (5 * PCA9955B_NUM)

/**
 * @brief Total number of LED channels (WS2812B + PCA9955B).
 */
#define TOTAL_CH (WS2812B_NUM + PCA9955B_CH_NUM)

/**
 * @brief I2C bus clock frequency in Hertz.
 */
#define I2C_FREQ 400000

/**
 * @brief Maximum time (in milliseconds) allowed for a single I2C transaction.
 */
#define I2C_TIMEOUT_MS 2

/**
 * @brief Maximum time (in milliseconds) allowed for RMT (Remote Control peripheral) operations.
 */
#define RMT_TIMEOUT_MS 10

/**
 * @brief Hardware LED channel configuration.
 *
 * @note Unions provide both array access (iteration/bulk ops) and
 *       named access (channel clarity).
 */
typedef struct {
    /** @brief WS2812B RMT output GPIO pins for LED strip channels 0–7 */
    union {
        gpio_num_t rmt_pins[WS2812B_NUM];

        struct {
            gpio_num_t ws2812b_0;  // ch0
            gpio_num_t ws2812b_1;  // ch1
            gpio_num_t ws2812b_2;  // ch2
            gpio_num_t ws2812b_3;  // ch3
            gpio_num_t ws2812b_4;  // ch4
            gpio_num_t ws2812b_5;  // ch5
            gpio_num_t ws2812b_6;  // ch6
            gpio_num_t ws2812b_7;  // ch7
        };
    };

    /** @brief PCA9955B IC I2C addresses for LED channels 8–47 */
    union {
        uint8_t i2c_addrs[PCA9955B_NUM];

        struct {
            uint8_t pca9955b_0;  // ch 8~12
            uint8_t pca9955b_1;  // ch 13~17
            uint8_t pca9955b_2;  // ch 18~22
            uint8_t pca9955b_3;  // ch 23~27
            uint8_t pca9955b_4;  // ch 28~32
            uint8_t pca9955b_5;  // ch 33~37
            uint8_t pca9955b_6;  // ch 38~42
            uint8_t pca9955b_7;  // ch 43~47
        };
    };

} hw_config_t;

/**
 * @brief LED channel information and pixel count storage.
 *
 * Memory layout:
 * - `rmt_strips[0–7]` → WS2812B strip pixel counts (RMT channels)
 * - `i2c_leds[0–29]` → PCA9955B RGB LED pixel counts (I2C channels)
 */
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
