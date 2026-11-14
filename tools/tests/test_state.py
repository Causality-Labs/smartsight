import mount_spec as spec
from helpers import at_position, far_pan_from, observe, pose, travel_timeout
from testbase import DisabledTestCase, HomedTestCase, motion


class StateTests(DisabledTestCase):
    def test_enable_and_disable_flip_status(self):
        self.enable_and_wait()
        self.disable_and_wait()

    def test_move_while_disabled_is_ignored(self):
        start = pose(self.link.latest)

        self.link.send_move(far_pan_from(start[0]), start[1])
        history = observe(self.link, 0.5)

        self.assertTrue(history)
        self.assertTrue(all(pose(t) == start for t in history))
        self.assertFalse(self.link.latest.enabled)

    def test_rejected_move_is_not_applied_on_enable(self):
        start = pose(self.link.latest)

        self.link.send_move(far_pan_from(start[0]), start[1])
        observe(self.link, 0.3)

        self.enable_and_wait()
        history = observe(self.link, 0.5)

        self.assertTrue(history)
        self.assertTrue(all(pose(t) == start for t in history))

    def test_enable_holds_position(self):
        start = pose(self.link.latest)

        self.enable_and_wait()
        history = observe(self.link, 0.5)

        self.assertTrue(history)
        self.assertTrue(all(pose(t) == start for t in history))


@motion
class StateMotionTests(HomedTestCase):
    def stop_mid_move(self):
        self.link.send_move(spec.PAN_MAX_DEG, spec.TILT_HOME_DEG)
        self.wait(lambda t: t.pan_deg > spec.PAN_HOME_DEG + 20, 2.0)
        self.disable_and_wait()

        return pose(self.link.latest)

    def test_home_is_ignored_while_disabled(self):
        away = (30.0, 155.0)

        self.link.send_move(*away)
        self.wait(at_position(*away), travel_timeout(abs(spec.PAN_HOME_DEG - away[0])))
        self.disable_and_wait()

        self.link.home()
        history = observe(self.link, 0.5)

        self.assertTrue(history)
        self.assertTrue(all(at_position(*away)(t) for t in history))

    def test_disable_stops_motion(self):
        stopped = self.stop_mid_move()
        history = observe(self.link, 0.5)

        self.assertLess(stopped[0], spec.PAN_MAX_DEG)
        self.assertTrue(history)
        self.assertTrue(all(pose(t) == stopped for t in history))

    def test_reenable_holds_position_instead_of_resuming(self):
        stopped = self.stop_mid_move()

        self.enable_and_wait()
        history = observe(self.link, 0.5)

        self.assertTrue(history)
        self.assertTrue(all(pose(t) == stopped for t in history))
