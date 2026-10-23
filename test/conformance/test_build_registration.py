"""Declared tests, registration edits, and optional outcomes stay observable."""
import os
from pathlib import Path
import shutil
import subprocess
import tempfile
import time
import unittest

ROOT = Path(__file__).resolve().parents[2]


@unittest.skipUnless(shutil.which("make"), "GNU Make is unavailable")
class RegistrationTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        self.addCleanup(self.temporary.cleanup)
        self.directory = Path(self.temporary.name)
        self.source = self.directory / "present.cpp"
        self.source.write_text("int main() { return 0; }\n")
        (self.directory / "build").mkdir()
        (self.directory / "stamp").touch()
        self.registration = self.directory / "test/conformance/suites/declared.mk"
        self.registration.parent.mkdir(parents=True)
        self.registration.write_text("FFMT_FLAGS_declared := -DORIGINAL\n")
        (self.directory / "Makefile").write_text(
            "SHELL := /bin/bash\nFLAGS_STAMP := stamp\n"
            "stamp build/.dir:\n\ttouch $@\n"
            "include " + str(ROOT / "mk/portable-tests.mk") + "\n")

    def make(self, target, *, source=None, optional=False, dry_run=True):
        command = ["make"] + (["-n"] if dry_run else []) + [
            target, "FFMT_CAPABILITIES=declared",
            "FFMT_SOURCE_declared=" + str(source or self.source),
            "FFMT_STANDARD_declared=c++20",
            "FFMT_OPTIONAL_declared=" + str(int(optional))]
        return subprocess.run(command, cwd=self.directory, capture_output=True, text=True)

    def cached_binary(self, return_code=0):
        binary = self.directory / "build/cap_declared"
        binary.write_text("#!/bin/sh\nexit " + str(return_code) + "\n")
        binary.chmod(0o755)
        stamp = time.time() + 60
        os.utime(binary, (stamp, stamp))
        return stamp

    def test_missing_declared_source_is_an_error(self):
        result = self.make("build/cap_declared", source=self.directory / "missing.cpp")
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("missing.cpp", result.stderr)

    def test_present_declared_source_is_compiled(self):
        result = self.make("build/cap_declared")
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertIn(str(self.source), result.stdout)
        self.assertIn("-o build/cap_declared", result.stdout)

    def test_registration_edit_invalidates_cached_binary(self):
        stamp = self.cached_binary()
        before = self.make("build/cap_declared")
        self.assertEqual(before.returncode, 0, before.stderr)
        self.assertNotIn(str(self.source), before.stdout)
        self.registration.write_text("FFMT_FLAGS_declared := -DCHANGED\n")
        os.utime(self.registration, (stamp + 1, stamp + 1))
        after = self.make("build/cap_declared")
        self.assertEqual(after.returncode, 0, after.stderr)
        self.assertIn("-DCHANGED", after.stdout)
        self.assertIn(str(self.source), after.stdout)

    def test_mandatory_exit_77_is_a_failure(self):
        self.cached_binary(77)
        result = self.make("test-capabilities", dry_run=False)
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("FAIL (77)", result.stdout)

    def test_explicit_optional_exit_77_is_a_skip(self):
        self.cached_binary(77)
        result = self.make("test-capabilities", optional=True, dry_run=False)
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertIn("SKIP", result.stdout)


if __name__ == "__main__":
    unittest.main()
