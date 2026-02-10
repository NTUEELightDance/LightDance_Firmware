#pragma once

/**
 * @file ld_config.h
 * @brief Project-wide compile-time switches and runtime limits.
 *
 * This header is intentionally lightweight and shared by multiple components.
 * Keep values deterministic and avoid side effects in macro definitions.
 */

/* Feature toggles */
#define SD_ENABLE 1
#define BT_ENABLE 0
#define LOGGER_ENABLE 0

/* Per-channel max brightness for PCA9955B path (0..255). */
#define OF_MAX_BRIGHTNESS_R 210
#define OF_MAX_BRIGHTNESS_G 200
#define OF_MAX_BRIGHTNESS_B 255

/* Global max brightness for WS2812B path (0..255). */
#define LED_MAX_BRIGHTNESS 50

/* I2C configuration */
#define I2C_FREQ 400000
#define I2C_TIMEOUT_MS 2

/* RMT configuration */
#define RMT_TIMEOUT_MS 10

/* Behavior controls */
#define LD_IGNORE_DRIVER_INIT_FAIL 1

#define LD_ENABLE_INTERNAL_PULLUP 1
