#!/usr/bin/env python3
# Copyright (c) 2025 Nations Technologies Inc.
# SPDX-License-Identifier: Apache-2.0

"""Locate and open the NSLink CDC ACM port.

Extracted from the retired camera capture client so that the port-detection
rules -- which are not camera-specific -- survive it.  NSLink is selected by
USB VID/PID: COMx in Windows, /dev/ttyACMx in Linux/WSL.  --port overrides
detection.  USB/IP ownership must be switched outside this program before
changing OS.

The bridge takes its UART rate from this side's line coding, so the baud passed
to open_nslink_port() must match the firmware's USART_Configuration() or the
link carries nothing at all.  A mismatch is *silent*, not garbled, which makes
it easy to blame the wrong end.
"""
import sys

import serial
from serial.tools import list_ports

NSLINK_USB_ID = (0x19F5, 0x3106)
DEFAULT_BAUD = 921600
WRITE_TIMEOUT = 1.0

SERIAL_OPEN_ERRORS = (serial.SerialException, OSError)
if sys.platform != "win32":
    import termios
    SERIAL_OPEN_ERRORS += (termios.error,)


def port_ownership_hint():
    if sys.platform == "win32":
        return ("Windows Python needs a COM port owned by Windows. If NSLink is "
                "attached to WSL, check 'usbipd list' and run "
                "'usbipd detach --busid <BUSID>' in Windows first.")
    return ("Linux/WSL Python needs a /dev/ttyACM* or /dev/ttyUSB* port. "
            "On WSL, check 'usbipd list' and attach NSLink with "
            "'usbipd attach --wsl --busid <BUSID>' in Windows first.")


def describe_ports(ports):
    return "; ".join(f"{p.device} ({p.description or 'unknown'}, {p.hwid or 'no ID'})"
                     for p in ports) or "none"


def resolve_serial_port(requested=None):
    if requested and requested.lower() != "auto":
        if sys.platform == "win32" and requested.startswith("/dev/"):
            raise serial.SerialException("Windows Python cannot open a Linux device path; "
                                         "omit --port for auto-detection or use --port COMx.")
        if sys.platform != "win32" and requested.upper().startswith("COM") and requested[3:].isdigit():
            raise serial.SerialException("Linux Python cannot open a Windows COM name; "
                                         "omit --port for auto-detection. " + port_ownership_hint())
        return requested
    ports = sorted(list_ports.comports(), key=lambda p: p.device)
    matches = [p for p in ports if (p.vid, p.pid) == NSLINK_USB_ID]
    if not matches:
        matches = [p for p in ports if "nslink" in
                   " ".join(filter(None, (p.description, p.product, p.manufacturer))).lower()]
    if len(matches) == 1:
        return matches[0].device
    if len(matches) > 1:
        raise serial.SerialException("Multiple NSLink serial ports found; choose --port: "
                                     + describe_ports(matches))
    raise serial.SerialException("No NSLink serial port detected. Available: "
                                 + describe_ports(ports) + ". " + port_ownership_hint()
                                 + " Other adapters require an explicit --port.")


def open_nslink_port(device, baud, timeout=0.2):
    """Open the port, converting an OS-level failure into an actionable one.

    `exclusive` is Linux-only and stops a second reader from stealing bytes;
    on Windows the OS already grants exclusive access.
    """
    options = dict(timeout=timeout, write_timeout=WRITE_TIMEOUT)
    if sys.platform != "win32":
        options["exclusive"] = True
    try:
        return serial.Serial(device, baud, **options)
    except SERIAL_OPEN_ERRORS as exc:
        raise serial.SerialException(f"Cannot open {device} at {baud} baud: {exc}. "
                                     + port_ownership_hint()) from exc
