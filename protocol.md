# SmartSight RPMsg Protocol (v1)

Binary message protocol between the Linux application (A53) and the M7 firmware over RPMsg.

**Source of truth:** [`m7_app/smartsight_message.h`](m7_app/smartsight_message.h). If this document and the header disagree, the header is correct and this document must be updated. The Python format strings below must match the header's struct layouts.

## Transport

| Item | Value |
|---|---|
| Channel name (M7 announces) | `rpmsg-raw` |
| M7 endpoint address | 30 (`0x1E`) |
| Linux device | `/dev/rpmsg0` (created by `rpmsg_chrdev`) |
| Framing | one `write()` = one message, one `read()` = one message |
| Max payload | 496 bytes |
| Byte order | little-endian |
| Padding | none; every field is naturally aligned, sizes are exact |

## Common header: `ss_header_t` (4 bytes)

Every message starts with this header.

| Offset | Size | Type | Field | Description |
|---|---|---|---|---|
| 0 | 1 | `uint8_t` | `version` | Protocol version, `SS_PROTOCOL_VERSION` (`1` for this document). |
| 1 | 1 | `uint8_t` | `opcode` | Message type (see below). |
| 2 | 2 | `uint16_t` | `sequence` | Sender's sequence number. +1 per message, wraps 65535 → 0. |

### Opcodes

| Value | Constant | Name | Direction | C type | Size |
|---|---|---|---|---|---|
| `0x00` | `SS_OP_MOVE` | `MOVE` | Linux → M7 | `ss_cmd_t`, `payload.position` | 8 bytes |
| `0x01` | `SS_OP_CTRL` | `CTRL` | Linux → M7 | `ss_cmd_t`, `payload.ctrl` | 8 bytes |
| `0x80` | `SS_OP_TELEMETRY` | `TELEMETRY` | M7 → Linux | `ss_tel_t` | 16 bytes |

Linux → M7 opcodes start at `0x00`; M7 → Linux opcodes start at `0x80`.

Both Linux → M7 messages use `ss_cmd_t`: an `ss_header_t` followed by a 4-byte `payload` union. `opcode` says which union member is valid.

## Units

All angles are **centidegrees** in unsigned 16-bit integers: `12050` = 120.50°.

| Axis | Min | Max | Home |
|---|---|---|---|
| Pan | 0 | 18000 | 9000 |
| Tilt | 15000 | 18000 | 16500 |

Integers are used instead of floats so that NaN/Inf cannot be sent and range checks are plain integer comparisons.

## `MOVE` (`SS_OP_MOVE`, `0x00`): Linux → M7

Commands the mount to move to an absolute position. The M7 steps toward it at its configured maximum rate. A newer `MOVE` replaces the current target ("newest wins"), so the M7 does not queue them.

C type: `ss_cmd_t` with `payload.position` (`ss_position_payload_t`).

| Offset | Size | Type | Field | Description |
|---|---|---|---|---|
| 0 | 4 | `ss_header_t` | `header` | `version=1`, `opcode=0x00` |
| 4 | 2 | `uint16_t` | `payload.position.pan_cdeg` | Target pan, centidegrees |
| 6 | 2 | `uint16_t` | `payload.position.tilt_cdeg` | Target tilt, centidegrees |

Python: `struct.pack('<BBHHH', 1, 0x00, sequence, pan_cdeg, tilt_cdeg)`

**Rate:** at most 50 Hz. The M7 steps every 20 ms, so faster commands are just overwritten.

**Example:** sequence 1, pan 120.50°, tilt 160.00°

```
01 00 01 00 12 2F 80 3E
```

## `CTRL` (`SS_OP_CTRL`, `0x01`): Linux → M7

Changes the mount's state.

C type: `ss_cmd_t` with `payload.ctrl` (`ss_ctrl_payload_t`).

| Offset | Size | Type | Field | Description |
|---|---|---|---|---|
| 0 | 4 | `ss_header_t` | `header` | `version=1`, `opcode=0x01` |
| 4 | 1 | `uint8_t` | `payload.ctrl.ctrl` | Control code (see below) |
| 5 | 1 | `uint8_t` | `payload.ctrl.reserved_0` | Send as 0, ignore on receive |
| 6 | 2 | `uint16_t` | `payload.ctrl.reserved_1` | Send as 0, ignore on receive |

Python: `struct.pack('<BBHBBH', 1, 0x01, sequence, ctrl, 0, 0)`

### Control codes

| Value | Constant | Effect |
|---|---|---|
| `0x01` | `SS_CTRL_ENABLE` | Start driving the servos. The mount holds its current position until a `MOVE` or `HOME` arrives. |
| `0x02` | `SS_CTRL_DISABLE` | Stop driving the servos immediately. Any target in progress is discarded. |
| `0x03` | `SS_CTRL_HOME` | Move to the home position (see [Units](#units)) at the normal step rate. |

Any other `ctrl` value, including `0x00`, is rejected.

**Example:** sequence 2, HOME

```
01 01 02 00 03 00 00 00
```

## Processing rules (M7 side)

- **`CTRL` messages are processed in the order received and are never dropped.**
- **`MOVE` messages are newest-wins:** if several arrive before the mount task runs, only the latest is used.
- Each control tick, the M7 applies all pending `CTRL`s first, then the latest `MOVE`, then steps the mount. A `MOVE` sent after a `CTRL` is therefore always applied after it.
- **The mount is disabled after boot.** Linux must send `SS_CTRL_ENABLE` before the mount will move.
- **While disabled, `MOVE` and `SS_CTRL_HOME` are rejected.** They are not stored for later.

## `TELEMETRY` (`SS_OP_TELEMETRY`, `0x80`): M7 → Linux

Periodic report of the mount's state. Each message is a complete snapshot, so a lost message is corrected by the next one. Telemetry also acts as a heartbeat: if it stops, the M7 is down or stuck.

C type: `ss_tel_t`.

| Offset | Size | Type | Field | Description |
|---|---|---|---|---|
| 0 | 4 | `ss_header_t` | `header` | `version=1`, `opcode=0x80`, `sequence` = M7's own counter |
| 4 | 4 | `uint32_t` | `timestamp_ms` | M7 time since boot in ms |
| 8 | 2 | `uint16_t` | `pan_cdeg` | Current pan, centidegrees |
| 10 | 2 | `uint16_t` | `tilt_cdeg` | Current tilt, centidegrees |
| 12 | 2 | `uint16_t` | `status` | Status flags (see below) |
| 14 | 2 | `uint16_t` | `reserved` | Send as 0, ignore on receive |

Python: `struct.unpack('<BBHIHHHH', data)`

**Rate:** 20 Hz.

**Timestamp:** FreeRTOS tick count converted to ms. Wraps after ~49.7 days, so compare timestamps with unsigned subtraction (`(now - then) & 0xFFFFFFFF`). The source may later change to the i.MX8M system counter so that M7 and Linux share a timebase.

### `status` flags

| Bit | Name | Meaning |
|---|---|---|
| 0 | `ENABLED` | Servos are being driven |
| 1 | `SERVO_ERROR` | Last servo update failed |
| 2–15 | reserved | Send as 0 |

**Example:** sequence 5, 51230 ms, pan 90.00°, tilt 165.00°, enabled

```
01 80 05 00 1E C8 00 00 28 23 74 40 01 00 00 00
```

## Start-up sequence

1. M7 boots and announces `rpmsg-raw` on endpoint 30.
2. Linux kernel binds `rpmsg_chrdev` and creates `/dev/rpmsg0`.
3. Linux opens `/dev/rpmsg0` and sends a first message, normally `CTRL` with `SS_CTRL_ENABLE` since the mount starts disabled.
4. The M7 learns Linux's address from that message and starts sending `TELEMETRY`.

The M7 cannot send anything before step 3 because it does not know Linux's address yet. The M7 updates the address from every received message, so telemetry follows the Linux app if it closes and reopens the device.
