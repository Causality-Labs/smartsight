import time

import mount_spec as spec
from helpers import median, observe, periods_ms
from testbase import LinkTestCase


class TelemetryTests(LinkTestCase):
    def test_stream_is_alive(self):
        before = self.link.stats.received
        time.sleep(1.0)

        self.assertGreater(self.link.stats.received, before)

    def test_no_malformed_packets(self):
        before = self.link.stats
        time.sleep(2.0)
        after = self.link.stats

        self.assertGreater(after.received, before.received)
        self.assertEqual(after.bad, before.bad)

    def test_sequence_has_no_gaps(self):
        before = self.link.stats
        time.sleep(3.0)
        after = self.link.stats

        self.assertGreater(after.received, before.received)
        self.assertEqual(after.lost, before.lost)

    def test_timestamps_strictly_increase(self):
        history = observe(self.link, 1.0)
        stamps = [t.timestamp_ms for t in history]

        self.assertGreaterEqual(len(stamps), 10)
        self.assertTrue(all(b > a for a, b in zip(stamps, stamps[1:])))

    def test_period_matches_firmware_loop(self):
        periods = periods_ms(observe(self.link, 2.0))

        self.assertGreaterEqual(len(periods), 50)
        self.assertAlmostEqual(median(periods), spec.TELEMETRY_PERIOD_MS, delta=5)
        self.assertLess(max(periods), 5 * spec.TELEMETRY_PERIOD_MS)

    def test_servo_error_flag_is_clear(self):
        history = observe(self.link, 1.0)

        self.assertTrue(history)
        self.assertFalse(any(t.servo_error for t in history))
