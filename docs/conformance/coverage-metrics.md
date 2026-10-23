# Report source-file region, executable-line and branch coverage

One-header region percentages omit new headers and cannot describe executable-line or branch-edge coverage.

Two headers with a region at the same coordinates remain two regions; an unexecuted branch side stays uncovered.

Include source filenames in union keys, preserve any-instantiation coverage, and read executable lines from LLVM LCOV export rather than estimating spans.

Only instantiated runtime code is measured. Device-only code, compile-time evaluation and uninstantiated templates remain outside the denominator. This PR preserves the blocking policy.

Coverage metrics can accept a file or header directory. This measurement extension does not relax the existing regression gate; policy is reviewed separately.

Validation:

```sh
python3 -m unittest discover -s test/conformance -p test_coverage_metrics.py
make coverage CXX=clang++
```
