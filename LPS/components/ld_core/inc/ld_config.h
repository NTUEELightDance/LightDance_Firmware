#pragma once

/**
 * @file ld_config.h
 * @brief Project-wide compile-time switches and runtime limits.
 *
 * This header is intentionally lightweight and shared by multiple components.
 * Keep values deterministic and avoid side effects in macro definitions.
 */

/* Feature toggles */
#define LD_CFG_ENABLE_SD 1
#define LD_CFG_ENABLE_BT 1
#define LD_CFG_ENABLE_LOGGER 0

/* Per-channel max brightness for PCA9955B path (0..255). */
#define LD_CFG_PCA9955B_MAX_BRIGHTNESS_R 210
#define LD_CFG_PCA9955B_MAX_BRIGHTNESS_G 200
#define LD_CFG_PCA9955B_MAX_BRIGHTNESS_B 255

/* Global max brightness for WS2812B path (0..255). */
#define LD_CFG_WS2812B_MAX_BRIGHTNESS 50

/* I2C configuration */
#define LD_CFG_I2C_FREQ_HZ 400000
#define LD_CFG_I2C_TIMEOUT_MS 2

/* RMT configuration */
#define LD_CFG_RMT_TIMEOUT_MS 10

/* Behavior controls */
#define LD_CFG_IGNORE_DRIVER_INIT_FAIL 1
#define LD_CFG_SHOW_TIME_PER_FRAME 0

#define LD_CFG_ENABLE_INTERNAL_PULLUP 1
