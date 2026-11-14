import mount_spec as spec
from helpers import at_position, periods_ms, travel_timeout
from testbase import HomedTestCase, motion


@motion
class MotionTests(HomedTestCase):
    def test_home_pose(self):
        self.assertTrue(at_position(spec.PAN_HOME_DEG, spec.TILT_HOME_DEG)(self.link.latest))

    def test_move_reaches_target(self):
        targets = [(0.0, 150.0), (180.0, 180.0), (45.5, 172.25)]

        for pan, tilt in targets:
            with self.subTest(pan=pan, tilt=tilt):
                self.rehome()
                distance = max(abs(pan - spec.PAN_HOME_DEG), abs(tilt - spec.TILT_HOME_DEG))

                self.link.send_move(pan, tilt)
                self.wait(at_position(pan, tilt), travel_timeout(distance))

    def test_motion_speed_is_limited(self):
        target = spec.PAN_MIN_DEG
        distance = spec.PAN_HOME_DEG - target

        self.link.clear_history()
        self.link.send_move(target, spec.TILT_HOME_DEG)
        self.wait(at_position(target, spec.TILT_HOME_DEG), travel_timeout(distance))
        history = self.link.history()

        first_moved = next(i for i, t in enumerate(history) if t.pan_deg != spec.PAN_HOME_DEG)
        arrived = next(t for t in history if t.pan_deg == target)
        started = history[max(first_moved - 1, 0)]

        duration_s = (arrived.timestamp_ms - started.timestamp_ms) / 1000.0
        average = distance / duration_s

        self.assertLessEqual(average, spec.MAX_SPEED_DEG_S * 1.05)
        self.assertGreaterEqual(average, spec.MAX_SPEED_DEG_S * 0.7)

    def test_out_of_range_targets_are_clamped(self):
        cases = [
            (250.0, 120.0, spec.PAN_MAX_DEG, spec.TILT_MIN_DEG),
            (90.0, 200.0, 90.0, spec.TILT_MAX_DEG),
            (655.0, 10.0, spec.PAN_MAX_DEG, spec.TILT_MIN_DEG),
        ]

        for pan, tilt, expected_pan, expected_tilt in cases:
            with self.subTest(pan=pan, tilt=tilt):
                self.rehome()

                self.link.clear_history()
                self.link.send_move(pan, tilt)
                self.wait(at_position(expected_pan, expected_tilt),
                          travel_timeout(spec.PAN_MAX_DEG))

                for t in self.link.history():
                    self.assertTrue(spec.PAN_MIN_DEG <= t.pan_deg <= spec.PAN_MAX_DEG)
                    self.assertTrue(spec.TILT_MIN_DEG <= t.tilt_deg <= spec.TILT_MAX_DEG)

    def test_move_flood_latest_wins_and_telemetry_keeps_flowing(self):
        final = (30.0, spec.TILT_HOME_DEG)
        before = self.link.stats

        self.link.clear_history()
        for i in range(100):
            self.link.send_move(90.0 - 0.6 * i, spec.TILT_HOME_DEG)
        self.link.send_move(*final)

        self.wait(at_position(*final), travel_timeout(spec.PAN_HOME_DEG - final[0]))
        after = self.link.stats
        periods = periods_ms(self.link.history())

        self.assertEqual(after.bad, before.bad)
        self.assertEqual(after.lost, before.lost)
        self.assertLess(max(periods), 5 * spec.TELEMETRY_PERIOD_MS)
