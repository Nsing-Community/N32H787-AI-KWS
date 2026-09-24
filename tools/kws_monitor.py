#!/usr/bin/env python3
# Copyright (c) 2025 Nations Technologies Inc.
# SPDX-License-Identifier: Apache-2.0

"""Print what the keyword spotter sends on USART1.

The firmware prints one line per detected keyword (`yes 92%`) plus a banner of
class names at boot.  This is the only host-side reader left: the camera and M4
audio clients that used to own the serial port were retired with their
peripherals, so what remains here is just the port plumbing from
serial_port.py and a line loop.

NSLink exposes the CMSIS-DAP probe and the USART1 bridge as one USB device, so
the port is the same one OpenOCD talks through.  Reading it does not stop
flashing, but a flash run resets the target and the port will print whatever
the new image says at startup.

The bridge takes its UART rate from this side's line coding, so the baud here
must match USART_Configuration() in the firmware.  A mismatch is silent, not
garbled -- if nothing arrives, check the rate before blaming the firmware.
"""
import argparse
import sys
import time
from pathlib import Path

import serial
from serial.tools import list_ports

sys.path.insert(0, str(Path(__file__).resolve().parent))

from serial_port import (DEFAULT_BAUD, describe_ports, open_nslink_port,
                         port_ownership_hint, resolve_serial_port)


def split_lines(pending, chunk):
    """Split `pending + chunk` into complete lines, returning (lines, rest).

    The firmware ends every line with \r\n, and only \n terminates here: a
    lone \r is kept pending until its \n arrives, because treating both as
    terminators would emit an empty line for the \r half of every CRLF.
    """
    data = pending + chunk
    # A buffer that opens with CRLF is a CR left pending by the previous read
    # plus its LF; consume the pair rather than letting the LF end an empty
    # line.
    start = 2 if data[:2] == b"\r\n" else 0
    lines = []
    for index in range(start, len(data)):
        if data[index] != 0x0A:
            continue
        end = index - 1 if index > start and data[index - 1] == 0x0D else index
        lines.append(data[start:end])
        start = index + 1
    return lines, data[start:]


def stream(port, seconds=None, out=sys.stdout):
    """Print every complete line the port produces.  Returns the line count."""
    pending = b""
    count = 0
    deadline = None if seconds is None else time.monotonic() + seconds
    try:
        while deadline is None or time.monotonic() < deadline:
            chunk = port.read(256)
            if not chunk:
                continue
            lines, pending = split_lines(pending, chunk)
            for line in lines:
                text = line.decode("utf-8", "replace")
                out.write(text + "\n")
                out.flush()
                count += 1
        if pending:
            # Bounded runs end where the timer says, not where the firmware
            # happens to have finished a line; print the remainder rather than
            # losing it.
            out.write(pending.decode("utf-8", "replace") + "\n")
            out.flush()
            count += 1
    except KeyboardInterrupt:
        pass
    return count


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--port", default=None,
                        help="serial device (COMx or /dev/ttyACMx); default: auto-detect NSLink")
    parser.add_argument("--baud", type=int, default=DEFAULT_BAUD,
                        help=f"default {DEFAULT_BAUD}, must match the firmware")
    parser.add_argument("--seconds", type=float, default=None,
                        help="stop after this long instead of running until Ctrl-C")
    parser.add_argument("--log", default=None,
                        help="also append every line to this file")
    parser.add_argument("--list-ports", action="store_true",
                        help="list the ports this host can see, then exit")
    args = parser.parse_args(argv)

    if args.list_ports:
        print(describe_ports(list_ports.comports()))
        print(port_ownership_hint())
        return 0

    try:
        device = resolve_serial_port(args.port)
    except serial.SerialException as exc:
        print(f"error: {exc}", file=sys.stderr)
        return 1

    try:
        port = open_nslink_port(device, args.baud)
    except serial.SerialException as exc:
        print(f"error: {exc}", file=sys.stderr)
        return 1

    log = open(args.log, "a", buffering=1) if args.log else None
    print(f"# {device} at {args.baud} 8N1 -- Ctrl-C to stop")
    if log:
        print(f"# logging to {args.log}", file=sys.stderr)
    try:
        with port:
            count = stream(port, args.seconds,
                           out=Multiplexer(sys.stdout, log) if log else sys.stdout)
    finally:
        if log:
            log.close()
    print(f"# {count} line(s)", file=sys.stderr)
    return 0


class Multiplexer:
    """Writes to the terminal and to the log file alike."""

    def __init__(self, *streams):
        self.streams = streams

    def write(self, text):
        for stream in self.streams:
            stream.write(text)

    def flush(self):
        for stream in self.streams:
            stream.flush()


if __name__ == "__main__":
    sys.exit(main())
