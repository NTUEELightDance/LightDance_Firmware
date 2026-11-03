# Player Component

The `Player` component is the runtime animation engine for LightDance firmware.
It receives control events, advances playback time, computes frame colors, and pushes
output frames to `LedController`.

This `README` is high level. Detailed implementation notes are under [`docs/`](docs/).

## What This Component Does

- Runs a dedicated FreeRTOS task (`PlayerTask`) as the single execution point.
- Accepts thread-safe external commands: `play`, `pause`, `stop`, `release`, `test`.
- Enforces a finite-state machine for safe transitions.
- Uses `PlayerClock` + GPTimer notifications for periodic updates.
- Uses `FrameBuffer` to generate interpolated frame data.
- Flushes generated frames through `LedController`.

## Public API

Declared in `include/player.hpp`:

- `esp_err_t init()`
- `esp_err_t deinit()`
- `esp_err_t play()`
- `esp_err_t pause()`
- `esp_err_t stop()`
- `esp_err_t release()`
- `esp_err_t test()`
- `esp_err_t test(uint8_t r, uint8_t g, uint8_t b)`
- `esp_err_t exit()`
- `uint8_t getState()`

## Runtime Model

- Event path: public API -> queue -> `processEvent(...)` in player task.
- Update path: GPTimer ISR -> `NOTIFICATION_UPDATE` -> `updateState()`.
- Main states: `UNLOADED`, `READY`, `PLAYING`, `PAUSE`, `TEST`.

## Dependencies

From `components/Player/CMakeLists.txt`:

- `LedController`
- `PT_Reader`
- `driver`
- `esp_driver_gptimer`
- `console`
- `ld_core`

## Folder Layout

```text
components/Player/
|-- include/
|   |-- framebuffer.hpp
|   |-- player.hpp
|   |-- player_clock.h
|   `-- player_protocal.h
|-- src/
|   |-- framebuffer.cpp
|   |-- player.cpp
|   |-- player_clock.cpp
|   |-- player_console.cpp
|   `-- player_fsm.cpp
|-- docs/
|   |-- 00-overview.md
|   |-- 01-public-api.md
|   |-- 02-state-machine.md
|   |-- 03-render-pipeline.md
|   |-- 04-clock-and-task.md
|   `-- 05-integration-and-debug.md
`-- README.md
```

## Documentation Map

- [`docs/00-overview.md`](docs/00-overview.md): component boundaries and ownership.
- [`docs/01-public-api.md`](docs/01-public-api.md): command/event contract and API behavior.
- [`docs/02-state-machine.md`](docs/02-state-machine.md): FSM states, transitions, and side effects.
- [`docs/03-render-pipeline.md`](docs/03-render-pipeline.md): frame loading, interpolation, and output path.
- [`docs/04-clock-and-task.md`](docs/04-clock-and-task.md): metronome, clock, and task scheduling model.
- [`docs/05-integration-and-debug.md`](docs/05-integration-and-debug.md): integration checklist and debug workflow.

## Minimal Example

```cpp
#include "player.hpp"

extern "C" void app_main(void) {
    Player& player = Player::getInstance();
    if (player.init() != ESP_OK) {
        return;
    }

    vTaskDelay(pdMS_TO_TICKS(100));
    player.play();
}
```

