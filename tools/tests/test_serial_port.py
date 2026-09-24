# Copyright (c) 2025 Nations Technologies Inc.
# SPDX-License-Identifier: Apache-2.0

"""NSLink port detection and open-failure reporting.

These began as tests of the camera capture client's own helpers; the rules are
not camera-specific, so they moved here with the code when that client was
retired.
"""
from pathlib import Path
import sys
import unittest
from unittest.mock import patch
from types import SimpleNamespace

import serial

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import serial_port


class SerialPortTests(unittest.TestCase):
    @staticmethod
    def port(device, vid=None, pid=None, description="USB serial"):
        return SimpleNamespace(device=device, vid=vid, pid=pid, description=description,
                               product=None, manufacturer=None, hwid="test")

    def test_auto_port_uses_nslink_id_on_windows_and_linux(self):
        for platform, device, other in (("win32", "COM12", "COM1"),
                                        ("linux", "/dev/ttyACM2", "/dev/ttyS0")):
            ports = [self.port(other), self.port(device, 0x19f5, 0x3106)]
            with patch.object(serial_port.sys, "platform", platform), \
                    patch.object(serial_port.list_ports, "comports", return_value=ports):
                self.assertEqual(serial_port.resolve_serial_port(), device)
                self.assertEqual(serial_port.resolve_serial_port("auto"), device)

    def test_auto_port_rejects_ambiguity_and_unrelated_devices(self):
        for ports, expected in (([self.port("COM1")], "No NSLink"),
                                ([], "No NSLink"),
                                ([self.port("COM4", 0x19f5, 0x3106),
                                  self.port("COM5", 0x19f5, 0x3106)], "Multiple")):
            with patch.object(serial_port.list_ports, "comports", return_value=ports):
                with self.assertRaisesRegex(serial.SerialException, expected):
                    serial_port.resolve_serial_port()

    def test_nslink_description_fallback(self):
        with patch.object(serial_port.list_ports, "comports", return_value=[
                self.port("COM7", description="NSLink CDC")]):
            self.assertEqual(serial_port.resolve_serial_port(), "COM7")

    def test_explicit_port_is_preserved_and_wrong_platform_is_explained(self):
        for platform, valid, invalid in (("win32", "COM8", "/dev/ttyACM0"),
                                         ("linux", "/dev/ttyUSB0", "COM4")):
            with patch.object(serial_port.sys, "platform", platform), \
                    patch.object(serial_port.list_ports, "comports") as enumerate_ports:
                self.assertEqual(serial_port.resolve_serial_port(valid), valid)
                enumerate_ports.assert_not_called()
                with self.assertRaisesRegex(serial.SerialException, "cannot open"):
                    serial_port.resolve_serial_port(invalid)

    def test_open_port_platform_options_and_actionable_error(self):
        for platform in ("win32", "linux"):
            with patch.object(serial_port.sys, "platform", platform), \
                    patch.object(serial_port.serial, "Serial") as open_port:
                serial_port.open_nslink_port("test", 921600)
                self.assertEqual(open_port.call_args.kwargs.get("exclusive"),
                                 True if platform == "linux" else None)
                open_port.side_effect = serial.SerialException("busy")
                with self.assertRaisesRegex(serial.SerialException, "usbipd"):
                    serial_port.open_nslink_port("test", 921600)

    def test_open_port_defaults_to_a_nonblocking_ish_timeout(self):
        with patch.object(serial_port.serial, "Serial") as open_port:
            serial_port.open_nslink_port("/dev/ttyACM0", 921600)
            self.assertEqual(open_port.call_args.kwargs["timeout"], 0.2)
            self.assertEqual(open_port.call_args.kwargs["write_timeout"], serial_port.WRITE_TIMEOUT)


if __name__ == "__main__":
    unittest.main()
