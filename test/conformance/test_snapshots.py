"""Focused regressions for portable conformance infrastructure."""
import hashlib
import importlib.util
import io
import json
import os
from pathlib import Path
import signal
import struct
import subprocess
import sys
import tempfile
import time
from types import SimpleNamespace
import unittest
from unittest import mock

sys.path.insert(0, str(Path(__file__).resolve().parents[2] / "tools"))

import conformance as runner

class PortableConfigurationTests(unittest.TestCase):
    def test_default_profiles_need_no_campaign_files(self):
        config = runner.load_profiles()
        self.assertEqual(config["fmt_commit"], runner.FMT_COMMIT)
        self.assertEqual(config["profiles"]["local"]["routes"],
                         ["host-buffer", "host-specifiers"])

    def test_external_profiles_cannot_change_oracle_or_invent_routes(self):
        with tempfile.TemporaryDirectory() as temporary:
            path = Path(temporary) / "profiles.json"
            for overrides in ({"fmt_commit": "e" * 40},
                              {"profiles": {"custom": {"routes": ["misspelled-gpu"]}}}):
                data = dict(fmt_commit=runner.FMT_COMMIT,
                            profiles={"custom": {"routes": ["host-buffer"]}})
                data.update(overrides)
                path.write_text(json.dumps(data))
                with self.assertRaises(ValueError):
                    runner.load_profiles(path)

    def test_source_checkout_and_pinned_snapshot_are_independent_of_runner(self):
        with tempfile.TemporaryDirectory() as temporary:
            directory = Path(temporary)
            source = directory / "source"
            source.mkdir()
            subprocess.run(["git", "init", "--quiet", "--template=", str(source)], check=True)
            header = source / "include/ffmt/base.hpp"
            header.parent.mkdir(parents=True)
            header.write_bytes(b"qualified header\n")
            subprocess.run(["git", "-C", str(source), "add", "include"], check=True)
            subprocess.run(["git", "-C", str(source), "-c", "user.name=ffmt test",
                "-c", "user.email=ffmt-test@example.invalid", "-c", "core.hooksPath=/dev/null",
                "commit", "--no-gpg-sign", "--quiet", "-m", "source fixture"], check=True)
            pin = subprocess.check_output(["git", "-C", str(source), "rev-parse", "HEAD"], text=True).strip()
            header.write_bytes(b"subsequent development header\n")
            with mock.patch.object(runner, "SOURCE_ROOT", source):
                hashes = runner.snapshot(directory / "pinned", pin)
                live_hashes = runner.snapshot(directory / "worktree", "WORKTREE")
                self.assertEqual(runner.source_git("rev-parse", "HEAD"), pin)
            self.assertEqual((directory / "pinned/include/ffmt/base.hpp").read_bytes(),
                             b"qualified header\n")
            self.assertEqual(hashes["include/ffmt/base.hpp"],
                             hashlib.sha256(b"qualified header\n").hexdigest())
            self.assertNotEqual(hashes, live_hashes)
