#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "pca9955b.h"
#include "ws2812b.h"

#include "config.h"
#include "ld_board.h"
#include "led_ops.h"
#include "led_types.h"

extern "C" void app_main(void) {
    cal_gamma_lut();

    static i2c_master_bus_handle_t bus_handle;
    i2c_bus_init(GPIO_NUM_21, GPIO_NUM_22, &bus_handle);

    bool pca_exist[PCA9955B_NUM];

    for(int i = 0; i < PCA9955B_NUM; i++) {
        pca_exist[i] = false;
        if(i2c_master_probe(bus_handle, BOARD_HW_CONFIG.i2c_addrs[i], 10) == ESP_OK) {
            pca_exist[i] = true;
        }
    }

    static pca9955b_dev_t pca[PCA9955B_NUM];

    for(int i = 0; i < PCA9955B_NUM; i++) {
        if(pca_exist[i]) {
            pca9955b_init(&pca[i], BOARD_HW_CONFIG.i2c_addrs[i], bus_handle);
        }
    }

    static ws2812b_dev_t ws[WS2812B_NUM];
    for(int i = 0; i < WS2812B_NUM; i++) {
        ws2812b_init(&ws[i], BOARD_HW_CONFIG.rmt_pins[i], WS2812B_MAX_PIXEL_NUM);
    }

    uint16_t h = 0;
    while(1) {
        grb8_t color = hsv_to_grb_u8(hsv8(h, 255, 255));

        grb8_t color_led = grb_set_brightness(grb_gamma_u8(color, GAMMA_SET_LED), BRIGHTNESS_SET_LED);
        grb8_t color_of = grb_set_brightness(grb_gamma_u8(color, GAMMA_SET_OF), BRIGHTNESS_SET_OF);

        for(int i = 0; i < WS2812B_NUM / 2; i++) {
            ws2812b_fill(&ws[i], color_led.r, color_led.g, color_led.b);
            ws2812b_show(&ws[i]);
        }

        for(int i = 0; i < WS2812B_NUM / 2; i++) {
            ws2812b_wait_done(&ws[i]);
        }

        for(int i = 0; i < PCA9955B_NUM / 2; i++) {
            if(pca_exist[i]) {
                pca9955b_fill(&pca[i], color_of.r, color_of.g, color_of.b);
                pca9955b_show(&pca[i]);
            }
        }

        for(int i = WS2812B_NUM / 2; i < WS2812B_NUM; i++) {
            ws2812b_fill(&ws[i], color_led.r, color_led.g, color_led.b);
            ws2812b_show(&ws[i]);
        }

        for(int i = WS2812B_NUM / 2; i < WS2812B_NUM; i++) {
            ws2812b_wait_done(&ws[i]);
        }

        for(int i = PCA9955B_NUM / 2; i < PCA9955B_NUM; i++) {
            if(pca_exist[i]) {
                pca9955b_fill(&pca[i], color_of.r, color_of.g, color_of.b);
                pca9955b_show(&pca[i]);
            }
        }

        h = (h + 2) % (6 * 256);

        vTaskDelay(pdMS_TO_TICKS(10));
    }
}