"""Target overrides never query hardware; discovery failures use explicit defaults."""
import os
from pathlib import Path
import subprocess
import sys
import unittest
from unittest import mock
sys.path.insert(0, str(Path(__file__).resolve().parents[2] / "tools"))
import device_arch

class DeviceArchitectureTests(unittest.TestCase):
    def test_explicit_override_avoids_device_queries(self):
        with mock.patch.dict(os.environ, {"FFMT_GPU_ARCH": "selected-target"}), mock.patch.object(device_arch.subprocess, "check_output") as query:
            self.assertEqual(device_arch.cuda_architecture(), "selected-target")
            self.assertEqual(device_arch.hip_architecture(), "selected-target")
            query.assert_not_called()

    def test_unavailable_queries_use_compile_defaults(self):
        with mock.patch.dict(os.environ, {}, clear=True), mock.patch.object(device_arch.subprocess, "check_output", side_effect=OSError("no device tool")):
            self.assertEqual(device_arch.cuda_architecture(), "sm_90")
            self.assertEqual(device_arch.hip_architecture(), "gfx942")

    def test_visible_device_queries_select_the_first_target(self):
        with mock.patch.dict(os.environ, {}, clear=True), mock.patch.object(device_arch.subprocess, "check_output", side_effect=[b"8.0\n9.0\n", b"gfx90a\ngfx942\n"]):
            self.assertEqual(device_arch.cuda_architecture(), "sm_80")
            self.assertEqual(device_arch.hip_architecture(), "gfx90a")
