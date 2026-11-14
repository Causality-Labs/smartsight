#!/usr/bin/env python3
import argparse
import os
import select
import struct
import sys
import threading
import time

DEVICE = "/dev/rpmsg0"
VERSION = 1

OP_MOVE = 0x00
OP_CTRL = 0x01
OP_TELEMETRY = 0x80

TEL_FMT = "<BBHIHHHH"
TEL_SIZE = struct.calcsize(TEL_FMT)
RPMSG_MAX_PAYLOAD = 496

STATUS_ENABLED = 1 << 0
STATUS_SERVO_ERROR = 1 << 1

CTRL_CODES = {
    "enable": 0x01,
    "disable": 0x02,
    "home": 0x03,
}


class Link:
    def __init__(self, device):
        self.fd = os.open(device, os.O_RDWR)
        self.seq = 0

    def _next_seq(self):
        self.seq = (self.seq + 1) & 0xFFFF
        return self.seq

    def move(self, pan_deg, tilt_deg):
        pan = int(round(pan_deg * 100))
        tilt = int(round(tilt_deg * 100))
        msg = struct.pack("<BBHHH", VERSION, OP_MOVE, self._next_seq(), pan, tilt)
        os.write(self.fd, msg)
        print(f"  -> MOVE pan={pan_deg:.2f} tilt={tilt_deg:.2f}  [{msg.hex(' ')}]")

    def ctrl(self, code):
        msg = struct.pack("<BBHBBH", VERSION, OP_CTRL, self._next_seq(), code, 0, 0)
        os.write(self.fd, msg)
        name = next((k for k, v in CTRL_CODES.items() if v == code), f"0x{code:02X}")
        print(f"  -> CTRL {name}  [{msg.hex(' ')}]")

    def close(self):
        os.close(self.fd)


class TelemetryMonitor:
    def __init__(self, fd, every):
        self.fd = fd
        self.every = max(1, every)
        self.count = 0
        self.gaps = 0
        self.bad = 0
        self.last_seq = None
        self.last_t = None
        self.last_status = None

    def handle(self, buf):
        if len(buf) != TEL_SIZE:
            self.bad += 1
            print(f"  <- bad length {len(buf)}  [{buf.hex(' ')}]")
            return

        version, op, seq, t_ms, pan, tilt, status, _ = struct.unpack(TEL_FMT, buf)

        if version != VERSION or op != OP_TELEMETRY:
            self.bad += 1
            print(f"  <- unexpected version={version} op=0x{op:02X}  [{buf.hex(' ')}]")
            return

        if self.last_seq is not None and ((seq - self.last_seq) & 0xFFFF) != 1:
            self.gaps += 1
            print(f"  <- GAP seq {self.last_seq} -> {seq}")

        dt = 0
        if self.last_t is not None:
            dt = (t_ms - self.last_t) & 0xFFFFFFFF

        status_changed = status != self.last_status

        self.count += 1
        self.last_seq = seq
        self.last_t = t_ms
        self.last_status = status

        if self.count == 1 or status_changed or self.count % self.every == 0:
            state = "ENABLED " if status & STATUS_ENABLED else "DISABLED"
            err = " SERVO_ERROR" if status & STATUS_SERVO_ERROR else ""
            print(f"  <- TEL seq={seq:5d} t={t_ms:9d}ms dt={dt:3d}ms "
                  f"pan={pan / 100:6.2f} tilt={tilt / 100:6.2f} {state}{err}")

    def run(self, stop):
        while not stop.is_set():
            ready, _, _ = select.select([self.fd], [], [], 0.2)
            if ready:
                try:
                    buf = os.read(self.fd, RPMSG_MAX_PAYLOAD)
                except OSError as e:
                    print(f"  <- read error: {e}")
                    return
                self.handle(buf)

    def summary(self):
        print(f"\ntelemetry: {self.count} messages, {self.gaps} gaps, {self.bad} bad")


def step(title, expected, auto):
    print(f"\n=== {title}")
    print(f"    expect: {expected}")
    if not auto:
        input("    press Enter to send...")


def run_sequence(link, auto):
    step("1. MOVE while disabled",
         "UART 'Rejected positonal command while disabled', no motion", auto)
    link.move(45.0, 160.0)
    time.sleep(1.0)

    step("2. ENABLE", "servos powered, mount holds position", auto)
    link.ctrl(CTRL_CODES["enable"])
    time.sleep(1.0)

    step("3. MOVE pan 0, tilt 160", "smooth motion at ~50 deg/s (~2 s)", auto)
    link.move(0.0, 160.0)
    time.sleep(3.0)

    step("4. HOME", "returns to pan 90, tilt 165", auto)
    link.ctrl(CTRL_CODES["home"])
    time.sleep(3.0)

    step("5. MOVE pan 180, DISABLE halfway", "stops part-way, servos go limp", auto)
    link.move(180.0, 165.0)
    time.sleep(0.9)
    link.ctrl(CTRL_CODES["disable"])
    time.sleep(1.0)

    step("6. ENABLE again", "HOLDS where it stopped, does not resume to 180", auto)
    link.ctrl(CTRL_CODES["enable"])
    time.sleep(2.0)

    step("7. Flood 100 MOVEs", "smooth motion toward pan 30, M7 stays responsive", auto)
    for i in range(100):
        link.move(90.0 - 0.6 * i, 165.0)
    time.sleep(3.0)

    step("8. Invalid CTRL codes", "UART 'Invalid CTRL payload 0x00' and '0x07'", auto)
    link.ctrl(0x00)
    link.ctrl(0x07)
    time.sleep(1.0)

    step("9. 5 CTRLs back-to-back", "some 'Ctrl Queue is full' messages on UART", auto)
    for _ in range(5):
        link.ctrl(CTRL_CODES["home"])
    time.sleep(3.0)

    step("Done", "disabling mount", auto)
    link.ctrl(CTRL_CODES["disable"])


def main():
    parser = argparse.ArgumentParser(description="SmartSight RPMsg protocol test tool")
    parser.add_argument("--device", default=DEVICE)
    sub = parser.add_subparsers(dest="cmd", required=True)

    p_move = sub.add_parser("move", help="send MOVE <pan_deg> <tilt_deg>")
    p_move.add_argument("pan", type=float)
    p_move.add_argument("tilt", type=float)

    p_ctrl = sub.add_parser("ctrl", help="send CTRL enable|disable|home|<raw int>")
    p_ctrl.add_argument("code")

    p_seq = sub.add_parser("seq", help="run the full test sequence")
    p_seq.add_argument("--auto", action="store_true", help="don't pause between steps")
    p_seq.add_argument("--listen", action="store_true", help="print telemetry during the sequence")
    p_seq.add_argument("--every", type=int, default=10, help="print every Nth telemetry message")

    p_listen = sub.add_parser("listen", help="decode telemetry until Ctrl-C")
    p_listen.add_argument("--every", type=int, default=10, help="print every Nth telemetry message")

    args = parser.parse_args()

    try:
        link = Link(args.device)
    except OSError as e:
        sys.exit(f"cannot open {args.device}: {e}")

    monitor = None
    stop = threading.Event()
    reader = None

    if args.cmd == "listen" or (args.cmd == "seq" and args.listen):
        monitor = TelemetryMonitor(link.fd, args.every)
        reader = threading.Thread(target=monitor.run, args=(stop,), daemon=True)
        reader.start()

    try:
        if args.cmd == "move":
            link.move(args.pan, args.tilt)
        elif args.cmd == "ctrl":
            code = CTRL_CODES.get(args.code.lower())
            if code is None:
                code = int(args.code, 0)
            link.ctrl(code)
        elif args.cmd == "seq":
            run_sequence(link, args.auto)
            if monitor:
                time.sleep(1.0)
        elif args.cmd == "listen":
            print("  (sending an invalid CTRL as a ping so the M7 learns our address)")
            link.ctrl(0x00)
            while True:
                time.sleep(1.0)
    except KeyboardInterrupt:
        pass
    finally:
        stop.set()
        if reader:
            reader.join()
        if monitor:
            monitor.summary()
        link.close()


if __name__ == "__main__":
    main()
