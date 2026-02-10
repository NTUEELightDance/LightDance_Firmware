# 01 - Board And Runtime Configuration

This document describes the configuration contracts used by `LedController`.
The source of truth is in `ld_core`:
- `components/ld_core/inc/ld_board.h`
- `components/ld_core/src/ld_board.c`

## 1. Topology Constants

Defined in `ld_board.h`:

| Macro | Value | Description |
| --- | --- | --- |
| `LD_BOARD_WS2812B_NUM` | `8` | Number of WS2812B strips |
| `LD_BOARD_WS2812B_MAX_PIXEL_NUM` | `100` | Compile-time max pixels per strip |
| `LD_BOARD_I2C_SDA_GPIO` | `GPIO_NUM_21` | I2C SDA pin used by LedController |
| `LD_BOARD_I2C_SCL_GPIO` | `GPIO_NUM_22` | I2C SCL pin used by LedController |
| `LD_BOARD_I2C_PROBE_TIMEOUT_MS` | `100` | I2C probe timeout for PCA detection |
| `LD_BOARD_I2C_GLITCH_IGNORE_CNT` | `7` | I2C glitch filter count |
| `LD_BOARD_WS2812B_RMT_RESOLUTION_HZ` | `10000000` | WS RMT resolution |
| `LD_BOARD_RMT_MEM_BLOCK_SYMBOLS` | `64` | WS RMT memory block symbols |
| `LD_BOARD_RMT_TRANS_QUEUE_DEPTH` | `8` | WS RMT TX queue depth |
| `LD_BOARD_PCA9955B_NUM` | `8` | Number of PCA9955B devices |
| `LD_BOARD_PCA9955B_RGB_PER_IC` | `5` | Logical RGB outputs per PCA device |
| `LD_BOARD_PCA9955B_CH_NUM` | `40` | Total PCA logical channels (`8 * 5`) |

Total logical output channels:
- PCA: `0 .. 39`
- WS: `40 .. 47`

## 2. Hardware Mapping

Defined in `ld_board.c` (`BOARD_HW_CONFIG`).

### 2.1 PCA9955B I2C Addresses

| Device Index | I2C Address |
| --- | --- |
| `0` | `0x1f` |
| `1` | `0x20` |
| `2` | `0x22` |
| `3` | `0x23` |
| `4` | `0x5b` |
| `5` | `0x5c` |
| `6` | `0x5e` |
| `7` | `0x5f` |

### 2.2 WS2812B Output GPIO

| Strip Index | GPIO |
| --- | --- |
| `0` | `GPIO_NUM_32` |
| `1` | `GPIO_NUM_25` |
| `2` | `GPIO_NUM_26` |
| `3` | `GPIO_NUM_27` |
| `4` | `GPIO_NUM_19` |
| `5` | `GPIO_NUM_18` |
| `6` | `GPIO_NUM_5` |
| `7` | `GPIO_NUM_17` |

## 3. Runtime Pixel Metadata (`ch_info`)

`ch_info` is a mutable global initialized to zero at startup:

```c
extern ch_info_t ch_info;
```

Only `ch_info.rmt_strips[]` is consumed directly by `LedController::init()` and write paths.

Contract:
- You must set `ch_info.rmt_strips[i]` before `LedController::init()`.
- Each value must be `<= LD_BOARD_WS2812B_MAX_PIXEL_NUM`.
- Unused strips should be set to `0` (or left at `0`).

Example:

```c
for(int i = 0; i < LD_BOARD_WS2812B_NUM; ++i) {
    ch_info.rmt_strips[i] = 60;
}
```

## 4. Runtime Driver Config (`ld_config.h`)

Settings that affect LedController behavior:

| Macro | Default | Effect |
| --- | --- | --- |
| `LD_CFG_I2C_FREQ_HZ` | `400000` | PCA I2C bus speed |
| `LD_CFG_I2C_TIMEOUT_MS` | `2` | I2C transfer timeout |
| `LD_CFG_RMT_TIMEOUT_MS` | `10` | WS RMT wait timeout |
| `LD_CFG_IGNORE_DRIVER_INIT_FAIL` | `1` | Ignore per-device init failure when enabled |
| `LD_CFG_ENABLE_INTERNAL_PULLUP` | `1` | Enable internal pull-up in I2C config |

## 5. Notes

- `LedController::init()` currently creates I2C bus using `LD_BOARD_I2C_SDA_GPIO` / `LD_BOARD_I2C_SCL_GPIO`.
- If board wiring differs, update the macros in `ld_board.h`.
