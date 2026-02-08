# 01 - Board Configuration (BoardConfig)

The **BoardConfig** module serves as the Hardware Abstraction Layer (HAL) configuration for the project. It centralizes all hardware-related constants, GPIO mappings, I2C addresses, and channel definitions, ensuring that the driver logic remains decoupled from specific hardware pins.

## 1. Overview

This module defines:
* **Global Constants:** System-wide limits for LED channels, I2C frequencies, and timeouts.
* **Hardware Mapping:** Physical GPIO assignments for RMT (WS2812B) and I2C addresses for LED drivers (PCA9955B).
* **Data Structures:** Unified structures using C `unions` to allow both iterative (array-based) and specific (named) access to hardware resources.

## 2. Configuration Constants

Key macros defining the system limits and bus parameters:

| Macro | Value | Description |
| :--- | :--- | :--- |
| `WS2812B_NUM` | **8** | Number of RMT channels for WS2812B strips. |
| `WS2812B_MAX_PIXEL_NUM` | **100** | Max pixels supported per single strip. |
| `PCA9955B_NUM` | **8** | Number of PCA9955B ICs (Currently disabled). |
| `I2C_FREQ` | **400 kHz** | I2C Bus Frequency. |
| `I2C_TIMEOUT_MS` | **2 ms** | Timeout for I2C transactions. |
| `RMT_TIMEOUT_MS` | **5 ms** | Timeout for RMT operations. |

## 3. Hardware Pinout & Mapping

The configuration is stored in the global `BOARD_HW_CONFIG` structure.

### 3.1 WS2812B (RMT) Pin Mapping
These pins are configured for RMT output to control LED strips.

| Channel | Variable Name | GPIO Pin |
| :--- | :--- | :--- |
| **Ch 0** | `ws2812b_0` | **GPIO 32** |
| **Ch 1** | `ws2812b_1` | **GPIO 25** |
| **Ch 2** | `ws2812b_2` | **GPIO 26** |
| **Ch 3** | `ws2812b_3` | **GPIO 27** |
| **Ch 4** | `ws2812b_4` | **GPIO 19** |
| **Ch 5** | `ws2812b_5` | **GPIO 18** |
| **Ch 6** | `ws2812b_6` | **GPIO 5** |
| **Ch 7** | `ws2812b_7` | **GPIO 17** |

### 3.2 PCA9955B (I2C) Addresses

| IC Index | Address (Hex) |
| :--- | :--- |
| IC 0 | `0x1F` |
| IC 1 | `0x20` |
| IC 2 | `0x22` |
| IC 3 | `0x23` |
| IC 4 | `0x5B` |
| IC 5 | `0x5C` |
| IC 6 | `0x1F` |
| IC 7 | `0x20` |

## 4. Data Structure Design

This module utilizes **Unions** to provide flexible access to hardware configurations. This allows the code to iterate over pins in a loop or access them by name without memory overhead.

### `hw_config_t`
Stores the constant hardware pin/address definitions.
```c
typedef struct {
    union {
        gpio_num_t rmt_pins[WS2812B_NUM]; // Array access for loops
        struct {
            gpio_num_t ws2812b_0;         // Named access
            // ...
            gpio_num_t ws2812b_7;
        };
    };
    // ... PCA9955B union definition
} hw_config_t;
```

### `ch_info_t`
Stores the dynamic pixel count for each channel.

```c
typedef struct {
    union {
        uint16_t pixel_counts[TOTAL_CH]; // Flat array for all channels
        struct {
            uint16_t rmt_strips[WS2812B_NUM];
            uint16_t i2c_leds[PCA9955B_CH_NUM];
        };
    };
} ch_info_t;
```

## 5. Usage Example
```c
#include "BoardConfig.h"

void init_hardware() {
    // Access via array for bulk initialization
    for (int i = 0; i < WS2812B_NUM; i++) {
        gpio_num_t pin = BOARD_HW_CONFIG.rmt_pins[i];
    }
    for (int i = 0; i < PCA9955B_NUM; i++) {
        uint8_t addr = BOARD_HW_CONFIG.i2c_addrs[i];
    }
}
```