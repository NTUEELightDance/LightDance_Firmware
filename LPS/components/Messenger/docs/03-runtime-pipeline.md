# 03 - Runtime Pipeline

## Receive and Queue Stage

`host_rcv_pkt()` passes each VHCI packet to `fast_parse_and_trigger()`.
The parser:

1. Captures one `esp_timer_get_time()` timestamp for the received HCI event.
2. Finds the first valid, targeted LightDance advertisement.
3. Converts wire milliseconds to internal microseconds.
4. Copies the command into `ble_rx_packet_t`.
5. Enqueues it to `s_adv_queue` with `xQueueSendFromISR()`.
6. Returns after the first accepted report.

The queue decouples the time-sensitive receive path from synchronization,
logging, timer operations, Player calls, and ACK advertising.

## Synchronization Window

`sync_process_task()` collects repeated packets for one command ID. The first
packet opens a window at `window_start_time`; later packets with the same command
ID are accumulated only while:

```text
now < window_start_time + sync_window_us
```

For each sample, the absolute requested execution time is:

```text
sample_target_us = packet.rx_time_us + packet.delay_val
```

When the window expires, or a different command ID arrives, the scheduler uses:

```text
average_target_us = sum(sample_target_us) / sample_count
wait_us           = average_target_us - now
```

The command is scheduled only when `wait_us > 100000`. Commands with 100 ms or
less remaining are silently rejected apart from any surrounding diagnostics.

The current task contains two nearly identical finalization paths: one for a
new command ID and one for a window timeout. Consolidating them is part of the
proposed split, but must not change the strict `> 100000` rule or timing point.

## Command Identity

The synchronization collector groups only by the 4-bit command ID. While a
window is open, a packet with the same ID but different command type or payload
is treated as another timing sample; the first packet's command fields remain
the fields that are eventually scheduled.

A different command ID immediately finalizes the previous window and begins a
new one with the new packet.

## Timer Slots

There are 16 `action_slot_t` entries, directly indexed by command ID. Each slot
owns:

- one `esp_timer` handle
- command type
- target mask
- three payload bytes
- SEEK target time

Scheduling a command first stops the existing timer in that slot, overwrites its
context, and starts it once with the calculated `wait_us`.

When the timer fires, `timer_timeout_cb()` resets the slot's visual-ACK flag and
dispatches according to the command type.

## Player Dispatch

The timer callback maps commands to the `Player` singleton:

- `PLAY`, `PAUSE`, `STOP`, and `RELEASE` call the matching method.
- `TEST 0,0,0` selects the parameterless test mode; any other RGB payload uses
  solid-color test mode.
- `SEEK` calls `Player::seek()` with the microsecond timeline value.
- `CANCEL` stops the target slot named by its first payload byte.

If a canceled slot contains `PLAY`, Messenger also stops the preparation-light
timer and calls `Player::stop()`.

## PLAY Preparation Feedback

When a `PLAY` command is successfully locked, and its preparation duration is
greater than zero, Messenger immediately calls:

```cpp
Player::getInstance().test(255, 0, 0);
```

It then starts a single global preparation-light timer. The feedback is emitted
once per command-ID slot until that slot's action fires or is canceled.

When the preparation timer fires, it calls `Player::stop()` only if the numeric
Player state is `4`, which the current code treats as TEST mode.

## CHECK and ACK

Every successfully scheduled non-`CHECK` action updates a cache containing its
command ID, command type, remaining delay at lock time, and lock timestamp.

When a scheduled `CHECK` fires:

1. Current Player state is read.
2. Remaining time for the cached command is calculated and clamped to zero.
3. Heap storage for ACK parameters is allocated.
4. A temporary `ack_task` is created.
5. The task waits 150 ms, pauses scanning, advertises for 300 ms, resumes
   scanning, frees its parameters, and deletes itself.

The current design has no serialization guard for overlapping ACK tasks, so
multiple `CHECK` commands can manipulate the same controller scan/advertise
state concurrently.

## System Commands

`UPLOAD` and `RESET` are sent from the timer callback to `sys_cmd_queue` with
zero wait. The main application task performs the slow or destructive work, so
those operations do not run in the timer callback.

## Stop and Deinit Interaction

`stop()` clears the running flag and stops scanning and timers. The sync task can
take up to its 10 ms queue wait to observe the flag and self-delete.

`deinit()` removes the remaining resources and permanently releases Bluetooth
controller memory for the current boot. See the integration notes for the
existing Wi-Fi handoff sequence and lifecycle caveats.
