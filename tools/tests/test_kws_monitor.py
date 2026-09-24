# Copyright (c) 2025 Nations Technologies Inc.
# SPDX-License-Identifier: Apache-2.0

"""Line reassembly for the keyword-spotter serial reader.

The only thing here worth testing is split_lines: it sits between a byte stream
that arrives in arbitrary chunks and a terminal that wants whole lines, so the
cases that matter are a line straddling two reads and CRLF arriving intact.
"""
from pathlib import Path
import sys
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import kws_monitor


class SplitLinesTests(unittest.TestCase):
    def test_crlf_is_one_terminator_not_two(self):
        lines, rest = kws_monitor.split_lines(b"", b"yes 92%\r\nno 71%\r\n")
        self.assertEqual(lines, [b"yes 92%", b"no 71%"])
        self.assertEqual(rest, b"")

    def test_incomplete_line_is_carried_over(self):
        lines, rest = kws_monitor.split_lines(b"", b"yes 9")
        self.assertEqual(lines, [])
        self.assertEqual(rest, b"yes 9")
        lines, rest = kws_monitor.split_lines(rest, b"2%\r\n")
        self.assertEqual(lines, [b"yes 92%"])
        self.assertEqual(rest, b"")

    def test_several_lines_in_one_chunk_keep_their_order(self):
        lines, _ = kws_monitor.split_lines(b"", b"a\r\nb\r\nc\r\n")
        self.assertEqual(lines, [b"a", b"b", b"c"])

    def test_bare_newline_also_ends_a_line(self):
        lines, rest = kws_monitor.split_lines(b"", b"a\nb\n")
        self.assertEqual(lines, [b"a", b"b"])
        self.assertEqual(rest, b"")

    def test_crlf_split_across_two_reads_still_makes_one_line(self):
        # The CR is left pending and the LF that arrives next must complete the
        # same line, not start an empty one.  This is the case the first version
        # of this function got wrong: it treated both bytes as terminators, so
        # every CRLF produced a blank line.
        lines, rest = kws_monitor.split_lines(b"", b"yes 92%\r")
        self.assertEqual(lines, [])
        self.assertEqual(rest, b"yes 92%\r")
        lines, rest = kws_monitor.split_lines(rest, b"\n")
        self.assertEqual(lines, [b"yes 92%"])
        self.assertEqual(rest, b"")

    def test_a_blank_line_between_reports_is_still_reported(self):
        lines, rest = kws_monitor.split_lines(b"", b"yes 92%\r\n\r\nno 71%\r\n")
        self.assertEqual(lines, [b"yes 92%", b"", b"no 71%"])
        self.assertEqual(rest, b"")

    def test_a_carriage_return_alone_does_not_end_a_line(self):
        lines, rest = kws_monitor.split_lines(b"", b"yes 92%\rno 71%\r\n")
        self.assertEqual(lines, [b"yes 92%\rno 71%"])
        self.assertEqual(rest, b"")


if __name__ == "__main__":
    unittest.main()
