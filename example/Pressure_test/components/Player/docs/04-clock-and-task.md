# 04 - Clock and Task Scheduling

This doc covers `PlayerClock`, `PlayerMetronome`, and `PlayerTask` behavior.

## Clock Model

`PlayerClock` tracks playback timeline in microseconds:

- `accumulated_us`
- `last_start_us`
- states: `UNINIT`, `STOPPED`, `RUNNING`, `PAUSED`

`now_us()` returns:

- `0` when uninitialized
- frozen accumulated time when not running
- accumulated plus wall-time delta when running

## Metronome Model

`PlayerMetronome` uses GPTimer with auto-reload alarms.
ISR callback sends `NOTIFICATION_UPDATE` to player task.

`Player::acquireResources()` currently initializes with:

- `clock.init(true, taskHandle, 1000000 / LD_CFG_PLAYER_FPS)`

Target update rate is configured by `LD_CFG_PLAYER_FPS`.

## Task Loop Model

`Player::Loop()` waits on task notifications and processes in order:

1. Event notifications: drain queue and process FSM events
2. Update notifications: run `updateState()` if still running

This ordering prioritizes control commands over rendering.

