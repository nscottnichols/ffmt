"""Device-memory source generation is checked without claiming hardware execution."""
from pathlib import Path
import sys
import unittest
sys.path.insert(0, str(Path(__file__).resolve().parents[2] / "tools"))
import backends
import numeric_memory

class MemoryAdapterTests(unittest.TestCase):
    def test_numeric_records_use_the_selected_memory_backend(self):
        routes = [route for route in backends.ROUTES if backends.adapter(route).__name__.endswith("backend_cuda")]
        self.assertTrue(routes)
        for route in routes:
            with self.subTest(route=route):
                source = numeric_memory.source(route, samples=0)
                self.assertNotIn("#include <fmt/", source)
                self.assertIn("format_record", source)
                self.assertIn("serialize(input, output)", source)
                self.assertIn('cudaMalloc', source)
                self.assertIn('cudaMemcpyHostToDevice', source)
                self.assertIn('cudaMemcpyDeviceToHost', source)
                self.assertIn('cudaDeviceSynchronize', source)
                self.assertIn('memory_kernel<<<', source)
