#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "LedController.hpp"
#include "frame.h"
#include "led_ops.h"
#include "pca9955b.hpp"
#include "ws2812b_tx.hpp"

extern "C" LedController_example(void) {
    ld_channel_layout_t layout = {};

    // for(int pix = 0; pix < 5 && pix < PCA9955B_CH_NUM; ++pix) {
    //     layout.pca9955b_pixel_counts[pix] = 1;
    // }

    if(WS2812B_NUM > 0)
        layout.ws2812b_pixel_counts[0] = 30;
    if(WS2812B_NUM > 1)
        layout.ws2812b_pixel_counts[1] = 30;
    if(WS2812B_NUM > 7)
        layout.ws2812b_pixel_counts[7] = 30;

    ld::LedController controller;
    esp_err_t ret = controller.init(layout);
    if(ret != ESP_OK) {
        ESP_LOGE(TAG, "controller.init failed: %s", esp_err_to_name(ret));
        // In many setups you can still run to test partial outputs, but we stop here.
        return;
    }

    controller.deinit();
    controller.init(layout);

    uint32_t t = 0;

    while(true) {
        // PCA: 5 RGB pixels cycling colors
        // frame.pca_pixels[] must match your renamed field; change if needed.
        if(PCA9955B_CH_NUM >= 5) {
            frame.pca_pixels[0] = hsv_to_grb_u8(hsv8(1 * 255 + t, 255, 255));
            frame.pca_pixels[1] = hsv_to_grb_u8(hsv8(2 * 255 + t, 255, 255));
            frame.pca_pixels[2] = hsv_to_grb_u8(hsv8(3 * 255 + t, 255, 255));
            frame.pca_pixels[3] = hsv_to_grb_u8(hsv8(4 * 255 + t, 255, 255));
            frame.pca_pixels[4] = hsv_to_grb_u8(hsv8(5 * 255 + t, 255, 255));
        }

        // WS strips: simple moving pattern
        for(int s = 0; s < WS2812B_NUM; ++s) {
            uint16_t n = layout.ws2812b_pixel_counts[s];
            if(n == 0)
                continue;
            if(n > WS2812B_MAX_PIXEL_NUM)
                n = WS2812B_MAX_PIXEL_NUM;

            for(uint16_t i = 0; i < n; ++i) {
                frame.ws_pixels[s][i] = hsv_to_grb_u8(hsv8((2 * i + t) % (6 * 255), 255, 31));
            }
        }

        // ----------------------------
        // 4) Show
        // ----------------------------
        ret = controller.show(frame);
        if(ret != ESP_OK) {
            ESP_LOGE(TAG, "controller.show returned: %s", esp_err_to_name(ret));
            // Continue to observe recovery/partial behavior
        }

        t += 3;
        vTaskDelay(pdMS_TO_TICKS(25));
    }
}