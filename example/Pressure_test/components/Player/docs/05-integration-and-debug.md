# 05 - Integration and Debug

## Integration Checklist

1. Call `Player::getInstance().init()` once at startup.
2. Ensure `LedController` prerequisites are configured (`ch_info`, board config).
3. Ensure frame source is available (`read_frame` path or fallback test path).
4. Use command APIs only after init succeeds.
5. Keep one logical owner for control APIs to simplify sequencing.

## Console Commands

From `src/player_console.cpp`:

- `play`
- `pause`
- `stop`
- `release`
- `test [r g b]`
- `exit`

## Common Failure Points

- `player not ready`: API called before `init()` finished.
- `event queue full`: producer is sending faster than task can drain.
- resource init failures: board config or dependent drivers not ready.
- no visible output: clock not running, state not `PLAYING/TEST`, or downstream LED init failed.

## Suggested Debug Order

1. Verify state via logs around state transitions.
2. Verify update ticks arrive (`NOTIFICATION_UPDATE`).
3. Verify framebuffer path (test mode first, then normal frames).
4. Verify `LedController::show()` logs and channel failures.
