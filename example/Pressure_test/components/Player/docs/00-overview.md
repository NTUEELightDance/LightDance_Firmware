# 00 - Overview

## Purpose

`Player` is the runtime control plane for LED playback.
It does not implement hardware drivers directly. It coordinates:

- `FrameBuffer` for frame generation
- `PlayerClock` for playback timeline
- `LedController` for hardware flush

## Boundaries

Owned by this component:

- Control API (`play`, `pause`, `stop`, `release`, `test`)
- Event queue and player task lifecycle
- State transitions and resource lifecycle
- Render update loop

Not owned by this component:

- Physical driver internals (`LedController`, WS2812B, PCA9955B)
- Board topology definitions (`ld_core`)
- Frame storage format implementation outside read APIs (`PT_Reader`)

## Main Data Flow

1. External caller sends command API.
2. API packs `Event` and enqueues it.
3. Player task processes event through FSM.
4. Metronome tick triggers update cycle.
5. `FrameBuffer` computes frame for current time.
6. `LedController` writes and shows the frame.
