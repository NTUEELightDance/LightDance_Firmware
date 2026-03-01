# 02 - State Machine

FSM implementation: `src/player_fsm.cpp`.

## States

- `UNLOADED`
- `READY`
- `PLAYING`
- `PAUSE`
- `TEST`

## Entry Side Effects

- `UNLOADED`
  - calls `releaseResources()`
  - enqueues `EVENT_LOAD` retry path
- `READY`
  - calls `resetPlayback()`
- `PLAYING`
  - calls `startPlayback()`
- `PAUSE`
  - calls `pausePlayback()`
- `TEST`
  - calls `resetPlayback()` then `testPlayback(...)`

## Transition Rules

- `UNLOADED + EVENT_LOAD` -> `READY` when resources acquired
- `READY + EVENT_PLAY` -> `PLAYING`
- `READY + EVENT_TEST` -> `TEST`
- `PLAYING + EVENT_PAUSE` -> `PAUSE`
- `PLAYING + EVENT_STOP` -> `READY`
- `PAUSE + EVENT_PLAY` -> `PLAYING`
- `PAUSE + EVENT_STOP` -> `READY`
- `TEST + EVENT_TEST` -> `TEST` (refresh payload)
- `READY/PLAYING/PAUSE/TEST + EVENT_RELEASE` -> `UNLOADED`

## Update Behavior

`updateState()` renders only in:

- `PLAYING`
- `TEST`

No frame update work is done in `UNLOADED`, `READY`, or `PAUSE`.
