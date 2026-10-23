"""Device-memory source generation is checked without claiming hardware execution."""
from pathlib import Path
import sys
import unittest
sys.path.insert(0, str(Path(__file__).resolve().parents[2] / "tools"))
import backends
import numeric_memory

class MemoryAdapterTests(unittest.TestCase):
    def test_numeric_records_use_the_selected_memory_backend(self):
        routes = [route for route in backends.ROUTES if backends.adapter(route).__name__.endswith("backend_openmp")]
        self.assertTrue(routes)
        for route in routes:
            with self.subTest(route=route):
                source = numeric_memory.source(route, samples=0)
                self.assertNotIn("#include <fmt/", source)
                self.assertIn("format_record", source)
                self.assertIn("serialize(input, output)", source)
                self.assertIn('omp_is_initial_device', source)
                self.assertIn('if (initial_device) return 77', source)
                self.assertIn('map(to: device_input[0:count])', source)
                self.assertIn('map(from: device_output[0:count])', source)
