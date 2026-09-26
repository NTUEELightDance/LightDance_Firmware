# 02 - Packet Protocol

## Scope

Messenger receives LightDance commands from BLE advertising reports and sends a
short ACK advertisement in response to `CHECK`. Multi-byte field byte order and
time-unit conversions are part of the protocol contract.

This document records what `bt_receiver.cpp` currently accepts and emits, even
where that differs from older documentation.

## Accepted HCI Event

The receive path accepts only packets whose leading bytes identify:

- H4 packet type `0x04` (event)
- HCI event code `0x3E` (LE Meta Event)
- LE subevent `0x02` (Advertising Report)

It then iterates through the advertising reports and AD structures in the event.
Only an AD structure with type `0xFF` and `ad_len == 22` is considered a
LightDance command.

`ad_len` includes the one-byte AD type, so the manufacturer-specific data below
contains 21 bytes.

## RX Manufacturer Data

Offsets in this table are relative to the first manufacturer-data byte, after
the `0xFF` AD type.

| Offset | Size | Field | Encoding |
| --- | --- | --- | --- |
| `0` | 2 | Manufacturer ID | Literal bytes `FF FF` |
| `2` | 2 | LightDance marker | Literal ASCII bytes `4C 44` (`LD`) |
| `4` | 1 | Command info | High nibble: command ID; low nibble: command type |
| `5` | 8 | Target mask | Little-endian 64-bit mask |
| `13` | 4 | Execution delay | Big-endian milliseconds |
| `17` | 4 | Command-specific payload | See below |

The total manufacturer-data size is 21 bytes. On receipt, execution delay is
multiplied by 1000 and stored internally in microseconds.

### Command-Specific Payload

| Command | Offset `17..20` | Internal conversion |
| --- | --- | --- |
| `PLAY` (`0x01`) | Big-endian preparation duration in ms | Multiplied by 1000 to us |
| `TEST` (`0x05`) | `R`, `G`, `B`, unused | First three bytes copied |
| `CANCEL` (`0x06`) | Target command ID, unused x3 | First byte copied |
| `SEEK` (`0x0A`) | Big-endian timeline position in ms | Multiplied by 1000 to us |
| Other commands | Ignored | Zero-initialized internal payload |

## Targeting

A packet is accepted for this player when either:

- the target mask is `0xFFFFFFFFFFFFFFFF`, or
- bit `my_player_id` in the target mask is set.

The command ID is four bits wide, so its range is `0..15`. This matches the 16
timer slots in the current scheduler.

## ACK Advertisement

The ACK task temporarily disables scanning, configures advertising, transmits
for 300 ms, disables advertising, and resumes scanning.

The complete advertising data constructed by the implementation is:

| AD-data offset | Size | Field | Value / encoding |
| --- | --- | --- | --- |
| `0` | 1 | Flags AD length | `0x02` |
| `1` | 1 | Flags AD type | `0x01` |
| `2` | 1 | Flags | `0x06` |
| `3` | 1 | Manufacturer AD length | `0x0E` |
| `4` | 1 | Manufacturer AD type | `0xFF` |
| `5` | 2 | Manufacturer ID | `FF FF` |
| `7` | 2 | LightDance marker | `4C 44` |
| `9` | 1 | Packet type | `0x07` |
| `10` | 1 | Player ID | `my_player_id` truncated to one byte |
| `11` | 1 | Locked command ID | Most recently locked non-`CHECK` command |
| `12` | 1 | Locked command type | Most recently locked non-`CHECK` command |
| `13` | 4 | Remaining delay | Big-endian milliseconds |
| `17` | 1 | Player state | Value returned by `Player::getState()` |

The ACK reports the cached last non-`CHECK` command, not the command ID/type of
the `CHECK` request itself. Remaining delay is clamped to zero after its target
time and converted from microseconds to integer milliseconds.

## Current Validation Limits

The parser currently assumes the HCI event and nested advertising structures are
well formed. It does not comprehensively validate `len`, report boundaries, or
every AD-structure boundary before reading fields.

The configured `manufacturer_id` is not consulted; literal `FF FF` bytes are
used for both RX filtering and ACK transmission. These are implementation facts
to preserve during the initial file move and address separately afterward.
