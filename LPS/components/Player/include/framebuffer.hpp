#pragma once

#include "esp_err.h"

#include "board.h"
#include "config.h"
#include "frame.h"
#include "led_ops.h"
#include "led_types.h"

#include "player_protocal.h"

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