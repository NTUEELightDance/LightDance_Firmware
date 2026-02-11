#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @file ld_gamma_lut.h
 * @brief Gamma configuration and lookup tables for LED output correction.
 */

/* Gamma parameters for PCA9955B (OF) output path. */
#define GAMMA_OF_R 2.65f
#define GAMMA_OF_G 2.55f
#define GAMMA_OF_B 2.65f

/* Gamma parameters for WS2812B output path. */
#define GAMMA_LED_R 1.75f
#define GAMMA_LED_G 2.3f
#define GAMMA_LED_B 2.5f

/** Gamma LUT for PCA9955B R channel. */
extern uint8_t GAMMA_OF_R_lut[256];
/** Gamma LUT for PCA9955B G channel. */
extern uint8_t GAMMA_OF_G_lut[256];
/** Gamma LUT for PCA9955B B channel. */
extern uint8_t GAMMA_OF_B_lut[256];

/** Gamma LUT for WS2812B R channel. */
extern uint8_t GAMMA_LED_R_lut[256];
/** Gamma LUT for WS2812B G channel. */
extern uint8_t GAMMA_LED_G_lut[256];
/** Gamma LUT for WS2812B B channel. */
extern uint8_t GAMMA_LED_B_lut[256];

/**
 * @brief Build all gamma lookup tables.
 *
 * Call once during startup before using grb_gamma_u8().
 */
void calc_gamma_lut();

#ifdef __cplusplus
}
#endif
