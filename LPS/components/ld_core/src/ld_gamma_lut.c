#include "ld_gamma_lut.h"

#include <math.h>

/**
 * @file ld_gamma_lut.c
 * @brief Gamma LUT generation for all supported LED output paths.
 */

enum {
    LUT_SIZE = 256,
    U8_MAX = 255,
};

/**
 * @brief Map x in [0,255] to y = pow(x/255, gamma) * 255 with rounding.
 */
static uint8_t gamma_u8(uint8_t x, float gamma) {
    if(x == 0u)
        return 0;
    if(x == U8_MAX)
        return U8_MAX;

    /* Fast paths for common gamma values. */
    if(gamma == 1.0f) {
        return x;
    }
    if(gamma == 0.0f) {
        return U8_MAX;
    }

    float xf = (float)x / (float)U8_MAX;
    float yf = powf(xf, gamma) * (float)U8_MAX;

    int yi = (int)(yf + 0.5f);
    if(yi < 0) {
        yi = 0;
    }
    if(yi > U8_MAX) {
        yi = U8_MAX;
    }

    return (uint8_t)yi;
}

/**
 * @brief Fill a single LUT with the given gamma curve.
 */
static void build_lut(uint8_t dst[LUT_SIZE], float gamma) {
    for(int i = 0; i < LUT_SIZE; ++i) {
        dst[i] = gamma_u8((uint8_t)i, gamma);
    }
}

uint8_t GAMMA_OF_R_lut[LUT_SIZE];
uint8_t GAMMA_OF_G_lut[LUT_SIZE];
uint8_t GAMMA_OF_B_lut[LUT_SIZE];

uint8_t GAMMA_LED_R_lut[LUT_SIZE];
uint8_t GAMMA_LED_G_lut[LUT_SIZE];
uint8_t GAMMA_LED_B_lut[LUT_SIZE];

void calc_gamma_lut() {
    build_lut(GAMMA_OF_R_lut, GAMMA_OF_R);
    build_lut(GAMMA_OF_G_lut, GAMMA_OF_G);
    build_lut(GAMMA_OF_B_lut, GAMMA_OF_B);

    build_lut(GAMMA_LED_R_lut, GAMMA_LED_R);
    build_lut(GAMMA_LED_G_lut, GAMMA_LED_G);
    build_lut(GAMMA_LED_B_lut, GAMMA_LED_B);
}
