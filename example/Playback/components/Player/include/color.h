#pragma once

#include <stdint.h>

typedef struct {
    uint8_t green, red, blue;
} pixel_t;

typedef struct __attribute__((packed)) {
    uint8_t g, r, b;
} grb8_t;

typedef struct __attribute__((packed)) {
    uint16_t h;  // h: 0..1535
    uint8_t s, v;
} hsv8_t;

// ============= math helper function =============

static inline uint8_t u8_max3(uint8_t a, uint8_t b, uint8_t c) {
    uint8_t m = (a > b) ? a : b;
    return (m > c) ? m : c;
}

static inline uint8_t u8_min3(uint8_t a, uint8_t b, uint8_t c) {
    uint8_t m = (a < b) ? a : b;
    return (m < c) ? m : c;
}

// (x * y) / 255 with rounding, inputs 0..255 => output 0..255
static inline uint8_t mul255_u8(uint8_t x, uint8_t y) {
    return (uint8_t)(((uint16_t)x * (uint16_t)y + 127) / 255);
}

// (x * y) / 255 with rounding, x up to 255, y up to 255, but return 16-bit if needed
static inline uint16_t mul255_u16(uint16_t x, uint16_t y) {
    return (uint16_t)((x * y + 127) / 255);
}

static inline uint8_t lerp_u8(uint8_t start, uint8_t end, uint8_t t) {
    uint16_t val = (uint16_t)start * (255 - t) + (uint16_t)end * t;
    return (uint8_t)((val + 127) / 255);
}

static inline uint16_t wrap_h_1536(int32_t h) {
    // normalize to [0,1535]
    h %= 1536;
    if(h < 0)
        h += 1536;
    return (uint16_t)h;
}

static inline int16_t shortest_dh_1536(int16_t dh) {
    // map to [-768, +767] for shortest path
    if(dh > 768)
        dh -= 1536;
    if(dh < -768)
        dh += 1536;
    return dh;
}

// ============= color function =============

inline hsv8_t grb_to_hsv_u8(grb8_t in) {
    uint8_t r = in.r, g = in.g, b = in.b;
    uint8_t maxv = u8_max3(r, g, b);
    uint8_t minv = u8_min3(r, g, b);
    uint8_t delta = (uint8_t)(maxv - minv);

    hsv8_t out;
    out.v = maxv;

    if(maxv == 0) {
        out.s = 0;
        out.h = 0;
        return out;
    }

    // S = delta / max
    out.s = (delta == 0) ? 0 : (uint8_t)(((uint16_t)delta * 255 + (maxv / 2)) / maxv);

    if(delta == 0) {
        out.h = 0;  // undefined hue for gray
        return out;
    }

    // Hue in 0..1535 (6*256)
    // Use signed intermediate because (g-b) etc may be negative.
    int16_t h;
    if(maxv == r) {
        // sector 0
        h = (int16_t)(0 + (int32_t)256 * ((int16_t)g - (int16_t)b) / delta);
    } else if(maxv == g) {
        // sector 2
        h = (int16_t)(512 + (int32_t)256 * ((int16_t)b - (int16_t)r) / delta);
    } else {
        // maxv == b, sector 4
        h = (int16_t)(1024 + (int32_t)256 * ((int16_t)r - (int16_t)g) / delta);
    }
    out.h = wrap_h_1536(h);
    return out;
}

inline grb8_t hsv_to_grb_u8(hsv8_t in) {
    uint16_t h = in.h;  // 0..1535
    uint8_t s = in.s;   // 0..255
    uint8_t v = in.v;   // 0..255

    grb8_t out;

    if(s == 0) {
        // gray
        out.r = v;
        out.g = v;
        out.b = v;
        return out;
    }

    uint8_t sector = (uint8_t)((h >> 8) % 6);  // 0..5
    uint8_t f = (uint8_t)(h & 0xFF);           // 0..255

    // p = v*(1-s)
    uint8_t p = mul255_u8(v, 255 - s);

    // q = v*(1 - s*f)
    uint8_t sf = (uint8_t)(((uint16_t)s * f + 127) / 255);
    uint8_t q = mul255_u8(v, 255 - sf);

    // t = v*(1 - s*(1-f))
    uint8_t s1f = (uint8_t)(((uint16_t)s * (255 - f) + 127) / 255);
    uint8_t t = mul255_u8(v, 255 - s1f);

    // ESP_LOGI("color", "sector: %d", sector);

    switch(sector) {
        case 0:
            out.r = v;
            out.g = t;
            out.b = p;
            break;
        case 1:
            out.r = q;
            out.g = v;
            out.b = p;
            break;
        case 2:
            out.r = p;
            out.g = v;
            out.b = t;
            break;
        case 3:
            out.r = p;
            out.g = q;
            out.b = v;
            break;
        case 4:
            out.r = t;
            out.g = p;
            out.b = v;
            break;
        default:  // 5
            out.r = v;
            out.g = p;
            out.b = q;
            break;
    }
    return out;
}

inline grb8_t grb_lerp_hsv_u8(grb8_t start, grb8_t end, uint8_t t) {
    hsv8_t hstart = grb_to_hsv_u8(start);
    // ESP_LOGI("color", "hstart: h: %d, s: %d, v: %d", hstart.h, hstart.s, hstart.v);
    hsv8_t hend = grb_to_hsv_u8(end);
    // ESP_LOGI("color", "hend: h: %d, s: %d, v: %d", hend.h, hend.s, hend.v);

    hsv8_t h;
    h.s = lerp_u8(hstart.s, hend.s, t);
    h.v = lerp_u8(hstart.v, hend.v, t);

    // Hue interpolate with wrap (shortest path)
    // If either side is gray-ish, you may optionally freeze hue to the other.
    if(hstart.s == 0 && hend.s != 0) {
        h.h = hend.h;
    } else if(hend.s == 0 && hstart.s != 0) {
        h.h = hstart.h;
    } else {

        int16_t dh = (int16_t)hend.h - (int16_t)hstart.h;
        dh = shortest_dh_1536(dh);
        int32_t hh = (int32_t)hstart.h + (int32_t)dh * t / 255;
        h.h = wrap_h_1536(hh);
    }

    // ESP_LOGI("color", "h: h: %d, s: %d, v: %d", h.h, h.s, h.v);

    return hsv_to_grb_u8(h);
}

inline grb8_t grb_lerp_u8(grb8_t start, grb8_t end, uint8_t t) {
    grb8_t out;

    out.r = lerp_u8(start.r, end.r, t);
    out.g = lerp_u8(start.g, end.g, t);
    out.b = lerp_u8(start.b, end.b, t);

    return out;
}