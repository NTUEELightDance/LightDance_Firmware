# 05 - Color Math & Utilities

This header-only library provides essential data structures and mathematical functions for color manipulation. It includes high-performance integer math helpers, RGB-HSV color space conversions, and smart interpolation algorithms necessary for smooth LED animation effects.

## 1. Data Structures

The library uses packed structures to ensure memory alignment matches hardware buffers (like WS2812B) where possible.

### `grb8_t` (Hardware Color)
Represents a color in **Green-Red-Blue** order, matching the native data format of WS2812B LEDs.

```c
typedef struct __attribute__((packed)) {
    uint8_t g; // Green (0-255)
    uint8_t r; // Red   (0-255)
    uint8_t b; // Blue  (0-255)
} grb8_t;
```

### `hsv8_t` (Logical Color)
Represents a color in **Hue-Saturation-Value** model, used for rainbows and logical color shifts.

| Member | Type | Range | Description |
| :--- | :--- | :--- | :--- |
| **h** | `uint16_t` | **0 .. 1535** | Extended precision Hue. <br>mapped as: 6 sectors × 256 steps. |
| **s** | `uint8_t` | 0 .. 255 | Saturation (0 = White/Gray, 255 = Pure Color). |
| **v** | `uint8_t` | 0 .. 255 | Value / Brightness. |

> **Why 0-1535 for Hue?**
> Standard 0-255 Hue often creates visible "steps" in slow rainbows. Using 1536 steps provides ~6x higher precision for smoother animations without the cost of full floating-point math.

---

## 2. Math Helpers (Optimization)

To avoid slow floating-point operations on the ESP32, this library uses integer math approximations.

### Fast Scaling (`mul255`)
Calculates `(x * y) / 255` with correct rounding. This is essential for applying brightness (`v`) to a color channel (`s` or `rgb`).

```c
// Output = (x * y) / 255
uint8_t mul255_u8(uint8_t x, uint8_t y);
```

### Linear Interpolation (`lerp_u8`)
Standard lerp function: $start + (end - start) \times t$.
```c
// t = 0   => returns start
// t = 255 => returns end
uint8_t lerp_u8(uint8_t start, uint8_t end, uint8_t t);
```

---

## 3. Color Space Conversion

### GRB $\leftrightarrow$ HSV
These functions convert between the hardware format and the logical format using integer math.

```c
// Convert Hardware GRB to Logical HSV
hsv8_t grb_to_hsv_u8(grb8_t in);

// Convert Logical HSV to Hardware GRB
grb8_t hsv_to_grb_u8(hsv8_t in);
```

---

## 4. Interpolation Algorithms

One of the most powerful features of this library is the ability to blend colors using different strategies.

### 4.1 RGB Interpolation (`grb_lerp_u8`)
Blends strictly in the RGB linear space.
* **Pros:** Fast, simple.
* **Cons:** Blending Red to Green results in a dark/muddy "Brown" in the middle.

### 4.2 HSV Interpolation (`grb_lerp_hsv_u8`)
Blends in the HSV color space, then converts back to GRB.
* **Pros:** Blending Red to Green produces a vibrant Yellow/Orange.
* **Features:**
    * **Shortest Path Logic:** Automatically wraps around the color wheel (e.g., Hue 1500 to Hue 100 goes forward through 0, not backwards).
    * **White/Black Handling:** Handles saturation boundaries gracefully.

```c
// t = 0..255 (Progress 0% to 100%)
grb8_t grb_lerp_hsv_u8(grb8_t start, grb8_t end, uint8_t t);
```

---

## 5. Usage Example

```c
#include "color_math.h"

void demo_color_blend() {
    grb8_t color_red  = { .g=0, .r=255, .b=0 };
    grb8_t color_blue = { .g=0, .r=0, .b=255 };

    // 1. Simple RGB Blend (Result: Dark Purple)
    grb8_t mid_rgb = grb_lerp_u8(color_red, color_blue, 127);

    // 2. Vibrant HSV Blend (Result: Pink/Magenta via Color Wheel)
    grb8_t mid_hsv = grb_lerp_hsv_u8(color_red, color_blue, 127);
    
    // 3. Create a Rainbow pixel
    hsv8_t hsv_val = { .h = 0, .s = 255, .v = 255 };
    for(int i=0; i<1536; i+=10) {
        hsv_val.h = i;
        grb8_t pixel = hsv_to_grb_u8(hsv_val);
        // ... send pixel to LED ...
    }
}
```