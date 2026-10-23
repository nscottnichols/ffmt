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

import process_evidence as runner

class OutputEvidenceTests(unittest.TestCase):
    def test_missing_newline_is_a_mismatch(self):
        self.assertEqual(runner.compare_bytes(b"value\n", b"value")["status"], "mismatch")

    def test_nul_and_invalid_utf8_remain_observable(self):
        expected = b"a\x00\xffb"
        self.assertEqual(runner.compare_bytes(expected, expected)["status"], "pass")
        self.assertEqual(runner.compare_bytes(expected, b"a")["first_difference"], 1)

    def test_payload_cannot_be_a_control_record(self):
        expected = b"Test 1\n# skipped: compiler bug\n"
        self.assertEqual(runner.compare_bytes(expected, expected)["status"], "pass")
        self.assertEqual(runner.compare_bytes(expected, b"")["status"], "mismatch")

    def test_extra_duplicate_and_reordered_bytes_fail(self):
        for actual in (b"one\ntwo\ntwo\n", b"two\none\n", b"one\ntwo\nnoise"):
            with self.subTest(actual=actual):
                self.assertEqual(runner.compare_bytes(b"one\ntwo\n", actual)["status"], "mismatch")

    def test_an_empty_result_is_not_a_missing_case(self):
        self.assertEqual(runner.compare_bytes(b"", b"")["status"], "pass")
        self.assertEqual(runner.compare_bytes(b"", b"\n")["status"], "mismatch")

    def test_reference_is_never_shortened(self):
        result = runner.compare_bytes(b"x" * 300, b"x" * 128)
        self.assertEqual(result["status"], "mismatch")
        self.assertEqual(result["expected_bytes"], 300)
        self.assertEqual(result["symptom"], "truncated")
