import mount_spec as spec
from helpers import at_position, observe, pose
from testbase import DisabledTestCase, HomedTestCase, motion

INVALID_CTRL_CODES = [0x00, 0x04, 0x07, 0xFF]


class InvalidCtrlDisabledTests(DisabledTestCase):
    def test_invalid_ctrl_is_ignored_while_disabled(self):
        start = pose(self.link.latest)

        for code in INVALID_CTRL_CODES:
            with self.subTest(code=code):
                self.link.send_ctrl(code)
                history = observe(self.link, 0.5)

                self.assertTrue(history)
                self.assertFalse(any(t.enabled for t in history))
                self.assertTrue(all(pose(t) == start for t in history))


@motion
class InvalidCtrlEnabledTests(HomedTestCase):
    def test_invalid_ctrl_is_ignored_while_enabled(self):
        at_home = at_position(spec.PAN_HOME_DEG, spec.TILT_HOME_DEG)

        for code in INVALID_CTRL_CODES:
            with self.subTest(code=code):
                self.link.send_ctrl(code)
                history = observe(self.link, 0.5)

                self.assertTrue(history)
                self.assertTrue(all(t.enabled for t in history))
                self.assertTrue(all(at_home(t) for t in history))

    def test_ctrl_burst_does_not_stall_firmware(self):
        before = self.link.stats

        self.link.clear_history()
        for _ in range(10):
            self.link.home()
        history = observe(self.link, 0.5)
        after = self.link.stats

        self.assertGreaterEqual(len(history), 10)
        self.assertTrue(all(t.enabled for t in history))
        self.assertEqual(after.bad, before.bad)
        self.assertEqual(after.lost, before.lost)
