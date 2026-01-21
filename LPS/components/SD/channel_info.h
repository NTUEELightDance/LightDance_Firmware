#pragma once

#include <stdint.h>

/*
 * Channel layout information for player / driver
 *
 * Channel order:
 *   [0 ... WS2812B_NUM-1]                : WS2812B (RMT strips)
 *   [WS2812B_NUM ... WS2812B_NUM+PCA9955B_CH_NUM-1]
 *                                       : PCA9955B (I2C channels)
 *
 * pixel_counts[i] means:
 *   - WS2812B : number of LEDs in this strip
 *   - PCA9955B: number of logical LEDs (usually 1 per channel)
 */

#ifndef WS2812B_NUM
#define WS2812B_NUM 4
#endif

#ifndef PCA9955B_CH_NUM
#define PCA9955B_CH_NUM 16
#endif

// typedef struct {
//     union {
//         /* flat array form (for generic iteration) */
//         uint16_t pixel_counts[WS2812B_NUM + PCA9955B_CH_NUM];

//         /* grouped form (for readability) */
//         struct {
//             uint16_t rmt_strips[WS2812B_NUM];
//             uint16_t i2c_leds[PCA9955B_CH_NUM];
//         };
//     };
// } ch_info_t;

/*
 * Get current channel information derived from control.dat
 *
 * This function:
 *   - reflects runtime layout
 *   - is stable across frames
 *   - should be called after frame_system_init()
 *
 * Return:
 *   ch_info_t structure with all unused channels set to 0
 */
// ch_info_t get_channel_info(void);
