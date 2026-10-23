"""Reference builds verify existing oracle sources without modifying them."""
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]


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


if __name__ == "__main__":
    unittest.main()
