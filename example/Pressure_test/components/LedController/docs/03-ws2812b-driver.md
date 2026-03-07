# 03 - WS2812B Driver

Implementation: `src/ws2812b.c`, `src/ws2812b_encoder.c`.

## Responsibilities

- Maintain per-strip RMT channel + encoder
- Keep GRB byte buffer
- Encode bytes + reset symbol for WS timing
- Provide staged write and transmit helpers

## Key Structures

- `ws2812b_dev_t`
  - `rmt_channel`, `rmt_encoder`
  - `gpio_num`, `pixel_num`
  - internal buffer in GRB order

## Encoder Pipeline

Composite encoder stages:

1. byte waveform encoding (T0/T1 timing)
2. reset symbol append

Timing derives from `LD_BOARD_WS2812B_RMT_RESOLUTION_HZ`.

## API Semantics

- `ws2812b_init`: validate + allocate + clear-strip transmit
- `ws2812b_write_grb` / `set_pixel` / `fill`: staged buffer changes
- `ws2812b_show`: trigger TX
- `ws2812b_wait_done`: wait TX complete
- `ws2812b_del`: best-effort off + teardown

## Constraints

- `pixel_num` in valid range
- write count <= strip pixel count
- pixel index within bounds
