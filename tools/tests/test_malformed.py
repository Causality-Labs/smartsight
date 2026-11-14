import struct

import mount_spec as spec
import ss_protocol as proto
from helpers import at_position, build_raw, observe, pose
from testbase import DisabledTestCase, HomedTestCase, motion

UNKNOWN_OPCODES = [0x02, 0x7F, 0x80, 0xFF]

ENABLE_PAYLOAD = struct.pack("<BBH", proto.CTRL_ENABLE, 0, 0)
DISABLE_PAYLOAD = struct.pack("<BBH", proto.CTRL_DISABLE, 0, 0)
FAR_MOVE_PAYLOAD = struct.pack(
    "<HH",
    proto.deg_to_cdeg(spec.PAN_MAX_DEG),
    proto.deg_to_cdeg(spec.TILT_HOME_DEG),
)


def malformed_variants(opcode, payload):
    valid = build_raw(proto.PROTOCOL_VERSION, opcode, payload)

    variants = [
        ("version 0", build_raw(0, opcode, payload)),
        ("version 2", build_raw(2, opcode, payload)),
        ("version 255", build_raw(255, opcode, payload)),
        ("truncated to 7 bytes", valid[:7]),
        ("truncated to 5 bytes", valid[:5]),
        ("truncated to 4 bytes", valid[:4]),
        ("truncated to 1 byte", valid[:1]),
        ("oversized by 1 byte", valid + b"\x00"),
        ("oversized to 64 bytes", valid + bytes(64 - len(valid))),
    ]

    for unknown in UNKNOWN_OPCODES:
        variants.append(
            (f"unknown opcode 0x{unknown:02X}",
             build_raw(proto.PROTOCOL_VERSION, unknown, payload))
        )

    return variants


class MalformedDisabledTests(DisabledTestCase):
    def test_malformed_enable_is_ignored(self):
        for label, data in malformed_variants(proto.OP_CTRL, ENABLE_PAYLOAD):
            with self.subTest(case=label):
                self.disable_and_wait()
                start = pose(self.link.latest)

                self.link.send_raw(data)
                history = observe(self.link, 0.3)

                self.assertTrue(history)
                self.assertFalse(any(t.enabled for t in history))
                self.assertTrue(all(pose(t) == start for t in history))

    def test_valid_command_works_after_malformed_burst(self):
        before = self.link.stats
        messages = (
            malformed_variants(proto.OP_CTRL, ENABLE_PAYLOAD)
            + malformed_variants(proto.OP_MOVE, FAR_MOVE_PAYLOAD)
        )

        for _ in range(5):
            for _label, data in messages:
                self.link.send_raw(data)

        history = observe(self.link, 0.5)
        self.assertTrue(history)
        self.assertFalse(any(t.enabled for t in history))

        self.enable_and_wait()
        after = self.link.stats

        self.assertEqual(after.bad, before.bad)
        self.assertEqual(after.lost, before.lost)


@motion
class MalformedEnabledTests(HomedTestCase):
    def reset(self):
        self.enable_and_wait()
        self.rehome()

    def test_malformed_move_does_not_move_mount(self):
        at_home = at_position(spec.PAN_HOME_DEG, spec.TILT_HOME_DEG)

        for label, data in malformed_variants(proto.OP_MOVE, FAR_MOVE_PAYLOAD):
            with self.subTest(case=label):
                self.reset()

                self.link.send_raw(data)
                history = observe(self.link, 0.3)

                self.assertTrue(history)
                self.assertTrue(all(at_home(t) for t in history))

    def test_malformed_disable_does_not_disable_mount(self):
        for label, data in malformed_variants(proto.OP_CTRL, DISABLE_PAYLOAD):
            with self.subTest(case=label):
                self.reset()

                self.link.send_raw(data)
                history = observe(self.link, 0.3)

                self.assertTrue(history)
                self.assertTrue(all(t.enabled for t in history))
