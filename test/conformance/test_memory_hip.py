"""Device-memory source generation is checked without claiming hardware execution."""
from pathlib import Path
import sys
import unittest
sys.path.insert(0, str(Path(__file__).resolve().parents[2] / "tools"))
import backends
import numeric_memory

class MemoryAdapterTests(unittest.TestCase):
    def test_numeric_records_use_the_selected_memory_backend(self):
        routes = [route for route in backends.ROUTES if backends.adapter(route).__name__.endswith("backend_hip")]
        self.assertTrue(routes)
        for route in routes:
            with self.subTest(route=route):
                source = numeric_memory.source(route, samples=0)
                self.assertNotIn("#include <fmt/", source)
                self.assertIn("format_record", source)
                self.assertIn("serialize(input, output)", source)
                self.assertIn('hipMalloc', source)
                self.assertIn('hipMemcpyHostToDevice', source)
                self.assertIn('hipMemcpyDeviceToHost', source)
                self.assertIn('hipDeviceSynchronize', source)
                self.assertIn('hipLaunchKernelGGL(memory_kernel', source)
