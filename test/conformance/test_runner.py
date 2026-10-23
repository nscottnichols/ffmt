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

class BackendEvidenceTests(unittest.TestCase):
    def test_unknown_case_cannot_silently_shrink_a_campaign(self):
        with mock.patch.object(sys, "argv", ["conformance.py", "--case", "literal", "--case", "typo"]), \
                mock.patch.object(sys, "stderr", io.StringIO()), \
                mock.patch.object(runner, "Campaign") as campaign:
            with self.assertRaises(SystemExit) as error:
                runner.main()
            self.assertEqual(error.exception.code, 2)
            campaign.assert_not_called()

    def test_unknown_route_cannot_become_host_execution(self):
        with self.assertRaises(ValueError):
            runner.target_source([], [], "misspelled-gpu")

    def test_shared_binary_failure_is_isolated_before_case_classification(self):
        with tempfile.TemporaryDirectory() as temporary:
            campaign = object.__new__(runner.Campaign)
            campaign.output = Path(temporary)
            campaign.args = SimpleNamespace(run_timeout=30, jit_timeout=600,
                compile_timeout=60, no_memory=True, no_negative=True, shard_size=16)
            campaign.manifest = {"configurations": []}
            checkpoints = []
            campaign.save = lambda: checkpoints.append(json.loads(json.dumps(campaign.manifest)))
            def process(command, directory, stem, *args):
                result = dict(exit_code=0, stdout=b"", stderr=b"", stderr_file="diagnostic")
                if stem == "control-run":
                    result.update(stdout=b"control 7", stderr=b"FFMT_DEVICE host\n")
                elif stem in ("one", "two"):
                    if Path(command[0]).name == "cases-0-1":
                        result["exit_code"] = 1
                    else:
                        result["stdout"] = stem.encode()
                return result
            cases = [dict(id=name, family="core", pattern=name, args=[]) for name in ("one", "two")]
            with mock.patch.object(runner, "run", side_effect=process), mock.patch("builtins.print"):
                campaign.configuration("host-buffer", 2, cases, [b"one", b"two"])
            config = campaign.manifest["configurations"][0]
            self.assertEqual([case["status"] for case in config["cases"]], ["pass", "pass"])
            self.assertEqual(config["shard_failures"][0]["status"], "execution_failure")
            saved_failure = next(point["configurations"][0] for point in checkpoints
                                 if point["configurations"][0].get("shard_failures"))
            self.assertEqual(saved_failure["cases"], [])
            self.assertEqual(saved_failure["shard_failures"][0]["status"], "execution_failure")
            self.assertEqual(config["stdout_status"], "completed")
            self.assertEqual(config["status"], "completed")

    def test_failed_build_shard_is_saved_before_isolation(self):
        for timed_out in (False, True):
            with self.subTest(timed_out=timed_out), tempfile.TemporaryDirectory() as temporary:
                campaign = object.__new__(runner.Campaign)
                campaign.output = Path(temporary)
                campaign.args = SimpleNamespace(run_timeout=30, jit_timeout=600,
                    compile_timeout=60, no_memory=True, no_negative=True, shard_size=16)
                campaign.manifest = {"configurations": []}
                checkpoints = []
                campaign.save = lambda: checkpoints.append(json.loads(json.dumps(campaign.manifest)))

                def process(command, directory, stem, *args):
                    result = dict(exit_code=0, stdout=b"", stderr=b"", stderr_file="diagnostic", command=[str(x) for x in command])
                    if stem == "control-run":
                        result.update(stdout=b"control 7", stderr=b"FFMT_DEVICE host\n")
                    elif stem == "cases-0-1-build":
                        result.update(exit_code=-1 if timed_out else 1, timeout=timed_out)
                    elif stem in ("one", "two"):
                        result["stdout"] = stem.encode()
                    return result

                cases = [dict(id=name, family="core", pattern=name, args=[]) for name in ("one", "two")]
                with mock.patch.object(runner, "run", side_effect=process), mock.patch("builtins.print"):
                    campaign.configuration("host-buffer", 2, cases, [b"one", b"two"])
                config = campaign.manifest["configurations"][0]
                self.assertEqual([case["status"] for case in config["cases"]], ["pass", "pass"])
                saved_failure = next(point["configurations"][0] for point in checkpoints
                                     if point["configurations"][0].get("shard_failures"))
                self.assertEqual(saved_failure["cases"], [])
                failure = saved_failure["shard_failures"][0]
                self.assertEqual(failure["stage"], "build")
                self.assertEqual(failure["case_ids"], ["one", "two"])
                self.assertEqual(failure["status"], "compile_timeout" if timed_out else "compile_failure")
                source = campaign.output / "host-buffer-O2/cases-0-1.cpp"
                self.assertEqual(failure["source_sha256"], hashlib.sha256(source.read_bytes()).hexdigest())
                self.assertIn(str(source), failure["build"]["command"])
