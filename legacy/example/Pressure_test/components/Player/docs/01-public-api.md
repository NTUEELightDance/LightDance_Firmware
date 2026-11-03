# 01 - Public API and Event Contract

## Public API Surface

Declared in `include/player.hpp`:

- `init()` / `deinit()` / `exit()`
- `play()` / `pause()` / `stop()` / `release()`
- `test()` and `test(r,g,b)`
- `getState()`

All command APIs are asynchronous: they enqueue an event and return.

## Event Types

Defined in `include/player_protocal.h`:

- `EVENT_PLAY`
- `EVENT_TEST`
- `EVENT_PAUSE`
- `EVENT_STOP`
- `EVENT_RELEASE`
- `EVENT_LOAD`
- `EVENT_EXIT`

Payload model:

- Generic `uint32_t data`
- `TestData` (`mode`, `r`, `g`, `b`)

## Delivery Semantics

`sendEvent(...)` guarantees:

- Fails with `ESP_ERR_INVALID_STATE` if task/queue is not ready.
- Fails with `ESP_ERR_TIMEOUT` if queue is full.
- On success, task is notified with `NOTIFICATION_EVENT`.

## API Notes

- `init()` creates task and queue. It must be called first.
- `deinit()` currently routes to `EVENT_EXIT` semantics.
- `test()` without RGB enters breathing test mode.
- `test(r,g,b)` enters solid-color test mode.
