"""Source-generation checks; these do not establish accelerator execution."""
import os
from pathlib import Path
import sys
import unittest
from unittest import mock
sys.path.insert(0, str(Path(__file__).resolve().parents[2] / "tools"))
import backends
import conformance

class AdapterTests(unittest.TestCase):
    def test_handwritten_launcher_uses_its_backend(self):
        routes = [route for route in backends.ROUTES if backends.adapter(route).__name__.endswith("backend_hip")]
        self.assertTrue(routes)
        for route in routes:
            with self.subTest(route=route):
                source = conformance.target_source([dict(pattern="{}", args=["7"])], [0], route)
                self.assertIn("execute_case", source)
                self.assertIn("FFMT_DEVICE", source)
                self.assertNotIn("#include <fmt/", source)
                self.assertIn('hipGetDeviceCount', source)
                self.assertIn('hipGetLastError', source)
                self.assertIn('hipDeviceSynchronize', source)
                self.assertIn('hipLaunchKernelGGL', source)

    def test_compiler_and_flags_match_the_selected_route(self):
        with mock.patch.dict(os.environ, {"FFMT_GPU_ARCH": "review_target", "FFMT_KOKKOS_SERIAL_PREFIX": "/review/kokkos"}, clear=True):
            self.assertEqual(backends.compiler('hip'), ['hipcc'])
            self.assertIn('--offload-arch=review_target', backends.flags('hip', 2))
            self.assertEqual(backends.compiler('chipstar'), ['hipcc'])
            self.assertIn('hip', backends.flags('chipstar', 2))
