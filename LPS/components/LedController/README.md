# LedController

`LedController` is the unified LED output component for LightDance firmware.
It drives two backends behind one API:
- `WS2812B` strips via RMT (`ws2812b.c`)
- `PCA9955B` constant-current chips via I2C (`pca9955b.c`)

This component is intended for ESP-IDF projects and depends on shared topology/types from `ld_core`.

## Table of Contents

1. [What This Component Owns](#what-this-component-owns)
2. [Directory Layout](#directory-layout)
3. [Channel Model](#channel-model)
4. [Initialization Contract](#initialization-contract)
5. [Quick Start](#quick-start)
6. [Public API](#public-api)
7. [Runtime Behavior](#runtime-behavior)
8. [Logging](#logging)
9. [Troubleshooting](#troubleshooting)
10. [Build Integration](#build-integration)
11. [Related Docs](#related-docs)

## What This Component Owns

This component provides:
- One C++ facade class: `LedController`
- WS2812B driver lifecycle (`init`, buffer write, transmit, teardown)
- PCA9955B driver lifecycle (`init`, shadow buffer write, flush, teardown)
- Unified frame write path (`write_channel`, `write_frame`)
- Unified flush path (`show`)

This component does not provide:
- Animation timeline or interpolation logic (handled by `Player`)
- Board topology definitions (handled by `ld_core/inc/ld_board.h`)
- Color math, gamma, or brightness policy (handled by `ld_core`)

## Directory Layout

```text
components/LedController/
|-- include/
|   |-- LedController.hpp
|   |-- ws2812b.h
|   |-- ws2812b_encoder.h
|   `-- pca9955b.h
|-- src/
|   |-- LedController.cpp
|   |-- ws2812b.c
|   |-- ws2812b_encoder.c
|   `-- pca9955b.c
|-- docs/
|   |-- 01-BoardConfig.md
|   |-- 02-ws2812b.md
|   |-- 03-pca9955b.md
|   `-- 04-LedController.md
`-- CMakeLists.txt
```

## Channel Model

Global channel indices are split by backend:
- `0 .. LD_BOARD_PCA9955B_CH_NUM - 1`: PCA9955B logical RGB channels
- `LD_BOARD_PCA9955B_CH_NUM .. LD_BOARD_PCA9955B_CH_NUM + LD_BOARD_WS2812B_NUM - 1`: WS2812B strips

For PCA channels:
- `dev_idx = ch_idx / LD_BOARD_PCA9955B_RGB_PER_IC`
- `pixel_idx = ch_idx % LD_BOARD_PCA9955B_RGB_PER_IC`

Input color format is always `grb8_t` (GRB order). The PCA driver converts to internal RGB register layout for you.

## Initialization Contract

Before calling `LedController::init()`:

1. Configure board topology in `ld_core` (`BOARD_HW_CONFIG`).
2. Fill `ch_info.rmt_strips[]` with the real pixel count of each WS2812B strip.
3. Ensure each pixel count is `<= LD_BOARD_WS2812B_MAX_PIXEL_NUM`.

Notes:
- `init()` uses board-level I2C pin macros: `LD_BOARD_I2C_SDA_GPIO` / `LD_BOARD_I2C_SCL_GPIO`.
- Behavior on per-device init failure is controlled by `LD_CFG_IGNORE_DRIVER_INIT_FAIL` in `ld_config.h`.

## Quick Start

```cpp
#include "LedController.hpp"
#include "esp_check.h"
#include "esp_log.h"

extern "C" void app_main(void) {
    // 1) Runtime strip lengths (required before init)
    for(int i = 0; i < LD_BOARD_WS2812B_NUM; ++i) {
        ch_info.rmt_strips[i] = 60;
    }

    LedController leds;
    ESP_ERROR_CHECK(leds.init());

    // 2) Write one PCA channel (single pixel)
    grb8_t pca_color = grb8(255, 32, 0);
    ESP_ERROR_CHECK(leds.write_channel(0, &pca_color));

    // 3) Write one WS strip (full strip buffer for that channel)
    static grb8_t strip[LD_BOARD_WS2812B_MAX_PIXEL_NUM];
    for(int i = 0; i < ch_info.rmt_strips[0]; ++i) {
        strip[i] = grb8(0, 64, 255);
    }
    int ws_ch0 = LD_BOARD_PCA9955B_CH_NUM + 0;
    ESP_ERROR_CHECK(leds.write_channel(ws_ch0, strip));

    // 4) Flush to hardware
    ESP_ERROR_CHECK(leds.show());
}
```

## Public API

From `LedController.hpp`:

- `esp_err_t init();`
- `esp_err_t deinit();`
- `esp_err_t write_channel(int ch_idx, const grb8_t* data);`
- `esp_err_t write_frame(const frame_data* frame);`
- `esp_err_t show();`
- `esp_err_t fill(grb8_t color);`
- `esp_err_t black_out();`
- `void print_buffer();` (WS2812B buffer dump)

Behavior summary:
- `write_channel`:
  - PCA channel: expects pointer to one `grb8_t`
  - WS channel: expects pointer to strip GRB buffer (length = configured strip pixel count)
- `write_frame`: writes full PCA + WS payload from `frame_data`
- `show`: flushes staged buffers to all enabled devices

## Runtime Behavior

- `show()` updates devices in split batches to reduce bus/timing pressure:
  - first half WS2812B (trigger + wait), then first half PCA9955B
  - second half WS2812B (trigger + wait), then second half PCA9955B
- On per-device send failure, function continues other channels and returns the last error.
- `black_out()` performs `fill(GRB_BLACK)` then `show()`.

## Logging

ESP-IDF logging tags used by this component:
- `LedController` (`LedController.cpp`)
- `WS2812` (`ws2812b.c`)
- `PCA9955B` (`pca9955b.c`)

Recommended during bring-up:

```c
esp_log_level_set("LedController", ESP_LOG_DEBUG);
esp_log_level_set("WS2812", ESP_LOG_DEBUG);
esp_log_level_set("PCA9955B", ESP_LOG_DEBUG);
```

## Troubleshooting

- `init()` fails early:
  - Check I2C wiring and address list in `BOARD_HW_CONFIG.i2c_addrs`.
  - Verify `ch_info.rmt_strips[]` values are initialized and in range.
- WS channel write fails:
  - Ensure channel index is in WS range:
    `LD_BOARD_PCA9955B_CH_NUM .. LD_BOARD_PCA9955B_CH_NUM + LD_BOARD_WS2812B_NUM - 1`
- Partial output updates:
  - Inspect warnings/errors in `show()`, especially per-channel transmit failures.
- Excessive logs at runtime:
  - Lower log level for component tags to `ESP_LOG_INFO` or `ESP_LOG_WARN`.

## Build Integration

`CMakeLists.txt` registers this component with:
- Sources: `LedController.cpp`, `ws2812b.c`, `ws2812b_encoder.c`, `pca9955b.c`
- Public include dir: `include`
- Required components: `driver`, `esp_timer`, `freertos`, `log`, `ld_core`

## Related Docs

- `components/LedController/docs/01-BoardConfig.md`
- `components/LedController/docs/02-ws2812b.md`
- `components/LedController/docs/03-pca9955b.md`
- `components/LedController/docs/04-LedController.md`
- `components/ld_core/README.md`
