import struct
import time

import mount_spec as spec


def observe(link, seconds):
    link.clear_history()
    time.sleep(seconds)
    return link.history()


def pose(tel):
    return (tel.pan_deg, tel.tilt_deg)


def at_position(pan_deg, tilt_deg, tol=spec.POSITION_TOL_DEG):
    def predicate(tel):
        return abs(tel.pan_deg - pan_deg) <= tol and abs(tel.tilt_deg - tilt_deg) <= tol

    return predicate


def travel_timeout(distance_deg):
    return distance_deg / spec.MAX_SPEED_DEG_S * 1.5 + 1.0


def far_pan_from(pan_deg):
    if pan_deg < spec.PAN_HOME_DEG:
        return spec.PAN_MAX_DEG

    return spec.PAN_MIN_DEG


def build_raw(version, opcode, payload=b"", seq=0):
    return struct.pack("<BBH", version, opcode, seq) + payload


def periods_ms(history):
    return [b.timestamp_ms - a.timestamp_ms for a, b in zip(history, history[1:])]


def median(values):
    ordered = sorted(values)
    mid = len(ordered) // 2

    if len(ordered) % 2:
        return ordered[mid]

    return (ordered[mid - 1] + ordered[mid]) / 2
