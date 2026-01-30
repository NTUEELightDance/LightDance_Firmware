#pragma once

#include "BoardConfig.h"
#include "pca9955b_hal.h"
#include "ws2812b_hal.h"

#define SHOW_TIME_PER_FRAME 0

class LedController {
  public:
    LedController();
    ~LedController();

    esp_err_t init();
    esp_err_t write_buffer(int ch_idx, uint8_t* data);
    esp_err_t show();
    esp_err_t deinit();

    esp_err_t fill(uint8_t, uint8_t, uint8_t);
    esp_err_t black_out();

    void print_buffer();

  private:
    i2c_master_bus_handle_t bus_handle;
    ws2812b_dev_t ws2812b_devs[WS2812B_NUM];
    pca9955b_dev_t pca9955b_devs[PCA9955B_NUM];
};
