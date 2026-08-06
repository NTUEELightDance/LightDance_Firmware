# Messenger Component

`Messenger` receives LightDance commands from BLE advertising packets, aligns
repeated packets to a common execution time, and dispatches the resulting action
to `Player` or the application-level system command queue.

The component uses the ESP32 VHCI interface directly. It does not use a GATT
service and does not open a BLE connection.

This README describes the implementation as it exists today. A behavior-preserving
file split is proposed in [`docs/05-refactor-plan.md`](docs/05-refactor-plan.md),
but that split has not been applied yet.

## Responsibilities

`Messenger` currently owns:

- BLE controller initialization and scan control
- HCI command encoding for scanning and ACK advertising
- BLE advertisement parsing and target-mask filtering
- Synchronization-window averaging and delayed action scheduling
- Dispatch of playback commands to `Player`
- ACK advertising for `CHECK` commands
- Forwarding of `UPLOAD` and `RESET` to `sys_cmd_queue`

It does not own:

- Playback state or LED rendering (`Player`)
- The system command task and queue storage (`main`)
- Wi-Fi download behavior (`FileDownloader`)
- Player-ID persistence (`ld_nvs` in `main`)

## Public API

Declared in `include/bt_receiver.h`:

- `bt_receiver_init(const bt_receiver_config_t*)`
- `bt_receiver_start()`
- `bt_receiver_stop()`
- `bt_receiver_deinit()`

The application must also define the queue declared by the component:

```cpp
QueueHandle_t sys_cmd_queue = nullptr;
```

See [`docs/01-public-api.md`](docs/01-public-api.md) for the current lifecycle,
configuration fields, return behavior, and queue contract.

## Runtime Flow

1. The VHCI receive callback receives a BLE advertising report.
2. The parser accepts the LightDance manufacturer payload and checks its target
   mask.
3. A compact `ble_rx_packet_t` is sent to the FreeRTOS receive queue.
4. The synchronization task averages the absolute target timestamps observed
   during the configured window.
5. One of 16 timer slots schedules the command by its 4-bit command ID.
6. The timer callback dispatches the command to `Player`, starts an ACK task, or
   forwards a system command to `sys_cmd_queue`.

The wire format is documented in
[`docs/02-packet-protocol.md`](docs/02-packet-protocol.md). Timing and task
behavior are documented in
[`docs/03-runtime-pipeline.md`](docs/03-runtime-pipeline.md).

## Minimal Integration

```cpp
#include "bt_receiver.h"
#include "freertos/queue.h"

QueueHandle_t sys_cmd_queue = nullptr;

void init_messenger() {
    sys_cmd_queue = xQueueCreate(10, sizeof(sys_cmd_t));

    const bt_receiver_config_t config = {
        .feedback_gpio_num = -1,
        .manufacturer_id = 0xFFFF,
        .my_player_id = 0,
        .sync_window_us = 500000,
        .queue_size = 20,
    };

    if (bt_receiver_init(&config) == ESP_OK) {
        bt_receiver_start();
    }
}
```

`Player` must be initialized before commands can be dispatched, and the
application should create and consume `sys_cmd_queue` before starting the
receiver.

## Important Lifecycle Note

The current `bt_receiver_deinit()` calls
`esp_bt_mem_release(ESP_BT_MODE_BTDM)`. That release is irreversible until the
next reboot, so the current implementation must not be documented or used as a
same-boot stop/deinit/re-init cycle. `FileDownloader` deinitializes BLE before
switching to Wi-Fi and eventually reboots the device.

## Current Layout

```text
components/Messenger/
|-- CMakeLists.txt
|-- include/
|   `-- bt_receiver.h
|-- src/
|   `-- bt_receiver.cpp
|-- docs/
|   |-- 00-overview.md
|   |-- 01-public-api.md
|   |-- 02-packet-protocol.md
|   |-- 03-runtime-pipeline.md
|   |-- 04-integration-and-debug.md
|   `-- 05-refactor-plan.md
`-- README.md
```

## Documentation Map

- [`docs/00-overview.md`](docs/00-overview.md): scope, boundaries, dependencies,
  and data flow.
- [`docs/01-public-api.md`](docs/01-public-api.md): public types, lifecycle, and
  external queue contract.
- [`docs/02-packet-protocol.md`](docs/02-packet-protocol.md): RX and ACK packet
  layouts, byte order, and command payloads.
- [`docs/03-runtime-pipeline.md`](docs/03-runtime-pipeline.md): callback, queue,
  synchronization, timers, ACK, and visual feedback.
- [`docs/04-integration-and-debug.md`](docs/04-integration-and-debug.md): startup,
  Wi-Fi handoff, diagnostics, and known implementation risks.
- [`docs/05-refactor-plan.md`](docs/05-refactor-plan.md): proposed source split and
  a staged behavior-preserving migration plan.

## Build Dependencies

`components/Messenger/CMakeLists.txt` currently declares:

- `Player`
- `bt`
- `nvs_flash`

The source directly uses `Player`, ESP Bluetooth, ESP Timer, and FreeRTOS APIs.
The current dependency and header-boundary issues are recorded in the refactor
plan rather than changed as part of this documentation update.
