"""Small checks for rejection diagnostics, reference integrity and coverage metrics."""
import importlib.util
import json
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import unittest
from unittest import mock

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))
import compile_diagnostics as runner

spec = importlib.util.spec_from_file_location("coverage_union", ROOT / ".github/scripts/coverage_union.py")
coverage = importlib.util.module_from_spec(spec)
spec.loader.exec_module(coverage)

class NegativeEvidenceTests(unittest.TestCase):
    def status(self, rc, diagnostic):
        return runner.negative_status(dict(exit_code=rc, stderr=diagnostic.encode()))

    def test_acceptance_is_a_failure(self):
        self.assertEqual(self.status(0, ""), "unexpected_acceptance")

    def test_missing_header_and_compiler_crashes_are_not_rejections(self):
        for diagnostic in ("fatal error: fmt/format.h: No such file or directory", "LLVM ERROR: broken constant expression", "PLEASE submit a bug report", "internal compiler error: consteval"):
            self.assertEqual(self.status(1, diagnostic), "infrastructure_failure")
        self.assertEqual(self.status(-11, ""), "infrastructure_failure")

    def test_unrelated_error_is_not_format_rejection(self):
        self.assertEqual(self.status(1, "error: undeclared identifier xyz"), "wrong_diagnostic")

    def test_format_rejection_is_recognized(self):
        self.assertEqual(self.status(1, "error: call to consteval function is not a constant expression: invalid_format_spec"), "expected_rejection")

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

@unittest.skipUnless(shutil.which("git") and shutil.which("make"), "Git/Make unavailable")
class OraclePinTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        self.addCleanup(self.temporary.cleanup)
        self.oracle = Path(self.temporary.name) / "fmt"
        self.oracle.mkdir()
        self.git("init", "--quiet")
        self.header = self.oracle / "include/fmt/format.h"
        self.header.parent.mkdir(parents=True)
        self.header.write_text("// reference fixture\n")
        self.git("add", ".")
        self.git("-c", "user.name=Test", "-c", "user.email=test@example.invalid",
                 "commit", "--quiet", "-m", "reference fixture")
        self.pin = self.git("rev-parse", "HEAD").strip()

    def git(self, *arguments):
        return subprocess.check_output(["git", "-C", str(self.oracle), *arguments],
                                       text=True, stderr=subprocess.PIPE)

    def verify(self, pin=None):
        return subprocess.run(["make", "--no-print-directory", "verify-fmt",
                               "FMT_DIR=" + str(self.oracle), "FMT_REF=" + (pin or self.pin)],
                              cwd=ROOT, capture_output=True, text=True)

    def test_clean_matching_checkout_is_accepted(self):
        result = self.verify()
        self.assertEqual(result.returncode, 0, result.stderr)

    def test_unavailable_revision_fails_without_checkout(self):
        before = self.git("rev-parse", "HEAD")
        result = self.verify("0" * 40)
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("does not match", result.stderr)
        self.assertEqual(self.git("rev-parse", "HEAD"), before)

    def test_local_changes_are_preserved_and_rejected(self):
        self.header.write_text("// local work must survive\n")
        result = self.verify()
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("local changes", result.stderr)
        self.assertEqual(self.header.read_text(), "// local work must survive\n")
