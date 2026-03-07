# LedController Component

`LedController` is the hardware output layer for LightDance.
It provides one API surface for two physical LED backends:

- WS2812B strips over RMT (`ws2812b.c`)
- PCA9955B constant-current drivers over I2C (`pca9955b.c`)

This README is high level. Detailed implementation notes are under [`docs/`](docs/).

## Scope

This component is responsible for:

- Driver lifecycle (`init`, staged writes, `show`, `deinit`)
- Channel routing from logical channel index to backend driver
- Batched flush strategy in `show()`
- Utility operations (`fill`, `black_out`, debug dump)

This component is not responsible for:

- Animation timeline or interpolation (`Player`)
- Color policy (gamma/brightness/HSV interpolation in upper layers)
- Board ownership rules outside `ld_core` contracts

## Public API

Declared in `include/LedController.hpp`:

- `esp_err_t init()`
- `esp_err_t write_channel(int ch_idx, const grb8_t* data)`
- `esp_err_t write_frame(const frame_data* frame)`
- `esp_err_t show()`
- `esp_err_t deinit()`
- `esp_err_t fill(grb8_t color)`
- `esp_err_t black_out()`
- `void print_buffer()`

Compatibility alias:

- `write_buffer(...)` calls `write_channel(...)`

## Runtime Model

- `write_channel` and `write_frame` stage data in backend buffers.
- `show()` performs the actual hardware transaction.
- `show()` runs split batches for WS and PCA halves.
- `write_frame()` fails fast; `show()` is best-effort and returns last error.

## Dependencies

From `components/LedController/CMakeLists.txt`:

- `driver`
- `esp_timer`
- `freertos`
- `log`
- `ld_core`

## Folder Layout

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
|   |-- 00-overview.md
|   |-- 01-public-api.md
|   |-- 02-topology-and-config.md
|   |-- 03-ws2812b-driver.md
|   |-- 04-pca9955b-driver.md
|   `-- 05-runtime-and-debug.md
`-- README.md
```

## Documentation Map

- [`docs/00-overview.md`](docs/00-overview.md): boundaries, ownership, and data flow.
- [`docs/01-public-api.md`](docs/01-public-api.md): API contracts and channel routing behavior.
- [`docs/02-topology-and-config.md`](docs/02-topology-and-config.md): `ld_core` topology/config contract.
- [`docs/03-ws2812b-driver.md`](docs/03-ws2812b-driver.md): WS2812B RMT driver internals.
- [`docs/04-pca9955b-driver.md`](docs/04-pca9955b-driver.md): PCA9955B I2C driver internals.
- [`docs/05-runtime-and-debug.md`](docs/05-runtime-and-debug.md): runtime semantics, failure behavior, debugging.

## Minimal Example

```cpp
#include "LedController.hpp"

extern "C" void app_main(void) {
    for (int i = 0; i < LD_BOARD_WS2812B_NUM; ++i) {
        ch_info.rmt_strips[i] = 60;
    }

    LedController leds;
    if (leds.init() != ESP_OK) {
        return;
    }

    grb8_t red = grb8(255, 0, 0);
    leds.write_channel(0, &red);
    leds.show();
}
```

