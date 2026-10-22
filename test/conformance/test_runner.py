"""Regressions for false passes previously possible in the output harness."""
import importlib.util
import hashlib
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
import numeric_memory
import report as evidence_report

spec = importlib.util.spec_from_file_location("conformance", str(Path(__file__).resolve().parents[2] / "tools/conformance.py"))
runner = importlib.util.module_from_spec(spec)
spec.loader.exec_module(runner)


class ProcessLifetimeTests(unittest.TestCase):
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


class OutputEvidenceTests(unittest.TestCase):
    def test_missing_newline_is_a_mismatch(self):
        self.assertEqual(runner.compare_bytes(b"value\n", b"value")["status"], "mismatch")

    def test_nul_and_invalid_utf8_remain_observable(self):
        expected = b"a\x00\xffb"
        self.assertEqual(runner.compare_bytes(expected, expected)["status"], "pass")
        self.assertEqual(runner.compare_bytes(expected, b"a")["first_difference"], 1)

    def test_payload_cannot_be_a_control_record(self):
        expected = b"Test 1\n# skipped: compiler bug\n"
        self.assertEqual(runner.compare_bytes(expected, expected)["status"], "pass")
        self.assertEqual(runner.compare_bytes(expected, b"")["status"], "mismatch")

    def test_extra_duplicate_and_reordered_bytes_fail(self):
        for actual in (b"one\ntwo\ntwo\n", b"two\none\n", b"one\ntwo\nnoise"):
            with self.subTest(actual=actual):
                self.assertEqual(runner.compare_bytes(b"one\ntwo\n", actual)["status"], "mismatch")

    def test_an_empty_result_is_not_a_missing_case(self):
        self.assertEqual(runner.compare_bytes(b"", b"")["status"], "pass")
        self.assertEqual(runner.compare_bytes(b"", b"\n")["status"], "mismatch")

    def test_reference_is_never_shortened(self):
        result = runner.compare_bytes(b"x" * 300, b"x" * 128)
        self.assertEqual(result["status"], "mismatch")
        self.assertEqual(result["expected_bytes"], 300)
        self.assertEqual(result["symptom"], "truncated")


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


class MemoryEvidenceTests(unittest.TestCase):
    def fixture(self):
        return struct.pack("<II", numeric_memory.MAGIC, 1) + struct.pack("<IQIII", 64, 0, 3, 3, 0) + b"a\0b"

    def test_binary_payload_is_preserved(self):
        value = self.fixture()
        self.assertEqual(numeric_memory.compare_records(value, value)["passed"], 1)

    def test_truncated_missing_and_extra_records_fail(self):
        value = self.fixture()
        for actual in (value[:-1], value[:8], value + value[8:]):
            with self.subTest(actual=actual):
                with self.assertRaises(ValueError):
                    numeric_memory.compare_records(value, actual)

    def test_corrupt_header_cannot_pass(self):
        with self.assertRaises(ValueError):
            numeric_memory.parse_records(b"not a record")

    def test_counts_and_error_status_are_compared(self):
        expected = self.fixture()
        actual = bytearray(expected)
        actual[28] = 1  # formatting error even though the payload still matches
        self.assertEqual(numeric_memory.compare_records(expected, actual)["failed"], 1)


class ReportEvidenceTests(unittest.TestCase):
    def manifest(self, directory, status, case_status, fatal=None):
        path = directory / "manifest.json"
        data = dict(profile="local", label="test", started="test", source={}, oracle={}, host={},
            configurations=[dict(route="host-buffer", optimization=2, status=status,
                cases=[dict(id="case", status=case_status)])])
        if fatal:
            data["fatal_error"] = fatal
        path.write_text(json.dumps(data))
        return path

    def test_auxiliary_fatal_error_cannot_publish_a_legacy_completed_config(self):
        with tempfile.TemporaryDirectory() as temporary, \
                mock.patch.object(evidence_report, "ROOT", Path(temporary)):
            path = self.manifest(Path(temporary), "completed", "pass", "numeric oracle failed")
            result = evidence_report.summarize(path, terminal_only=True)
            self.assertEqual(result["configurations"], [])
            self.assertEqual(result["fatal_error"], "numeric oracle failed")
            self.assertEqual(result["unfinished_configurations"][0]["positive_counts"], {"pass": 1})

    def test_terminal_selection_retains_failed_controls(self):
        with tempfile.TemporaryDirectory() as temporary, \
                mock.patch.object(evidence_report, "ROOT", Path(temporary)):
            path = self.manifest(Path(temporary), "control_compile_failure", "not_run")
            result = evidence_report.summarize(path, terminal_only=True)
            self.assertEqual(result["unfinished_configurations"], [])
            config = result["configurations"][0]
            self.assertEqual(config["status"], "control_compile_failure")
            self.assertEqual(config["positive_executions"], 0)


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


if __name__ == "__main__":
    unittest.main()
