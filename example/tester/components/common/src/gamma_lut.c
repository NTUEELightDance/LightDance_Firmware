#include "gamma_lut.h"

#include <math.h>

// Map x in [0,255] to y = (x/255)^gamma in [0,255]
static inline uint8_t gamma_u8(uint8_t x, float gamma) {
    if(x == 0)
        return 0;
    if(x == 255)
        return 255;

    // Optional fast-paths
    if(gamma == 1.0f)
        return x;
    if(gamma == 0.0f)
        return 255;

    float xf = (float)x / 255.0f;         // 0..1
    float yf = powf(xf, gamma) * 255.0f;  // 0..255

    int yi = (int)(yf + 0.5f);  // rounding
    if(yi < 0)
        yi = 0;
    if(yi > 255)
        yi = 255;
    return (uint8_t)yi;
}

uint8_t GAMMA_OF_R_lut[256];
uint8_t GAMMA_OF_G_lut[256];
uint8_t GAMMA_OF_B_lut[256];

uint8_t GAMMA_LED_R_lut[256];
uint8_t GAMMA_LED_G_lut[256];
uint8_t GAMMA_LED_B_lut[256];

void cal_gamma_lut() {
    for(int i = 0; i < 256; i++) {
        uint8_t x = (uint8_t)i;

        GAMMA_OF_R_lut[i] = gamma_u8(x, GAMMA_OF_R);
        GAMMA_OF_G_lut[i] = gamma_u8(x, GAMMA_OF_G);
        GAMMA_OF_B_lut[i] = gamma_u8(x, GAMMA_OF_B);

        GAMMA_LED_R_lut[i] = gamma_u8(x, GAMMA_LED_R);
        GAMMA_LED_G_lut[i] = gamma_u8(x, GAMMA_LED_G);
        GAMMA_LED_B_lut[i] = gamma_u8(x, GAMMA_LED_B);
    }
}