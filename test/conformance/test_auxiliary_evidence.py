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
    def test_stdout_control_failure_does_not_hide_memory_results(self):
        for compile_failure in (False, True):
            with self.subTest(compile_failure=compile_failure), tempfile.TemporaryDirectory() as temporary:
                campaign = object.__new__(runner.Campaign)
                campaign.output = Path(temporary)
                header = campaign.output / "source/include/ffmt/core.hpp"
                header.parent.mkdir(parents=True)
                header.touch()
                campaign.args = SimpleNamespace(run_timeout=30, jit_timeout=600,
                    compile_timeout=60, no_memory=False)
                campaign.manifest = {"configurations": []}
                campaign.save = mock.Mock()
                def memory(cmd, directory, route, config):
                    self.assertNotIn("status", config)
                    config["numeric_memory"] = {"status": "execution_failure"}
                campaign.memory_suite = mock.Mock(side_effect=memory)
                ok = dict(exit_code=0, stdout=b"", stderr=b"", stderr_file="diagnostic")
                failed = dict(ok, exit_code=1)
                processes = [ok, failed] if compile_failure else [ok, ok, failed]
                with mock.patch.object(runner, "run", side_effect=processes), mock.patch("builtins.print"):
                    campaign.configuration("host-buffer", 2,
                        [dict(id="case", family="core")], [b"expected"])
                campaign.memory_suite.assert_called_once()
                config = campaign.manifest["configurations"][0]
                self.assertEqual(config["cases"][0]["status"], "not_run")
                self.assertEqual(config["numeric_memory"]["status"], "execution_failure")
                self.assertEqual(config["stdout_status"], "control_compile_failure" if compile_failure else "control_execution_failure")
                self.assertEqual(config["status"], config["stdout_status"])

    def test_auxiliary_exception_leaves_configuration_unfinished(self):
        for control_failure, failed_suite in ((None, "memory"), ("compile", "memory"),
                                               ("execution", "memory"), (None, "negative")):
            with self.subTest(control_failure=control_failure, failed_suite=failed_suite), \
                    tempfile.TemporaryDirectory() as temporary:
                campaign = object.__new__(runner.Campaign)
                campaign.output = Path(temporary)
                header = campaign.output / "source/include/ffmt/core.hpp"
                header.parent.mkdir(parents=True)
                header.touch()
                campaign.args = SimpleNamespace(run_timeout=30, jit_timeout=600,
                    compile_timeout=60, no_memory=False, no_negative=False, shard_size=1)
                campaign.manifest = {"configurations": []}
                checkpoints = []
                campaign.save = lambda: checkpoints.append(json.loads(json.dumps(campaign.manifest)))

                def process(command, directory, stem, *args):
                    result = dict(exit_code=0, stdout=b"", stderr=b"", stderr_file="diagnostic")
                    if stem == "control-build" and control_failure == "compile":
                        result["exit_code"] = 1
                    elif stem == "control-run":
                        result.update(stdout=b"control 7", stderr=b"FFMT_DEVICE host\n")
                        if control_failure == "execution":
                            result["exit_code"] = 1
                    elif stem == "literal":
                        result["stdout"] = b"expected"
                    return result

                def auxiliary(name, cmd, directory, route, config):
                    self.assertNotIn("status", config)
                    self.assertEqual(checkpoints[-1]["configurations"][0]["stdout_status"],
                                     config["stdout_status"])
                    if name == failed_suite:
                        raise RuntimeError("auxiliary failure")

                campaign.memory_suite = lambda *args: auxiliary("memory", *args)
                campaign.negatives = lambda *args: auxiliary("negative", *args)
                with mock.patch.object(runner, "run", side_effect=process), mock.patch("builtins.print"), \
                        self.assertRaisesRegex(RuntimeError, "auxiliary failure"):
                    campaign.configuration("host-buffer", 2,
                        [dict(id="literal", family="core", pattern="expected", args=[])], [b"expected"])
                config = campaign.manifest["configurations"][0]
                self.assertNotIn("status", config)
                expected = "control_" + control_failure + "_failure" if control_failure else "completed"
                self.assertEqual(config["stdout_status"], expected)
