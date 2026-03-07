#include "ld_board.h"

/**
 * @file ld_board.c
 * @brief Board-specific immutable hardware mapping and runtime channel state.
 */

const hw_config_t BOARD_HW_CONFIG = {
    /* PCA9955B addresses, channels 0..39 */
    .i2c_addrs =
        {
            0x1f,
            0x20,
            0x22,
            0x23,
            0x5b,
            0x5c,
            0x5e,
            0x5f,
        },

    /* WS2812B output pins, channels 40..47 */
    .rmt_pins =
        {
            GPIO_NUM_32,
            GPIO_NUM_25,
            GPIO_NUM_26,
            GPIO_NUM_27,
            GPIO_NUM_19,
            GPIO_NUM_18,
            GPIO_NUM_5,
            GPIO_NUM_17,
        },
};

/* Populated during startup from app config / control data. */
ch_info_t ch_info = {0};
