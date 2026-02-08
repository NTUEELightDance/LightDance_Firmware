#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define GAMMA_OF_R 2.65f
#define GAMMA_OF_G 2.55f
#define GAMMA_OF_B 2.65f

#define GAMMA_LED_R 1.75f
#define GAMMA_LED_G 2.3f
#define GAMMA_LED_B 2.5f

typedef enum {
    GAMMA_SET_OF = 0,
    GAMMA_SET_LED,
} gamma_set_t;

extern uint8_t GAMMA_OF_R_lut[256];
extern uint8_t GAMMA_OF_G_lut[256];
extern uint8_t GAMMA_OF_B_lut[256];

extern uint8_t GAMMA_LED_R_lut[256];
extern uint8_t GAMMA_LED_G_lut[256];
extern uint8_t GAMMA_LED_B_lut[256];

void cal_gamma_lut();

#ifdef __cplusplus
}
#endif