# 04 - LedController API (`LedController.cpp`)

This document describes the high-level C++ facade used by runtime modules to control both LED backends.

## 1. Purpose

`LedController` unifies:
- WS2812B strip updates (RMT path)
- PCA9955B channel updates (I2C path)

Callers stage pixel data with write APIs, then flush once via `show()`.

## 2. Public API

Defined in `components/LedController/include/LedController.hpp`.

```cpp
esp_err_t init();
esp_err_t write_channel(int ch_idx, const grb8_t* data);
esp_err_t write_frame(const frame_data* frame);
esp_err_t show();
esp_err_t deinit();
esp_err_t fill(grb8_t color);
esp_err_t black_out();
void print_buffer();
```

Compatibility alias:
- `write_buffer(int ch_idx, const grb8_t* data)` calls `write_channel(...)`.

## 3. Channel Routing

Input channel index decides backend:

- PCA range: `0 .. LD_BOARD_PCA9955B_CH_NUM - 1`
  - One logical color per channel (`*data` is consumed)
- WS range:
  - `LD_BOARD_PCA9955B_CH_NUM .. LD_BOARD_PCA9955B_CH_NUM + LD_BOARD_WS2812B_NUM - 1`
  - Pointer must reference strip pixel array for that WS channel

For WS channels, write length uses runtime count:
- `ws2812b_devs[ws_idx].pixel_num`

## 4. Initialization Contract

Before `init()`:
- Set `ch_info.rmt_strips[]`
- Ensure each strip length is valid
- Ensure board mapping (`BOARD_HW_CONFIG`) matches hardware

`init()` performs:
1. Validation and handle reset
2. I2C bus init
3. WS2812B init loop
4. PCA9955B detection/init loop

On failure:
- Calls `deinit()` for cleanup
- Returns error code

## 5. `show()` Execution Order

`show()` uses split-batch flushing:

1. Trigger WS first half
2. Wait WS first half
3. Flush PCA first half
4. Trigger WS second half
5. Wait WS second half
6. Flush PCA second half

Behavior:
- Per-device failures are logged
- Function continues remaining channels
- Returns last error encountered, or `ESP_OK`

## 6. Utility APIs

- `fill(grb8_t color)`
  - writes one color into all WS/PCA local buffers
  - does not push to hardware until `show()`
- `black_out()`
  - equivalent to `fill(GRB_BLACK)` then `show()`
- `print_buffer()`
  - dumps WS buffers through WS driver helper

## 7. Typical Usage

```cpp
#include "LedController.hpp"
#include "esp_check.h"

extern "C" void app_main(void) {
    for(int i = 0; i < LD_BOARD_WS2812B_NUM; ++i) {
        ch_info.rmt_strips[i] = 60;
    }

    LedController leds;
    ESP_ERROR_CHECK(leds.init());

    grb8_t c = grb8(255, 0, 0);
    ESP_ERROR_CHECK(leds.write_channel(0, &c));  // PCA ch0
    ESP_ERROR_CHECK(leds.show());
}
```
