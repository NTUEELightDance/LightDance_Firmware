# 02 - Topology and Config Contract

`LedController` depends on definitions in `ld_core`.

## Required Topology Inputs

Compile-time macros from `ld_board.h`:

- WS counts and max pixels
- PCA counts and channels per IC
- I2C pin definitions
- RMT/I2C timing-related constants

Runtime metadata:

- `ch_info.rmt_strips[]` must be assigned before `init()`.

## Required Hardware Mapping

From `BOARD_HW_CONFIG` in `ld_board.c`:

- `rmt_pins[]` for WS strips
- `i2c_addrs[]` for PCA devices

## Behavior Switches

From `ld_config.h`:

- `LD_CFG_IGNORE_DRIVER_INIT_FAIL`
- `LD_CFG_I2C_FREQ_HZ`
- `LD_CFG_I2C_TIMEOUT_MS`
- `LD_CFG_RMT_TIMEOUT_MS`
- `LD_CFG_ENABLE_INTERNAL_PULLUP`
- `LD_CFG_SHOW_TIME_PER_FRAME`

## Integration Checklist

1. Verify board macros and mapping.
2. Populate `ch_info.rmt_strips[]`.
3. Confirm I2C wiring and pull-up policy.
4. Choose strict/tolerant init behavior.
