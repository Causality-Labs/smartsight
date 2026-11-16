import struct
from collections import namedtuple

PROTOCOL_VERSION = 0x01

OP_MOVE = 0x00
OP_CTRL = 0x01
OP_TELEMETRY = 0x80

CTRL_ENABLE = 0x01
CTRL_DISABLE = 0x02
CTRL_HOME = 0x03

STATUS_ENABLED = 1 << 0
STATUS_SERVO_ERROR = 1 << 1

CDEG_PER_DEG = 100

SEQ_MASK = 0xFFFF
TIMESTAMP_MASK = 0xFFFFFFFF

MOVE_FMT = "<BBHii"
CTRL_FMT = "<BBHBBHI"
TELEMETRY_FMT = "<BBHIiiHH"

MOVE_SIZE = struct.calcsize(MOVE_FMT)
CTRL_SIZE = struct.calcsize(CTRL_FMT)
TELEMETRY_SIZE = struct.calcsize(TELEMETRY_FMT)

assert MOVE_SIZE == 12
assert CTRL_SIZE == 12
assert TELEMETRY_SIZE == 20


class ProtocolError(Exception):
    pass


class Telemetry(namedtuple("Telemetry", "seq timestamp_ms pan_deg tilt_deg status")):
    __slots__ = ()

    @property
    def enabled(self):
        return bool(self.status & STATUS_ENABLED)

    @property
    def servo_error(self):
        return bool(self.status & STATUS_SERVO_ERROR)


def deg_to_cdeg(deg):
    return int(round(deg * CDEG_PER_DEG))


def cdeg_to_deg(cdeg):
    return cdeg / CDEG_PER_DEG


def pack_move(seq, pan_deg, tilt_deg):
    try:
        return struct.pack(
            MOVE_FMT,
            PROTOCOL_VERSION,
            OP_MOVE,
            seq & SEQ_MASK,
            deg_to_cdeg(pan_deg),
            deg_to_cdeg(tilt_deg),
        )
    except struct.error as e:
        raise ProtocolError(f"cannot pack MOVE pan={pan_deg} tilt={tilt_deg}: {e}") from e


def pack_ctrl(seq, code):
    try:
        return struct.pack(
            CTRL_FMT,
            PROTOCOL_VERSION,
            OP_CTRL,
            seq & SEQ_MASK,
            code,
            0,
            0,
            0,
        )
    except struct.error as e:
        raise ProtocolError(f"cannot pack CTRL code={code}: {e}") from e


def unpack_telemetry(buf):
    if len(buf) != TELEMETRY_SIZE:
        raise ProtocolError(f"telemetry length {len(buf)}, expected {TELEMETRY_SIZE}")

    version, opcode, seq, timestamp_ms, pan_cdeg, tilt_cdeg, status, _reserved = struct.unpack(
        TELEMETRY_FMT, buf
    )

    if version != PROTOCOL_VERSION:
        raise ProtocolError(f"telemetry version {version}, expected {PROTOCOL_VERSION}")

    if opcode != OP_TELEMETRY:
        raise ProtocolError(f"opcode 0x{opcode:02X}, expected 0x{OP_TELEMETRY:02X}")

    return Telemetry(
        seq=seq,
        timestamp_ms=timestamp_ms,
        pan_deg=cdeg_to_deg(pan_cdeg),
        tilt_deg=cdeg_to_deg(tilt_cdeg),
        status=status,
    )
