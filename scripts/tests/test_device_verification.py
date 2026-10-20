from __future__ import annotations

import io
import unittest
from types import SimpleNamespace
from unittest import mock

from scripts import verify_firmware
from scripts.detect_esp32 import inventory


class DeviceVerificationTests(unittest.TestCase):
    def test_commit_and_ui_are_both_required(self):
        commit = "a" * 40
        self.assertEqual(verify_firmware.verify_stream(io.BytesIO(
            f"ignored network detail\nLEOPOLD_FIRMWARE_REVISION commit={commit} ui=12\n".encode()), commit, 12),
            {"commit": commit, "ui_revision": 12})
        for banner in [f"commit={commit} ui=11", f"commit={'b'*40} ui=12", f"commit={commit}"]:
            with self.assertRaises(ValueError):
                verify_firmware.verify_stream(io.BytesIO(f"LEOPOLD_FIRMWARE_REVISION {banner}\n".encode()), commit, 12)

    def test_timeout_does_not_accept_missing_banner(self):
        with mock.patch.object(verify_firmware.time, "monotonic", side_effect=[0, 16]):
            with self.assertRaises(ValueError):
                verify_firmware.verify_stream(io.BytesIO(), "a" * 40, 12)

    def port(self, name, vid=None, pid=None, serial="one"):
        return SimpleNamespace(device=name, vid=vid, pid=pid, serial_number=serial,
                               description="test USB", location="test")

    def test_inventory_absent_and_ambiguous_do_not_select_device(self):
        bluetooth = self.port("/dev/cu.Bluetooth")
        self.assertEqual(inventory([bluetooth])["status"], "absent")
        a = self.port("/dev/cu.usbmodem1", 0x303A, 0x1001)
        b = self.port("/dev/cu.usbmodem2", 0x303A, 0x1001, "two")
        self.assertEqual(inventory([a, b])["status"], "ambiguous")
        self.assertIsNone(inventory([a, b])["selected"])
        selected = inventory([a, b], serial_number="two")
        self.assertEqual(selected["selected"]["port"], b.device)
        self.assertFalse(selected["serial_port_opened"])
        self.assertIsNone(inventory([a], requested_port="stale-port")["selected"])


if __name__ == "__main__":
    unittest.main()
