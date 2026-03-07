# 01 - Public API

## API Surface

From `include/LedController.hpp`:

- `init()`
- `write_channel(ch_idx, data)`
- `write_frame(frame)`
- `show()`
- `deinit()`
- `fill(color)`
- `black_out()`
- `print_buffer()`

## Channel Routing Contract

Logical ranges:

- PCA channels: `0 .. LD_BOARD_PCA9955B_CH_NUM - 1`
- WS channels: `LD_BOARD_PCA9955B_CH_NUM .. LD_BOARD_PCA9955B_CH_NUM + LD_BOARD_WS2812B_NUM - 1`

Write contract:

- PCA write consumes one `grb8_t` entry.
- WS write consumes an array for that strip (`pixel_num` entries).

## Behavior Notes

- `write_frame()` writes all channels and returns on first failure.
- `fill()` only updates staged buffers.
- `black_out()` is `fill(GRB_BLACK)` then `show()`.
- `show()` attempts all devices and returns last observed error.
