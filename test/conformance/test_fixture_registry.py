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

class FixtureRegistryTests(unittest.TestCase):
    def test_every_loaded_fixture_has_a_source_hash(self):
        spec = importlib.util.spec_from_file_location("fixture_registry",
            Path(__file__).parent / "cases.py")
        fixtures = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(fixtures)
        hashes = fixtures.source_hashes()
        self.assertIn("cases.py", hashes)
        self.assertIn("case_support.py", hashes)
        for path in Path(__file__).parent.glob("cases_*.py"):
            self.assertEqual(hashes[path.name], hashlib.sha256(path.read_bytes()).hexdigest())
        identifiers = [case["id"] for case in fixtures.CASES]
        self.assertEqual(len(identifiers), len(set(identifiers)))
