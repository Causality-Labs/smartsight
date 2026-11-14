import atexit
import os

import mount_spec as spec
from helpers import at_position, travel_timeout
from minitest import TestCase
from ss_transport import DEFAULT_DEVICE, Link

DEVICE_ENV = "SS_DEVICE"
SKIP_MOTION_ENV = "SS_SKIP_MOTION"

_link = None
_link_error = None


def _shutdown():
    global _link

    if _link is None:
        return

    try:
        _link.disable()
    except OSError:
        pass

    _link.close()
    _link = None


def get_link():
    global _link, _link_error

    if _link_error is not None:
        raise _link_error

    if _link is not None:
        return _link

    device = os.environ.get(DEVICE_ENV, DEFAULT_DEVICE)
    lk = Link(device)

    try:
        lk.open()
        lk.sync(timeout=5.0)
    except (OSError, TimeoutError) as e:
        stats = lk.stats
        lk.close()
        _link_error = RuntimeError(f"link to {device} failed: {e} (stats: {stats})")
        raise _link_error

    atexit.register(_shutdown)
    _link = lk
    return lk


def motion(cls):
    if os.environ.get(SKIP_MOTION_ENV) == "1":
        cls.skip_reason = "motion tests disabled"

    return cls


class LinkTestCase(TestCase):
    @classmethod
    def setUpClass(cls):
        cls.link = get_link()

    def wait(self, predicate, timeout):
        try:
            return self.link.wait_until(predicate, timeout)
        except TimeoutError as e:
            self.fail(str(e))

    def enable_and_wait(self):
        self.link.enable()
        self.wait(lambda t: t.enabled, spec.STATE_TIMEOUT_S)

    def disable_and_wait(self):
        self.link.disable()
        self.wait(lambda t: not t.enabled, spec.STATE_TIMEOUT_S)


class DisabledTestCase(LinkTestCase):
    def setUp(self):
        self.addCleanup(self.disable_and_wait)
        self.disable_and_wait()


class HomedTestCase(LinkTestCase):
    def setUp(self):
        self.addCleanup(self.disable_and_wait)
        self.enable_and_wait()
        self.rehome()

    def rehome(self):
        self.link.home()
        self.wait(
            at_position(spec.PAN_HOME_DEG, spec.TILT_HOME_DEG),
            travel_timeout(spec.PAN_MAX_DEG - spec.PAN_MIN_DEG),
        )
