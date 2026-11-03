# 00 - Overview

## Purpose

`Messenger` is the BLE command ingress and scheduling layer for LightDance. It
turns connectionless BLE advertisements into timed calls to `Player` and
application-level system commands.

The implementation is currently contained in `src/bt_receiver.cpp`. The file
also contains the transport, protocol, scheduling, dispatch, ACK, and lifecycle
logic that the planned split will separate.

## Component Boundary

Owned by `Messenger` today:

- Direct ESP32 VHCI interaction
- BLE scan start and stop commands
- LightDance advertisement recognition and parsing
- Player target-mask filtering
- RX queue and synchronization task
- Per-command-ID timers
- `CHECK` response advertising
- Preparation-light behavior for `PLAY`
- Mapping protocol commands to `Player` or `sys_cmd_queue`

Owned elsewhere:

- Playback state machine and rendering: `Player`
- LED hardware output: `LedController`
- Player-ID persistence and startup order: `main`
- Definition and consumption of `sys_cmd_queue`: `main`
- BLE-to-Wi-Fi update transition: `FileDownloader`

## Main Data Flow

```text
BLE advertising report
        |
        v
VHCI receive callback
        |
        v
LightDance payload parser + target filter
        |
        v
s_adv_queue
        |
        v
sync_process_task
        |
        +--> average target timestamp over sync window
        +--> optional red PLAY preparation feedback
        `--> command-ID timer slot
                 |
                 v
          timer_timeout_cb
                 |
                 +--> Player command
                 +--> CHECK ACK task
                 `--> sys_cmd_queue (UPLOAD / RESET)
```

## Runtime Contexts

| Context | Current entry point | Main responsibility |
| --- | --- | --- |
| VHCI receive callback | `host_rcv_pkt` | Pass raw HCI events to the fast parser |
| Receive callback path | `fast_parse_and_trigger` | Parse, filter, timestamp, and enqueue |
| FreeRTOS task | `sync_process_task` | Collect packets, average timing, schedule timers |
| ESP Timer task | `timer_timeout_cb` | Dispatch the scheduled command |
| ESP Timer task | `led_timer_cb` | End PLAY preparation feedback when still in TEST |
| Temporary FreeRTOS task | `send_ack_task` | Stop scan, advertise ACK, then resume scan |
| Application task | `sys_cmd_task` in `main` | Execute upload and reset operations |

Although two callbacks carry `IRAM_ATTR` and the queue operation uses
`xQueueSendFromISR`, this documentation calls the first stage the "VHCI receive
callback path" rather than assuming it is a hardware ISR.

## State Owned by the Current Translation Unit

- Receiver configuration
- RX queue and synchronization-task handle
- Running flag and shared HCI command buffer
- Sixteen action timer slots, indexed by the 4-bit command ID
- Per-slot visual-feedback suppression flags
- One preparation-light timer
- The most recently locked non-`CHECK` command, used by ACK reporting

All of this state is currently file-static in `bt_receiver.cpp`. The refactor
plan keeps one explicit owner for it instead of replacing it with cross-file
globals.

## Dependency Direction Today

`Messenger` directly includes `player.hpp` and calls the `Player` singleton.
`main` includes the Messenger API and supplies `sys_cmd_queue`.
`FileDownloader` calls `bt_receiver_deinit()` before enabling Wi-Fi.

`Player` also currently declares a CMake dependency on `Messenger` and includes
`bt_receiver.h`, even though `player.cpp` does not use a Messenger symbol. This
creates an avoidable bidirectional component dependency and is explicitly left
for a later code change.
