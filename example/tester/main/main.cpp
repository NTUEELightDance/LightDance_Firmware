#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "LedController.hpp"

#define TEST_PCA9955B_NUM 6
#define TEST_WS2812B_NUM 8
#define TEST_WS2812B_PIXEL_NUM 100

i2c_master_bus_handle_t i2c_bus;

uint8_t i2c_addrs[8] = {
    0x1f,
    0x20,
    0x22,
    0x23,
    0x5b,
    0x5c,
};

bool i2c_enable[8] = {0};
pca9955b_dev_t pca_devs[8];
ws2812b_dev_t ws_devs[8];

uint8_t r[3] = {255, 0, 0};
uint8_t g[3] = {0, 255, 0};
uint8_t b[3] = {0, 0, 255};

extern "C" void app_main(void) {
    i2c_bus_init(GPIO_NUM_21, GPIO_NUM_22, &i2c_bus);

    for(int i = 0; i < TEST_WS2812B_NUM; i++) {
        ws2812b_init(&ws_devs[i], BOARD_HW_CONFIG.rmt_pins[i], TEST_WS2812B_PIXEL_NUM);
    }

    for(int i = 0; i < TEST_PCA9955B_NUM; i++) {
        i2c_enable[i] = false;
        if(i2c_master_probe(i2c_bus, i2c_addrs[i], 100) == ESP_OK) {
            i2c_enable[i] = true;
            pca9955b_init(&pca_devs[i], i2c_addrs[i], i2c_bus);
        }
    }

    int count = 0;

    while(1) {
        for(int i = 0; i < TEST_WS2812B_NUM / 2; i++) {
            ws2812b_fill(&ws_devs[i], r[count % 3], g[count % 3], b[count % 3]);
            ws2812b_show(&ws_devs[i]);
        }
        for(int i = 0; i < TEST_PCA9955B_NUM / 2; i++) {
            if(i2c_enable[i]) {
                pca9955b_fill(&pca_devs[i], r[count % 3], g[count % 3], b[count % 3]);
                pca9955b_show(&pca_devs[i]);
            }
        }
        for(int i = 0; i < TEST_WS2812B_NUM / 2; i++) {
            ws2812b_wait_done(&ws_devs[i]);
        }
        for(int i = TEST_WS2812B_NUM / 2; i < TEST_WS2812B_NUM; i++) {
            ws2812b_fill(&ws_devs[i], r[count % 3], g[count % 3], b[count % 3]);
            ws2812b_show(&ws_devs[i]);
        }
        for(int i = TEST_PCA9955B_NUM / 2; i < TEST_PCA9955B_NUM; i++) {
            if(i2c_enable[i]) {
                pca9955b_fill(&pca_devs[i], r[count % 3], g[count % 3], b[count % 3]);
                pca9955b_show(&pca_devs[i]);
            }
        }
        for(int i = TEST_WS2812B_NUM / 2; i < TEST_WS2812B_NUM; i++) {
            ws2812b_wait_done(&ws_devs[i]);
        }

        count++;

        vTaskDelay(pdMS_TO_TICKS(500));
    }
}