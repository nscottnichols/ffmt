"""A dirty oracle cannot be relabeled as the pinned reference."""
import hashlib
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[2] / "tools"))
from oracle import oracle_identity

class OracleIdentityTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        self.addCleanup(self.temporary.cleanup)
        self.checkout = Path(self.temporary.name)
        self.header = self.checkout / "include/fmt/format.h"
        self.source = self.checkout / "src/format.cc"
        for path, content in ((self.header, b"reference header\n"), (self.source, b"reference implementation\n")):
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_bytes(content)
        self.git("init", "--quiet", "--template=")
        self.git("add", "include", "src")
        self.git("-c", "user.name=ffmt test", "-c", "user.email=ffmt-test@example.invalid",
                 "-c", "core.hooksPath=/dev/null", "commit", "--no-gpg-sign", "--quiet", "-m", "oracle fixture")
        self.pin = self.git("rev-parse", "HEAD")

    def git(self, *arguments):
        return subprocess.check_output(["git", "-C", str(self.checkout), *arguments], text=True).strip()

    def test_clean_inputs_retain_their_exact_hashes(self):
        result = oracle_identity(self.checkout, self.pin)
        self.assertEqual(result["commit"], self.pin)
        self.assertEqual(result["format_header_sha256"], hashlib.sha256(self.header.read_bytes()).hexdigest())
        self.assertEqual(result["format_source_sha256"], hashlib.sha256(self.source.read_bytes()).hexdigest())

    def test_changed_reference_source_is_rejected_without_reverting_it(self):
        self.source.write_bytes(b"local reference edit\n")
        status = self.git("status", "--porcelain")
        with self.assertRaisesRegex(ValueError, "must be clean"):
            oracle_identity(self.checkout, self.pin)
        self.assertEqual(self.source.read_bytes(), b"local reference edit\n")
        self.assertEqual(self.git("status", "--porcelain"), status)
        self.assertEqual(self.git("rev-parse", "HEAD"), self.pin)

    def test_untracked_inputs_are_not_silently_accepted(self):
        local = self.checkout / "local-input.txt"
        local.write_bytes(b"keep this file\n")
        with self.assertRaisesRegex(ValueError, "must be clean"):
            oracle_identity(self.checkout, self.pin)
        self.assertEqual(local.read_bytes(), b"keep this file\n")

    def test_wrong_commit_is_rejected_without_checking_out_another_revision(self):
        with self.assertRaisesRegex(ValueError, "differs from oracle pin"):
            oracle_identity(self.checkout, "0" * 40)
        self.assertEqual(self.git("rev-parse", "HEAD"), self.pin)
