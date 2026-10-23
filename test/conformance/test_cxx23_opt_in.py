"""Default portable checks stay C++20; C++23 checks require an explicit target."""
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]


@unittest.skipUnless(shutil.which("make"), "GNU Make is unavailable")
class Cxx23OptInTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        self.addCleanup(self.temporary.cleanup)
        self.directory = Path(self.temporary.name)
        registrations = self.directory / "test/conformance/suites"
        registrations.mkdir(parents=True)
        for name, standard in (("baseline", "c++20"), ("future", "c++23")):
            source = self.directory / (name + ".cpp")
            source.write_text("int main() { return 0; }\n")
            (registrations / (name + ".mk")).write_text(
                "FFMT_CAPABILITIES += " + name + "\n"
                "FFMT_SOURCE_" + name + " := " + str(source) + "\n"
                "FFMT_STANDARD_" + name + " := " + standard + "\n"
                "FFMT_FLAGS_" + name + " :=\n"
                "FFMT_REFERENCE_" + name + " := 0\n"
                "FFMT_OPTIONAL_" + name + " := 0\n")
        (self.directory / "Makefile").write_text(
            "SHELL := /bin/bash\nCXX := unused-compiler\nFLAGS_STAMP := stamp\n"
            "stamp build/.dir:\n\ttouch $@\n"
            "include " + str(ROOT / "mk/portable-tests.mk") + "\n")

    def plan(self, target):
        result = subprocess.run(["make", "-n", target], cwd=self.directory,
                                capture_output=True, text=True)
        self.assertEqual(result.returncode, 0, result.stderr)
        return result.stdout

    def test_default_target_does_not_require_cxx23_mode(self):
        output = self.plan("test-capabilities")
        self.assertIn("baseline.cpp", output)
        self.assertIn("-std=c++20", output)
        self.assertNotIn("future.cpp", output)
        self.assertNotIn("-std=c++23", output)

    def test_explicit_cxx23_target_selects_the_declared_standard(self):
        output = self.plan("test-cxx23")
        self.assertIn("future.cpp", output)
        self.assertIn("-std=c++23", output)
        self.assertNotIn("baseline.cpp", output)


if __name__ == "__main__":
    unittest.main()
