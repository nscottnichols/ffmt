"""Measurement protocol rejects malformed or ambiguous sample records."""
import json
from pathlib import Path
import sys
import unittest
sys.path.insert(0, str(Path(__file__).resolve().parents[2] / "tools"))
import benchmark

class BenchmarkProtocolTests(unittest.TestCase):
    def record(self, count=30):
        return dict(schema_version=1, samples_ns=[10] * count,
                    warmups=5, iterations=2, calls=1)

    def process(self, records):
        return dict(stderr=b"".join(json.dumps(record, separators=(",", ":")).encode() + b"\n" for record in records))

    def test_a_missing_or_duplicate_protocol_record_fails(self):
        for records in ([], [self.record(), self.record()]):
            with self.subTest(records=len(records)), self.assertRaises(ValueError):
                benchmark.metrics(self.process(records))

    def test_incomplete_sample_batches_fail(self):
        with self.assertRaises(ValueError):
            benchmark.metrics(self.process([self.record(29)]))

    def test_sample_normalization_uses_iterations_and_calls(self):
        metrics = benchmark.metrics(self.process([self.record()]))
        self.assertEqual(metrics["median_ns"], 10)
        self.assertEqual(metrics["median_ns_per_call"], 5)
