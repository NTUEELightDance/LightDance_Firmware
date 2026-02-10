# common component

`common` is the shared foundation layer for the firmware. It provides:
- LED color types and constants (`GRB/HSV`)
- Color math utilities (convert, interpolate, gamma, brightness scale)
- Board-level hardware mapping (GPIO and I2C addresses)
- Shared frame data structures for render/playback modules

This component does not send data to hardware directly. It defines the common types and helpers used by higher-level modules such as `LedController`, `Player`, and `PT_Reader`.

## Directory

```text
components/common/
|-- inc/
|   |-- config.h
|   |-- led_types.h
|   |-- led_ops.h
|   |-- math_u8.h
|   |-- gamma_lut.h
|   |-- ld_board.h
|   `-- frame.h
|-- src/
|   |-- gamma_lut.c
|   `-- ld_board.c
`-- CMakeLists.txt
```

## Key Structures

- `grb8_t`, `hsv8_t`, `led_type_t` in `components/common/inc/led_types.h`
- `hw_config_t`, `ch_info_t`, `ld_channel_layout_t` in `components/common/inc/ld_board.h`
- `ld_frame_data_t`, `table_frame_t` in `components/common/inc/frame.h`

## Main APIs

From `components/common/inc/led_ops.h`:
- `grb_to_hsv_u8()`, `hsv_to_grb_u8()`
- `grb_lerp_u8()`, `grb_lerp_hsv_u8()`
- `grb_gamma_u8()`
- `grb_set_brightness()`

From `components/common/inc/gamma_lut.h`:
- `calc_gamma_lut()`

## Initialization Order

1. Call `calc_gamma_lut()` early in startup.
2. Configure channel pixel layout (`ch_info` or `ld_channel_layout_t`) before initializing hardware control modules.
3. Perform color/frame processing in upper layers, then pass the output to `LedController`.

## Minimal Example

```c
#include "gamma_lut.h"
#include "ld_board.h"
#include "led_ops.h"

void app_led_prepare(void) {
    calc_gamma_lut();

    ch_info.rmt_strips[0] = 60;
    ch_info.rmt_strips[1] = 60;

    grb8_t c = grb8(255, 80, 20);
    c = grb_gamma_u8(c, LED_WS2812B);
    c = grb_set_brightness(c, LED_WS2812B);
}
```

## Tuning Points

- Feature flags and timeouts: `components/common/inc/config.h`
- Gamma coefficients: `components/common/inc/gamma_lut.h`
- Board pin/address mapping: `components/common/src/ld_board.c`
- Brightness caps: `components/common/inc/config.h`
