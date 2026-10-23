# Require intended diagnostics in negative compilation tests

Treating every failed compilation as a rejected format lets missing headers and compiler crashes pass.

A consteval invalid-format diagnostic passes; "fmt/format.h: No such file or directory" is an infrastructure failure.

Compile a positive control and syntax-only fixture before testing both ffmt paths and fmt. Classify diagnostics independently of the differential runner.

Compiler diagnostic recognition is a conservative heuristic, not a compiler-independent proof; unfamiliar wording fails visibly.

Negative compilation requires the intended format diagnostic. Infrastructure failures, unrelated errors and unexpected acceptance fail the check.

Validation:

```sh
python3 -m unittest discover -s test/conformance -p test_negative_driver.py
make test-negative CXX=g++
```
