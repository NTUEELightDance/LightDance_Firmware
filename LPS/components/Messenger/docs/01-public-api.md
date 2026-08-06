# 01 - Public API and Lifecycle

## Header

The public API is declared in `include/bt_receiver.h` and places its declarations
inside `extern "C"` when included from C++. The header is not currently usable by
a C translation unit because it directly includes the C++ header `player.hpp`.

## Configuration

`bt_receiver_config_t` is copied into static component state by
`bt_receiver_init()`.

| Field | Unit | Current behavior |
| --- | --- | --- |
| `feedback_gpio_num` | GPIO number | Stored but not read by the implementation |
| `manufacturer_id` | 16-bit value | Stored but not used; RX filtering is hard-coded to bytes `FF FF` |
| `my_player_id` | Bit index | Selects one bit in the 64-bit target mask |
| `sync_window_us` | Microseconds | Duration used to collect repeated command packets |
| `queue_size` | Queue entries | Capacity of the internal `ble_rx_packet_t` queue |

The current implementation validates only that the config pointer is non-null
and that queue creation succeeds. Callers should currently provide a nonzero
queue size and a valid target-mask bit index.

## Lifecycle Functions

### `bt_receiver_init`

Current behavior:

1. Copies the configuration.
2. Creates the RX queue.
3. Creates 16 command timers and one preparation-light timer.
4. Initializes and enables the ESP Bluetooth controller in BLE mode.
5. Registers the VHCI callbacks.

Observed return behavior:

- `ESP_ERR_INVALID_ARG` for a null config pointer.
- `ESP_ERR_NO_MEM` if RX queue creation fails.
- `ESP_OK` otherwise.

Errors returned by timer and Bluetooth-controller setup calls are not currently
propagated or rolled back.

### `bt_receiver_start`

Current behavior:

1. Sends HCI reset and event-mask commands.
2. Configures passive scanning with interval and window `0x000F`.
3. Enables scanning without duplicate filtering.
4. Sets the running flag and creates `sync_process_task` pinned to core 1.

Calling `start()` while already running returns `ESP_OK` without doing more
work. The implementation does not currently reject `start()` before `init()`.

### `bt_receiver_stop`

Current behavior:

- Clears the running flag.
- Sends the HCI command that disables scanning.
- Stops all command timers and the preparation-light timer.

The synchronization task observes the running flag, exits its loop, and deletes
itself. The RX queue, timers, VHCI registration, and controller remain allocated.

### `bt_receiver_deinit`

Current behavior:

- Stops the receiver when marked running.
- Unregisters the VHCI callback.
- Deletes the task, queue, and timers.
- Disables and deinitializes the Bluetooth controller.
- Releases BTDM controller memory.

Because the last step calls `esp_bt_mem_release(ESP_BT_MODE_BTDM)`, reinitializing
Bluetooth in the same boot is not a supported current lifecycle. The existing
application uses this as a one-way handoff to Wi-Fi before reboot.

## Commands

`lps_cmd_t` defines the low nibble of the RX command byte:

| Value | Command | Payload | Dispatch target |
| --- | --- | --- | --- |
| `0x01` | `LPS_CMD_PLAY` | Preparation duration | `Player::play()` |
| `0x02` | `LPS_CMD_PAUSE` | None | `Player::pause()` |
| `0x03` | `LPS_CMD_STOP` | None | `Player::stop()` |
| `0x04` | `LPS_CMD_RELEASE` | None | `Player::release()` |
| `0x05` | `LPS_CMD_TEST` | RGB | `Player::test(...)` |
| `0x06` | `LPS_CMD_CANCEL` | Target command ID | Stops a timer slot |
| `0x07` | `LPS_CMD_CHECK` | None | Starts an ACK advertising task |
| `0x08` | `LPS_CMD_UPLOAD` | None | Sends `UPLOAD` to `sys_cmd_queue` |
| `0x09` | `LPS_CMD_RESET` | None | Sends `RESET` to `sys_cmd_queue` |
| `0x0A` | `LPS_CMD_SEEK` | Timeline position | `Player::seek(...)` |

All commands first pass through synchronization and timer scheduling. They are
not dispatched directly from the receive callback.

## System Command Queue Contract

The header declares:

```cpp
extern QueueHandle_t sys_cmd_queue;
```

The application currently defines the storage in `main/main.cpp`, creates a
queue whose item type is `sys_cmd_t`, and starts `sys_cmd_task` before Messenger.

`Messenger` produces:

- `UPLOAD`
- `RESET`

`FileDownloader` later produces `UPLOAD_SUCCESS` on the same queue. The enum and
global queue therefore serve more than Messenger even though both are currently
declared in `bt_receiver.h`.

If the queue is null, `UPLOAD` and `RESET` are ignored. If it is full, the send
uses zero wait and the command is dropped after an error log.

## Exposed but Internal-Looking Types

`ble_rx_packet_t` and `bt_receiver_callback_t` are currently public declarations.
The packet type is only used by `bt_receiver.cpp`, while the callback typedef is
unused. Moving or removing them would be an API change and is deferred until
after the behavior-preserving source split.
