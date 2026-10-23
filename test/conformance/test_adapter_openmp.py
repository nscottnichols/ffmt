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
        routes = [route for route in backends.ROUTES if backends.adapter(route).__name__.endswith("backend_openmp")]
        self.assertTrue(routes)
        for route in routes:
            with self.subTest(route=route):
                source = conformance.target_source([dict(pattern="{}", args=["7"])], [0], route)
                self.assertIn("execute_case", source)
                self.assertIn("FFMT_DEVICE", source)
                self.assertNotIn("#include <fmt/", source)
                self.assertIn('omp_is_initial_device', source)
                self.assertIn('if (initial_device) return 77', source)
                self.assertIn('#pragma omp target', source)

    def test_compiler_and_flags_match_the_selected_route(self):
        with mock.patch.dict(os.environ, {"FFMT_GPU_ARCH": "review_target", "FFMT_KOKKOS_SERIAL_PREFIX": "/review/kokkos"}, clear=True):
            self.assertEqual(backends.compiler('omp-cuda'), ['clang++'])
            self.assertIn('--offload-arch=review_target', backends.flags('omp-cuda', 2))
            self.assertEqual(backends.compiler('omp-hip'), ['clang++'])
            self.assertIn('--offload-arch=review_target', backends.flags('omp-hip', 2))
            self.assertEqual(backends.compiler('omp-spirv'), ['icpx'])
            self.assertIn('-fopenmp-targets=spir64', backends.flags('omp-spirv', 2))
