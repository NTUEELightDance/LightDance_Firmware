# 05 - Runtime and Debug

## Runtime Semantics

`show()` order in `LedController.cpp`:

1. trigger WS first half
2. wait WS first half
3. flush PCA first half
4. trigger WS second half
5. wait WS second half
6. flush PCA second half

This scheduling balances bus/transport pressure and keeps progress when some channels fail.

## Error Semantics

- `write_frame`: fails fast
- `show`: best-effort over devices, returns last error
- `init`: cleanup on failure via `deinit()` path

## Concurrency Assumption

No internal mutex in `LedController`.
Use one owner task (recommended: `Player`) or external locking.

## Debug Checklist

1. Confirm `ch_info.rmt_strips[]` is initialized.
2. Confirm I2C pins and addresses in board config.
3. Enable debug logs for `LedController`, `WS2812`, `PCA9955B`.
4. Test `fill()` + `show()` before full frame pipeline.
5. Inspect per-device errors in `show()` logs.
