"""Coverage collection keeps source identity and runtime metric categories separate."""
import importlib.util
import json
from pathlib import Path
import tempfile
import unittest
from unittest import mock

spec = importlib.util.spec_from_file_location("coverage_union",
    Path(__file__).resolve().parents[2] / ".github/scripts/coverage_union.py")
coverage = importlib.util.module_from_spec(spec)
spec.loader.exec_module(coverage)

class CoverageMetricsTests(unittest.TestCase):
    def test_file_identity_and_real_executable_lines_are_preserved(self):
        with tempfile.TemporaryDirectory() as temporary:
            directory = Path(temporary)
            first = directory / "first.hpp"
            second = directory / "second.hpp"
            first.touch()
            second.touch()
            foreign = str(directory.parent / "foreign.hpp")
            payload = {"data": [{"functions": [
                {"filenames": [str(first), foreign], "regions": [
                    [1, 1, 3, 4, 0, 0, 0, 0], [1, 1, 3, 4, 9, 0, 0, 0],
                    [1, 1, 3, 4, 9, 1, 0, 0]]},
                {"filenames": [str(second)], "regions": [[1, 1, 3, 4, 0, 0, 0, 0]]}],
                "files": [{"filename": str(first), "branches": [[2, 1, 2, 8, 1, 0, 0, 0, 4]]},
                          {"filename": foreign, "branches": [[2, 1, 2, 8, 1, 1, 0, 0, 4]]}]}]}
            lcov = f"SF:{first}\nDA:1,1\nDA:3,0\nSF:{second}\nDA:1,0\nSF:{foreign}\nDA:1,1\n"
            with mock.patch.object(coverage.subprocess, "run"), mock.patch.object(coverage.subprocess, "check_output", side_effect=[json.dumps(payload).encode(), lcov.encode()]):
                result = coverage.regions("llvm-cov", "llvm-profdata", str(directory), "binary", temporary)
            self.assertEqual(coverage.metrics(result["regions"]), dict(covered=1, count=2, percent=50.0))
            self.assertEqual(coverage.metrics(result["lines"]), dict(covered=1, count=3, percent=100 / 3))
            self.assertEqual(coverage.metrics(result["branches"]), dict(covered=1, count=2, percent=50.0))

    def test_empty_coverage_is_explicit_instead_of_perfect(self):
        self.assertEqual(coverage.metrics({}), dict(covered=0, count=0, percent=None))
