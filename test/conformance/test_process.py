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

import process_evidence as runner

class ProcessLifetimeTests(unittest.TestCase):
    def test_external_workspace_preserves_absolute_artifact_paths(self):
        with tempfile.TemporaryDirectory() as root, tempfile.TemporaryDirectory() as output, \
                mock.patch.object(runner, "ROOT", Path(root)):
            result = runner.run([sys.executable, "-c", "print('portable')"],
                                Path(output), "external", timeout=10)
            self.assertEqual(result["exit_code"], 0)
            path = Path(result["stdout_file"])
            self.assertTrue(path.is_absolute())
            self.assertEqual(path.read_bytes(), b"portable\n")

    def test_binary_output_and_nonzero_exit_are_preserved(self):
        with tempfile.TemporaryDirectory() as temporary, \
                mock.patch.object(runner, "ROOT", Path(temporary)):
            result = runner.run([sys.executable, "-c",
                "import os; os.write(1,b'out\\x00\\xff'); os.write(2,b'err\\x00\\xfe'); raise SystemExit(7)"],
                Path(temporary), "binary", timeout=10)
            self.assertEqual(result["exit_code"], 7)
            self.assertNotIn("timeout", result)
            for name, expected in (("stdout", b"out\x00\xff"), ("stderr", b"err\x00\xfe")):
                self.assertEqual(result[name], expected)
                self.assertEqual((Path(temporary) / result[name + "_file"]).read_bytes(), expected)

    @unittest.skipUnless(hasattr(os, "killpg"), "requires POSIX process groups")
    def test_timeout_kills_spawned_child_and_grandchild(self):
        def running(pid):
            try:
                os.kill(pid, 0)
            except ProcessLookupError:
                return False
            # An orphan can briefly remain as a zombie before init reaps it.
            # It is already dead and cannot keep a compiler or pipe alive.
            status = subprocess.run(["ps", "-p", str(pid), "-o", "stat="],
                                    stdout=subprocess.PIPE, stderr=subprocess.PIPE)
            state = status.stdout.strip()
            return bool(state) and not state.startswith(b"Z")

        with tempfile.TemporaryDirectory() as temporary, \
                mock.patch.object(runner, "ROOT", Path(temporary)):
            directory = Path(temporary)
            parent_file, child_file, grandchild_file = [directory / (name + ".json")
                for name in ("parent", "child", "grandchild")]
            save = "from pathlib import Path; import json,os,signal,time; signal.signal(signal.SIGTERM,signal.SIG_IGN); "
            grandchild = save + ("os.write(1,b'grandchild\\x00\\xff'); os.write(2,b'diagnostic\\x00\\xfe'); "
                f"Path({str(grandchild_file)!r}).write_text(json.dumps([os.getpid(),os.getpgrp()])); time.sleep(60)")
            child = save + (f"Path({str(child_file)!r}).write_text(json.dumps([os.getpid(),os.getpgrp()])); "
                f"import subprocess,sys; subprocess.Popen([sys.executable,'-c',{grandchild!r}]); time.sleep(60)")
            parent = save + (f"Path({str(parent_file)!r}).write_text(json.dumps([os.getpid(),os.getpgrp()])); "
                f"import subprocess,sys; subprocess.Popen([sys.executable,'-c',{child!r}]); time.sleep(60)")
            try:
                result = runner.run([sys.executable, "-c", parent], directory, "timeout", timeout=3)
                self.assertEqual(result["exit_code"], -1)
                self.assertTrue(result["timeout"])
                self.assertEqual(result["stdout"], b"grandchild\x00\xff")
                self.assertEqual(result["stderr"], b"diagnostic\x00\xfe")
                parent_pid, parent_group = json.loads(parent_file.read_text())
                descendants = [json.loads(path.read_text()) for path in (child_file, grandchild_file)]
                self.assertEqual(parent_pid, parent_group)
                for pid, group in descendants:
                    self.assertEqual(group, parent_group)
                    deadline = time.monotonic() + 3
                    while running(pid) and time.monotonic() < deadline:
                        time.sleep(0.02)
                    self.assertFalse(running(pid), f"descendant {pid} survived timeout")
                for name in ("stdout", "stderr"):
                    self.assertEqual((directory / result[name + "_file"]).read_bytes(), result[name])
            finally:
                # Also clean descendants if this regression is run against an
                # implementation that only kills the immediate parent.
                for path in (child_file, grandchild_file):
                    if path.exists():
                        pid, _ = json.loads(path.read_text())
                        try:
                            os.kill(pid, signal.SIGKILL)
                        except ProcessLookupError:
                            pass
