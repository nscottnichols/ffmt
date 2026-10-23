"""Focused regressions for portable conformance infrastructure."""
import hashlib
import importlib.util
import io
import json
import os
from pathlib import Path
import signal
import struct
import subprocess
import sys
import tempfile
import time
from types import SimpleNamespace
import unittest
from unittest import mock

sys.path.insert(0, str(Path(__file__).resolve().parents[2] / "tools"))

import numeric_memory

class MemoryEvidenceTests(unittest.TestCase):
    def fixture(self):
        return struct.pack("<II", numeric_memory.MAGIC, 1) + struct.pack("<IQIII", 64, 0, 3, 3, 0) + b"a\0b"

    def test_binary_payload_is_preserved(self):
        value = self.fixture()
        self.assertEqual(numeric_memory.compare_records(value, value)["passed"], 1)

    def test_truncated_missing_and_extra_records_fail(self):
        value = self.fixture()
        for actual in (value[:-1], value[:8], value + value[8:]):
            with self.subTest(actual=actual):
                with self.assertRaises(ValueError):
                    numeric_memory.compare_records(value, actual)

    def test_corrupt_header_cannot_pass(self):
        with self.assertRaises(ValueError):
            numeric_memory.parse_records(b"not a record")

    def test_counts_and_error_status_are_compared(self):
        expected = self.fixture()
        actual = bytearray(expected)
        actual[28] = 1  # formatting error even though the payload still matches
        self.assertEqual(numeric_memory.compare_records(expected, actual)["failed"], 1)
