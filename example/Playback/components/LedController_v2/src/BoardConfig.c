#include "BoardConfig.h"

const hw_config_t BOARD_HW_CONFIG = {
    .ws2812b_0 = GPIO_NUM_32,
    .ws2812b_1 = GPIO_NUM_25,
    .ws2812b_2 = GPIO_NUM_26,
    .ws2812b_3 = GPIO_NUM_27,
    .ws2812b_4 = GPIO_NUM_19,
    .ws2812b_5 = GPIO_NUM_18,
    .ws2812b_6 = GPIO_NUM_5,
    .ws2812b_7 = GPIO_NUM_17,

    .pca9955b_0 = 0x1f,
    .pca9955b_1 = 0x20,
    .pca9955b_2 = 0x22,
    .pca9955b_3 = 0x23,
    .pca9955b_4 = 0x5b,
    .pca9955b_5 = 0x5c,
    .pca9955b_6 = 0x1f,
    .pca9955b_7 = 0x20,
};

ch_info_t ch_info;