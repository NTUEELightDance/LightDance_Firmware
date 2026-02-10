#pragma once

#include "ld_frame.h"
#include "pca9955b.h"
#include "ws2812b.h"

class LedController {
  public:
    LedController();
    ~LedController();

    esp_err_t init();
    esp_err_t write_channel(int ch_idx, const grb8_t* data);
    esp_err_t write_frame(const frame_data* frame);

    /* Backward-compatible alias for channel write path. */
    inline esp_err_t write_buffer(int ch_idx, const grb8_t* data) {
        return write_channel(ch_idx, data);
    }

    esp_err_t show();
    esp_err_t deinit();

    esp_err_t fill(grb8_t color);
    esp_err_t black_out();

    void print_buffer();

  private:
    i2c_master_bus_handle_t bus_handle;
    ws2812b_dev_t ws2812b_devs[LD_BOARD_WS2812B_NUM];
    pca9955b_dev_t pca9955b_devs[LD_BOARD_PCA9955B_NUM];
};
