"""Regressions for false passes previously possible in the output harness."""
import importlib.util
from pathlib import Path
import struct
import sys
import tempfile
from types import SimpleNamespace
import unittest
from unittest import mock

sys.path.insert(0, str(Path(__file__).resolve().parents[2] / "tools"))
import numeric_memory

spec = importlib.util.spec_from_file_location("conformance", str(Path(__file__).resolve().parents[2] / "tools/conformance.py"))
runner = importlib.util.module_from_spec(spec)
spec.loader.exec_module(runner)


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


class BackendEvidenceTests(unittest.TestCase):
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

    def test_shared_binary_failure_is_isolated_before_case_classification(self):
        with tempfile.TemporaryDirectory() as temporary:
            campaign = object.__new__(runner.Campaign)
            campaign.output = Path(temporary)
            campaign.args = SimpleNamespace(run_timeout=30, jit_timeout=600,
                compile_timeout=60, no_memory=True, no_negative=True, shard_size=16)
            campaign.manifest = {"configurations": []}
            campaign.save = mock.Mock()
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


if __name__ == "__main__":
    unittest.main()
