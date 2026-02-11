# 01 - Frame Buffer (Animation Engine)

The **FrameBuffer** class acts as the core animation engine for the player. It is responsible for managing the timeline, fetching keyframes, and performing real-time color interpolation between frames.

## 1. Overview

* **Keyframe System:** Maintains a "Current" frame and a "Next" frame structure.
* **Time-Based Rendering:** Calculates the exact color state for any given millisecond.
* **Interpolation Modes:** Supports both smooth fading (Linear/HSV) and instant switching (Step).
* **Unified Output:** Generates a complete frame buffer containing data for both WS2812B and PCA9955B devices.

---

## 2. Data Structures

### 2.1 `frame_data` (Raw Pixels)
This structure holds the raw color data for the entire system (all strips and chips).

```cpp
typedef struct {
    grb8_t ws2812b[WS2812B_NUM][WS2812B_MAX_PIXEL_NUM]; // RMT Data
    grb8_t pca9955b[PCA9955B_CH_NUM];                   // I2C Data
} frame_data;
```

### 2.2 `table_frame_t` (Keyframe)
Represents a single point in the timeline.

| Member | Type | Description |
| :--- | :--- | :--- |
| **timestamp** | `uint64_t` | Absolute time (in ms) when this frame should be fully displayed. |
| **fade** | `bool` | `true`: Smoothly fade to this frame from the next one.<br>`false`: Instantly switch to next frame when timestamp is reached. |
| **data** | `frame_data` | The target pixel colors. |

---

## 3. Core Logic: The Compute Loop

The `compute(uint64_t time_ms)` method is the heart of the engine. It performs three main tasks:

### 3.1 Timeline Advancement (Swapping)
The buffer checks if the current time has passed the `next` frame's timestamp. If so:
1.  **Swap:** `current` becomes the old `next`.
2.  **Fetch:** `next` is recycled and populated with new data from the storage source.
3.  **Repeat:** Checks again in case multiple frames were skipped (frame drop logic).

### 3.2 Interpolation Factor Calculation
If `current->fade` is true, the engine calculates a progress factor `p` (0-255).

$$p = \frac{time\_ms - current.timestamp}{next.timestamp - current.timestamp} \times 255$$

* **Fade Mode:** `p` transitions linearly from 0 to 255.
* **Step Mode:** `p` stays 0 until the exact moment of the switch, then becomes 255.

### 3.3 Color Blending (HSV)
Using the helper library (`color.h`), the engine performs **HSV Interpolation** between the current and next frame.

> **Note:** HSV interpolation ensures that fading from Red to Green passes through Yellow/Orange, rather than becoming muddy brown (which happens in RGB linear interpolation).

```cpp
// For every pixel:
output_pixel = grb_lerp_hsv_u8(current_pixel, next_pixel, p);
```

---

## 4. API Reference

### Initialization
```cpp
FrameBuffer fb;
// Allocates memory, resets timers, and pre-fetches the first 2 frames.
esp_err_t init(); 
```

### Rendering
```cpp
// Updates the internal buffer based on the provided absolute system time.
void compute(uint64_t time_ms);
```

### Data Access
```cpp
// Returns a pointer to the computed/interpolated frame data.
// Pass this pointer to LedController::write_buffer().
frame_data* get_buffer();
```

---

## 5. Usage Example

The FrameBuffer is typically used inside a high-priority task loop.

```cpp
#include "FrameBuffer.hpp"
#include "esp_timer.h"

FrameBuffer frameBuf;
LedController ledCtrl;

void player_task(void *pvParameters) {
    // 1. Setup
    frameBuf.init();
    ledCtrl.init();
    
    // 2. Main Loop
    while (1) {
        // A. Get current system time (us -> ms)
        uint64_t now = esp_timer_get_time() / 1000;
        
        // B. Calculate colors for this specific moment
        frameBuf.compute(now);
        
        // C. Send data to hardware
        frame_data* pixels = frameBuf.get_buffer();
        
        // Push WS2812B data
        for(int i=0; i<WS2812B_NUM; i++) {
             ledCtrl.write_buffer(i, (uint8_t*)pixels->ws2812b[i]);
        }
        
        // Push PCA9955B data (channels start after WS2812B)
        // ... (Similar logic for I2C) ...

        ledCtrl.show();
        
        vTaskDelay(pdMS_TO_TICKS(10)); // ~100 FPS
    }
}
```

