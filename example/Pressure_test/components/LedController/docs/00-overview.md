# 00 - Overview

## Purpose

`LedController` is the unified LED output facade for LightDance.
It abstracts two backend families:

- WS2812B over RMT
- PCA9955B over I2C

## Ownership

Owned by this component:

- Device lifecycle and resource setup
- Staged write path and flush path
- Logical channel routing across both backends

Not owned by this component:

- Animation timing and interpolation (`Player`)
- Board policy definitions (`ld_core`)
- High-level effect logic

## Data Flow

1. Caller writes staged data (`write_channel` or `write_frame`).
2. Data stays in backend local/shadow buffers.
3. Caller triggers `show()` to push data to physical devices.
