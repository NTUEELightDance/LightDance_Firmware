# 05 - Behavior-Preserving File Split Plan

## Status

This is a proposed refactor. No Messenger source or build file has been changed
as part of the documentation work that introduced this plan.

## Goals

- Give transport, protocol, scheduling, ACK, and lifecycle code one clear owner.
- Keep the public API and wire protocol unchanged during the initial move.
- Make packet parsing and ACK serialization testable without Bluetooth hardware.
- Avoid new cross-file globals and circular internal dependencies.
- Keep timing-sensitive work out of the receive callback.

## Non-Goals for the Initial Split

- Do not change accepted packet bytes or ACK bytes.
- Do not change timing thresholds, task priorities, stack sizes, core affinity,
  delays, or scan parameters.
- Do not change command-to-Player behavior.
- Do not fix lifecycle, bounds-checking, or concurrency defects in the same
  mechanical refactor.
- Do not remove public declarations until downstream usage is verified.

## Proposed Layout

```text
components/Messenger/
|-- CMakeLists.txt
|-- include/
|   `-- bt_receiver.h              # Existing public API
|-- private/
|   |-- messenger_types.hpp        # Internal packets, action context, constants
|   |-- bt_protocol.hpp            # Pure parse/serialize interfaces
|   |-- bt_hci.hpp                 # VHCI command and radio-mode operations
|   |-- bt_scheduler.hpp           # Scheduler lifecycle and enqueue interface
|   `-- bt_ack.hpp                 # ACK request interface
|-- src/
|   |-- bt_receiver.cpp            # Public lifecycle and top-level ownership
|   |-- bt_protocol.cpp            # RX decode and ACK payload encoding
|   |-- bt_hci.cpp                 # HCI builders, callback adapter, scan/advertise
|   |-- bt_scheduler.cpp           # Queue task, sync window, timers, dispatch
|   `-- bt_ack.cpp                 # CHECK response task and sequencing
|-- docs/
`-- README.md
```

The exact private-header folder name can follow the project's preferred ESP-IDF
convention. It must not be added to `INCLUDE_DIRS`; use `PRIV_INCLUDE_DIRS` so
internal contracts do not become public component API.

## Ownership by File

| File | Owns | Must not own |
| --- | --- | --- |
| `bt_receiver.cpp` | Config validation/copy, component lifecycle, composition of internal modules | Packet offsets, sync algorithm details, Player dispatch switch |
| `bt_protocol.cpp` | Byte constants, RX decode, endian conversion, ACK payload serialization | VHCI calls, queues, timers, Player calls |
| `bt_hci.cpp` | HCI command construction, VHCI registration, scan/advertise operations | LightDance command semantics, Player calls |
| `bt_scheduler.cpp` | RX queue, sync window, timer slots, visual feedback, command dispatch | Raw HCI byte encoding, ACK advertisement byte layout |
| `bt_ack.cpp` | ACK task parameters, CHECK response sequence, scan-to-advertise transition | RX sync-window state, public receiver lifecycle |

## Internal Dependency Direction

```text
public bt_receiver API
          |
          v
bt_receiver.cpp (composition / lifecycle)
    |           |                |
    v           v                v
 bt_hci     bt_scheduler       bt_ack
    |           |                |
    `---------->+<---------------'
                |
                v
           bt_protocol
```

`bt_protocol` should be a leaf with no ESP Bluetooth, FreeRTOS, timer, or Player
dependency. If `bt_hci`, `bt_scheduler`, and `bt_ack` need shared state, they
should receive a pointer/reference to a runtime context owned by
`bt_receiver.cpp`, not declare `extern` file globals.

## Recommended Internal Interfaces

The split should favor narrow operations rather than exposing implementation
state. Examples of responsibilities, not final signatures:

- Protocol: decode one advertisement report into an internal packet; encode ACK
  advertising data.
- HCI: initialize/deinitialize controller, enable/disable scan, advertise one
  prepared payload, register an RX sink.
- Scheduler: create/destroy resources, start/stop task, enqueue a decoded packet,
  expose the cached status required for CHECK.
- ACK: accept an immutable status snapshot and perform one response operation.

The receive callback should not know timer-slot structure. The scheduler should
not know manufacturer-data offsets. ACK serialization should not manipulate the
Player singleton.

## Migration Sequence

### Phase 0 - Characterize Current Behavior

Before moving functions, add tests or captured vectors for:

- valid broadcast and single-target RX packets
- every command-specific payload
- malformed/truncated inputs
- ACK payload bytes
- sync averaging, command-ID change, timeout, and the strict 100 ms cutoff
- repeated PLAY visual feedback and CANCEL behavior
- stop/deinit sequences

Malformed-input tests can record current failures without requiring unsafe
behavior to be preserved; bounds hardening should still be a separately reviewed
change.

### Phase 1 - Extract Protocol

Move byte constants, endian helpers, RX field decoding, and ACK payload building
to `bt_protocol.*`. Keep the callback and queue in the original file. Compare
known packet vectors byte-for-byte.

### Phase 2 - Extract HCI Transport

Move HCI command builders and controller scan/advertise operations to
`bt_hci.*`. Preserve command order and all delays. Keep one top-level owner for
VHCI callback registration.

### Phase 3 - Extract Scheduler

Move queue processing, window state, timer slots, visual feedback, and Player
dispatch to `bt_scheduler.*`. First extract the duplicated "finalize current
window" block inside the original translation unit, verify it, and then move it.

### Phase 4 - Extract ACK Worker

Move ACK task allocation and scan/advertise sequencing to `bt_ack.*`. Preserve
the 150 ms pre-delay, 5/20 ms controller delays, 300 ms advertising duration,
task priority 5, and stack size 4096.

### Phase 5 - Reduce `bt_receiver.cpp`

Leave the public lifecycle functions and explicit runtime composition in the
original file. Update `CMakeLists.txt` only after each new source exists, keeping
all current component dependencies until the move builds cleanly.

### Phase 6 - Functional Cleanup in Separate Changes

After the mechanical split is verified, address one concern per reviewable
change:

1. Add complete packet bounds validation and queue-overflow reporting.
2. Propagate ESP-IDF errors and add rollback for partial initialization.
3. Define a lifecycle state machine and separate reversible shutdown from
   irreversible BT memory release.
4. Serialize ACK operations and controller mode transitions.
5. Replace numeric Player-state coupling with a named contract.
6. Move system command types/queue ownership to an application-domain header or
   inject a callback/queue into Messenger.
7. Remove `player.hpp` from the public Messenger header and include only the
   types actually required by that API.
8. Remove the unused Player-to-Messenger dependency.
9. Decide whether configured manufacturer ID should replace literal `FF FF`.
10. Move internal-only public declarations to private headers.

## Behavior Freeze Checklist

During the initial split, verify these values remain unchanged:

| Behavior | Current value |
| --- | --- |
| Command ID width / slot count | 4 bits / 16 |
| RX AD type and length | `0xFF`, `ad_len == 22` |
| Manufacturer/marker bytes | `FF FF 4C 44` |
| Wire-to-internal time conversion | ms multiplied by 1000 |
| Scan type | Passive (`0x00`) |
| Scan interval/window | `0x000F` / `0x000F` |
| Duplicate filtering | Disabled (`0`) |
| Sync task | stack 4096, priority 5, core 1 |
| Queue receive poll | 10 ms |
| Late-command cutoff | schedule only when `wait_us > 100000` |
| ACK task | stack 4096, priority 5 |
| ACK pre-delay / duration | 150 ms / 300 ms |
| PLAY preparation color | RGB `255,0,0` |
| System queue send | zero wait |

## Build Changes When Implementation Starts

The later implementation change will need to:

- list all new `.cpp` files in `idf_component_register(SRCS ...)`
- add the private header directory through `PRIV_INCLUDE_DIRS`
- keep `include/` as the only public include directory
- verify whether `nvs_flash` is actually required by Messenger
- remove the reverse `Player -> Messenger` dependency only after confirming no
  Player source or public header needs Messenger

## Verification Gate

The source split is complete only when:

- the firmware builds from a clean component dependency graph
- protocol vectors match byte-for-byte
- all commands still dispatch at the same scheduling point
- CHECK still returns the same fields and units
- stop and update/reboot flows still execute in the same order
- no new public header or global state was introduced
- the diff contains moves and interface wiring only; behavior fixes remain in
  follow-up commits
