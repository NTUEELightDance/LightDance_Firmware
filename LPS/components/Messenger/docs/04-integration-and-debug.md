# 04 - Integration and Debug

## Startup Order

The current application initializes Messenger in this order:

1. Configure board/channel data required by the playback stack.
2. Initialize `Player`.
3. Create `sys_cmd_queue` and `sys_cmd_task`.
4. Initialize NVS and load the persisted player ID.
5. Call `bt_receiver_init()`.
6. Call `bt_receiver_start()`.

The application currently accepts player IDs `1..31` from NVS and falls back to
ID `0`. The protocol mask can represent 64 targets, but that wider range is not
used by the current startup validation.

## System Queue Integration

`main/main.cpp` owns the global queue and consumes:

- `UPLOAD`: stop playback as needed, show green update feedback, and start the
  TCP update task.
- `RESET`: flush logs and reboot.
- `UPLOAD_SUCCESS`: turn LEDs off and enqueue `RESET`.

Messenger only produces `UPLOAD` and `RESET`. `FileDownloader` produces
`UPLOAD_SUCCESS`.

## BLE-to-Wi-Fi Handoff

The update task calls `bt_receiver_deinit()` before Wi-Fi initialization. This
is currently a one-way handoff:

```text
BLE active -> Messenger deinit -> BT memory released -> Wi-Fi update -> reboot
```

Do not attempt a same-boot `bt_receiver_init()` after this deinit path. If a
future feature requires switching back to BLE without rebooting, the lifecycle
must be changed so irreversible memory release is a separate operation.

## Useful Logs

The Messenger tag is `BT_RECEIVER`.

| Message fragment | Meaning |
| --- | --- |
| `Sync Task Running` | Synchronization task entered its receive loop |
| `LOCKED` | A command window produced a timer with more than 100 ms remaining |
| `ACTION TRIGGERED` | A command timer fired |
| `ACK START` / `ACK STOPPED` | CHECK response paused scan / finished advertising |
| `Sent System CMD` | UPLOAD or RESET entered the application queue |
| `Failed to send CMD to Queue` | System queue was full; command was dropped |
| `Receiver Started` | Scan setup was sent and sync task creation was requested |
| `Receiver De-initialized` | Teardown calls completed; return values are not checked |

## Suggested Debug Order

1. Confirm `Player::init()` and queue creation completed before Messenger starts.
2. Confirm `Receiver Started` and `Sync Task Running` appear.
3. Check the sender emits AD type `0xFF`, `ad_len == 22`, `FF FF 4C 44`, and a
   target mask containing this player ID.
4. Check `LOCKED`, paying attention to sample count, RSSI, and remaining delay.
5. If no lock occurs, verify sender delay leaves strictly more than 100 ms after
   the sync window.
6. Check `ACTION TRIGGERED` and then Player state-transition logs.
7. For CHECK failures, check whether scan resumes after ACK and whether multiple
   ACK tasks overlap.

## Current Risks and Deferred Cleanup

These are observations about the current code, not changes made by this
documentation work.

### High impact

- The RX parser does not comprehensively validate HCI and AD bounds before
  indexing packet bytes.
- `bt_receiver_init()`, `start()`, `stop()`, and `deinit()` ignore most ESP-IDF
  and task-creation return values, so partial initialization can be reported as
  success.
- `deinit()` releases BTDM memory irreversibly for the current boot.
- Multiple ACK tasks can race while changing shared scan/advertise state.

### Lifecycle and concurrency

- The synchronization task self-deletes but does not clear `s_task_handle`, so a
  later teardown can observe a stale handle after a standalone `stop()`.
- `s_is_running` and other static state cross callback/task contexts without an
  explicit synchronization policy.
- HCI sends do not check `esp_vhci_host_check_send_available()`.
- Queue overflow on the receive path is ignored.

### API and dependency boundary

- `manufacturer_id` and `feedback_gpio_num` are configured but unused.
- `ble_rx_packet_t` is public even though it is internal to the receiver.
- `bt_receiver_callback_t` is declared but unused.
- `sys_cmd_t` and `sys_cmd_queue` are application-domain contracts exposed from
  the Messenger header.
- The public header includes `player.hpp` even though none of its declarations
  use a `Player` type; this unnecessarily exposes C++ and Player dependencies to
  every Messenger caller.
- `Messenger` depends on `Player`, while `Player` also currently declares a
  dependency on `Messenger`.

### Behavioral coupling

- The numeric Player state `4` is embedded in the preparation-light callback.
- Sync-window finalization is duplicated in two code paths.
- ACK packet constants and command value `0x07` are duplicated instead of using
  a single protocol definition.

The initial file split should preserve all observable behavior above. Functional
fixes should be reviewed and tested as separate changes.
