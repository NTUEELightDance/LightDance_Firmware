# Components Overview

This folder contains firmware components used by LightDance.

This overview currently focuses on:
- `ld_core` (shared types/config/foundation)
- `LedController` (hardware output driver facade)

## Architecture (Current Focus)

```text
Application / Player
        |
        v
  LedController
   |         |
   v         v
ws2812b    pca9955b
        ^
        |
      ld_core
```

## `ld_core`

Path:
- `components/ld_core`

Purpose:
- Shared LED data types (`grb8_t`, `hsv8_t`)
- Board-level constants and mappings (`ld_board.h`, `BOARD_HW_CONFIG`, `ch_info`)
- Global behavior/config flags (`ld_config.h`)
- Shared frame payload definitions (`ld_frame.h`)
- Color ops and gamma LUT support

Key files:
- `components/ld_core/inc/ld_board.h`
- `components/ld_core/inc/ld_config.h`
- `components/ld_core/inc/ld_led_types.h`
- `components/ld_core/inc/ld_frame.h`
- `components/ld_core/src/ld_board.c`

## `LedController`

Path:
- `components/LedController`

Purpose:
- Unified LED output API for WS2812B + PCA9955B
- Buffer write APIs (`write_channel`, `write_frame`)
- Flush API (`show`) and utility APIs (`fill`, `black_out`)
- Per-backend low-level driver integration (`ws2812b.c`, `pca9955b.c`)

Public API entry:
- `components/LedController/include/LedController.hpp`

Key source files:
- `components/LedController/src/LedController.cpp`
- `components/LedController/src/ws2812b.c`
- `components/LedController/src/ws2812b_encoder.c`
- `components/LedController/src/pca9955b.c`

## Integration Notes

- `LedController` depends on `ld_core` for:
  - Board constants/macros
  - Channel metadata (`ch_info`)
  - Shared color and frame types
- Before `LedController::init()`, initialize `ch_info.rmt_strips[]` with valid strip lengths.
- Keep board-level constants in `ld_board.h` aligned with real hardware wiring.

## Next

If needed, this overview can be expanded to include `Player`, `PT_Reader`, `Messenger`, and `Logger` in the same format.
