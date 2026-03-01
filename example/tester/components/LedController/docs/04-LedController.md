# 04 - LED Controller (High-Level API)

This module provides a unified **C++ wrapper** (`LedController`) that manages both WS2812B strips and PCA9955B chips. It acts as the central interface for the application layer, abstracting away the differences between the RMT and I2C protocols.

## 1. Overview

* **Unified Addressing:** Treats all LEDs as a continuous list of channels.
* **Protocol Abstraction:** Automatically routes commands to the correct driver (WS2812B or PCA9955B).
* **Parallel Execution:** Interleaves RMT and I2C transmission to maximize throughput and stability.
* **Resource Management:** Handles initialization and cleanup of all hardware resources.

---

## 2. Parallel Execution Strategy

To optimize the frame rate and ensure signal integrity, the `show()` method utilizes a **Split-Batch Strategy**.

### The Interleaved Approach:

1. **Trigger WS2812B (Batch 1):** Starts RMT transmission for the first half of strips (Non-blocking).
2. **Trigger PCA9955B (Batch 1):** Sends I2C data (Blocking). *RMT is transmitting in the background.*
3. **Wait WS2812B:** Ensures the first batch of RMT signals is finished.
4. **Repeat for Batch 2:** Executes the same process for the remaining devices.

> **⚠️ Critical Technical Note (Anti-Glitch):**
> We strictly split the RMT transmission into two batches (e.g., 4 channels then 4 channels) rather than firing all 8 simultaneously.
>
> **Reason:** Testing revealed that triggering **8 RMT channels at the exact same time** on the ESP32 caused signal timing glitches (likely due to internal bus or interrupt contention). Splitting the load eliminates these glitches while maintaining high frame rates by overlapping I2C blocking time with RMT transmission time.

---

## 3. API Reference

### Initialization & Cleanup

> **⚠️ Prerequisite: Configure `ch_info` First**
> You **MUST** set the pixel counts in the global `ch_info` structure *before* calling `init()`. The controller reads these values to allocate the correct buffer size for each strip.

```cpp
LedController controller;

// Allocates resources, initializes I2C bus, and configures all drivers
esp_err_t init(); 

// Frees all resources and turns off LEDs
esp_err_t deinit();
```

### Data Input (`write_buffer`)
```cpp
// ch_idx: Global channel index (0 to TOTAL_CH-1)
// data:   Pointer to pixel data (Standard GRB format)
esp_err_t write_buffer(int ch_idx, uint8_t* data);
```

**Key Logic:**
* **WS2812B:** Passes raw GRB data directly to the strip driver.
* **PCA9955B:** Automatically converts the input **GRB** format to the hardware's expected **RGB** format before writing to the shadow buffer.

### Global Control
```cpp
// Updates physical LEDs for ALL channels (using Split-Batch Strategy)
esp_err_t show();

// Fills EVERY LED (both types) with a specific color
esp_err_t fill(uint8_t r, uint8_t g, uint8_t b);

// Turns off all LEDs immediately (Fill 0,0,0 + Show)
esp_err_t black_out();
```

## 4. Usage Example

```cpp
#include "LedController.hpp"
#include "BoardConfig.h" // Required to access ch_info

void app_main(void) {
    // 1. Setup Pixel Counts (CRITICAL STEP)
    // Define how many LEDs are on each RMT strip before init
    ch_info.rmt_strips[0] = 60;  // Strip 0 has 60 LEDs
    ch_info.rmt_strips[1] = 144; // Strip 1 has 144 LEDs
    // ... set other channels as needed ...

    // 2. Instantiate & Initialize
    LedController leds;
    
    if (leds.init() != ESP_OK) {
        ESP_LOGE("APP", "Init failed");
        return;
    }

    // 3. Set Data
    // WS2812B (e.g., Channel 0)
    uint8_t pixel_data[300 * 3]; 
    memset(pixel_data, 0xFF, sizeof(pixel_data)); // All White
    leds.write_buffer(0, pixel_data);

    // PCA9955B (e.g., Channel 8)
    uint8_t single_led[3] = {0, 255, 0}; // Green (GRB)
    leds.write_buffer(8, single_led);

    // 4. Update Hardware
    // This will execute the split-batch anti-glitch logic automatically
    leds.show();

    // 5. Cleanup (End of program)
    // leds.deinit();
}
```