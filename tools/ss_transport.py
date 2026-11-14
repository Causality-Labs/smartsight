import os
import select
import threading
import time
from collections import deque, namedtuple

import ss_protocol as proto

DEFAULT_DEVICE = "/dev/rpmsg0"
RPMSG_MAX_PAYLOAD = 496
HISTORY_LEN = 4096
READ_POLL_S = 0.1
HELLO_RETRY_S = 0.5


LinkStats = namedtuple("LinkStats", "received bad gaps lost")


class Link:
    def __init__(self, device=DEFAULT_DEVICE, history_len=HISTORY_LEN):
        self._device = device
        self._fd = None
        self._reader = None
        self._stop = threading.Event()

        self._seq = 0
        self._seq_lock = threading.Lock()

        self._cond = threading.Condition()
        self._latest = None
        self._history = deque(maxlen=history_len)
        self._received = 0
        self._bad = 0
        self._gaps = 0
        self._lost = 0
        self._last_seq = None
        self._error = None

    def __enter__(self):
        self.open()
        return self

    def __exit__(self, exc_type, exc, tb):
        self.close()

    def open(self):
        if self._fd is not None:
            raise RuntimeError("link already open")

        self._fd = os.open(self._device, os.O_RDWR)
        self._stop.clear()
        self._reader = threading.Thread(target=self._read_loop, name="ss-reader", daemon=True)
        self._reader.start()

    def close(self):
        if self._fd is None:
            return

        self._stop.set()
        self._reader.join()
        os.close(self._fd)
        self._fd = None
        self._reader = None

    def _next_seq(self):
        with self._seq_lock:
            self._seq = (self._seq + 1) & proto.SEQ_MASK
            return self._seq

    def send_raw(self, data):
        os.write(self._fd, data)

    def send_move(self, pan_deg, tilt_deg):
        self.send_raw(proto.pack_move(self._next_seq(), pan_deg, tilt_deg))

    def send_ctrl(self, code):
        self.send_raw(proto.pack_ctrl(self._next_seq(), code))

    def enable(self):
        self.send_ctrl(proto.CTRL_ENABLE)

    def disable(self):
        self.send_ctrl(proto.CTRL_DISABLE)

    def home(self):
        self.send_ctrl(proto.CTRL_HOME)

    def sync(self, timeout=5.0):
        deadline = time.monotonic() + timeout
        while time.monotonic() < deadline:
            self.send_ctrl(0x00)
            try:
                return self.wait_for_telemetry(HELLO_RETRY_S)
            except TimeoutError:
                continue
        raise TimeoutError(f"no telemetry from {self._device} within {timeout}s")

    def _read_loop(self):
        while not self._stop.is_set():
            try:
                ready, _, _ = select.select([self._fd], [], [], READ_POLL_S)
                if not ready:
                    continue
                buf = os.read(self._fd, RPMSG_MAX_PAYLOAD)
            except OSError as e:
                with self._cond:
                    self._error = e
                    self._cond.notify_all()
                return

            self._handle(buf)

    def _handle(self, buf):
        try:
            tel = proto.unpack_telemetry(buf)
        except proto.ProtocolError:
            with self._cond:
                self._bad += 1
            return

        with self._cond:
            if self._last_seq is not None:
                step = (tel.seq - self._last_seq) & proto.SEQ_MASK
                if step != 1:
                    self._gaps += 1
                    self._lost += (step - 1) & proto.SEQ_MASK
            self._last_seq = tel.seq
            self._received += 1
            self._latest = tel
            self._history.append(tel)
            self._cond.notify_all()

    @property
    def latest(self):
        with self._cond:
            return self._latest

    @property
    def stats(self):
        with self._cond:
            return LinkStats(self._received, self._bad, self._gaps, self._lost)

    def history(self):
        with self._cond:
            return list(self._history)

    def clear_history(self):
        with self._cond:
            self._history.clear()

    def wait_until(self, predicate, timeout):
        deadline = time.monotonic() + timeout
        with self._cond:
            while True:
                if self._error is not None:
                    raise RuntimeError(f"reader stopped: {self._error}")

                if self._latest is not None and predicate(self._latest):
                    return self._latest

                remaining = deadline - time.monotonic()
                if remaining <= 0:
                    raise TimeoutError(f"condition not met within {timeout}s, latest={self._latest}")

                self._cond.wait(remaining)

    def wait_for_telemetry(self, timeout):
        with self._cond:
            start = self._received

        def newer(_tel):
            return self._received > start

        return self.wait_until(newer, timeout)
