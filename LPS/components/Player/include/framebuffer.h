#pragma once

#include "esp_err.h"

#include "LedController_v2.hpp"
#include "color.h"

typedef struct {
    grb8_t ws2812b[WS2812B_NUM][WS2812B_MAX_PIXEL_NUM];
    grb8_t pca9955b[PCA9955B_CH_NUM];
} frame_data;

typedef struct {
    uint64_t timestamp;
    bool fade;
    frame_data data;
} table_frame_t;

class FrameBuffer {
  public:
    FrameBuffer();
    ~FrameBuffer();

    esp_err_t init();
    esp_err_t reset();
    esp_err_t deinit();

    void compute(uint64_t time_ms);

    void print_buffer();
    frame_data* get_buffer();

  private:
    table_frame_t frame0, frame1;

    table_frame_t* current;
    table_frame_t* next;

    frame_data buffer;
};

void test_read_frame(table_frame_t* p);

void print_table_frame(const table_frame_t& frame);
void print_frame_data(const frame_data& data);