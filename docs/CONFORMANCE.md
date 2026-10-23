# Portable conformance and regression tests

For library usage see [API contracts](API.md), and for installation/CMake
options see [build and test integration](BUILD.md).

The reference is an independent host build of fmt commit
`0fc31633e7d53ac895ae0a688b83b7d44f0b0c1f`. Generated target programs include
ffmt and backend headers, never fmt. The library and its test tools run without
campaign profiles, scheduler commands or the evaluation repository.

## Prepare the reference

```sh
git clone https://github.com/fmtlib/fmt.git third_party/fmt
git -C third_party/fmt checkout 0fc31633e7d53ac895ae0a688b83b7d44f0b0c1f
```

Use a C++20-capable compiler such as GCC 13 or a recent Clang. A normal library
consumer needs neither fmt nor Python. Tests use Python 3 and the pinned fmt
checkout; `--fmt-dir` or CMake's `FFMT_FMT_DIR` can select an existing checkout.

## Build and run

```sh
python3 -m unittest discover -s test/conformance -p 'test_*.py'
cmake -S . -B build/cmake -DFFMT_BUILD_TESTS=ON -DCMAKE_CXX_COMPILER=g++
cmake --build build/cmake -j2
ctest --test-dir build/cmake --output-on-failure
make test-host test-capabilities test-negative CXX=g++
```

CTest and Make explicitly register the decomposed numeric, bounded-output,
custom-type and independent family regressions. Missing declared C++ sources
are configuration/build errors, not silently omitted tests. CMake also runs
the seeded floating suite with `FFMT_PORTABLE_SHORTEST=1`. Optional extension types return an explicit skip
(exit 77). C++23 adapters are enabled with `FFMT_BUILD_CXX23_TESTS=ON` or
`make test-cxx23`. Assertions remain enabled in release test builds.

The differential runner can be used independently:

```sh
python3 tools/conformance.py --route host-buffer --route host-specifiers --opt 0 --opt 2
python3 tools/conformance.py --case long-padding --numeric-samples 32 --opt 2
```

The default is the current working headers; `--revision COMMIT` takes an immutable
snapshot. `--source-root CHECKOUT` selects another ffmt source repository while
the portable runner and fixtures remain identified separately. Output goes to a
fresh ignored `artifacts/` directory, or an explicit `--output` path. Manifests
retain source, fixture and oracle identities, commands, diagnostics and outcomes.

Expected stdout is compared byte for byte, including empty output, missing
newlines and embedded NUL. Expected output is never trimmed to a destination's
capacity. Failed combined builds and executions are preserved before per-case
isolation. A failed positive control does not establish a formatting result.
Invalid-literal tests require the intended diagnostic in both ffmt and fmt;
compiler crashes and unrelated errors cannot count as correct rejections.

The independent memory suite uses 4,096 seeded float and 4,096 seeded double
encodings plus 24 explicit boundaries. Each of the 8,216 records combines five
format specifications and checks bytes, status, written/required counts and
framing. `--numeric-samples` changes the seeded sample count; `--no-memory`
disables this suite for focused work. The original implementation has no bounded
memory API, so the numeric suite is not applicable to that revision.

Use `--help` for optional CUDA, HIP, SYCL, OpenMP, Kokkos, AdaptiveCpp and chipStar
routes. Compiler executables and flags can be supplied using the documented
`FFMT_CXX_<ROUTE>` and `FFMT_FLAGS_<ROUTE>` environment overrides, with hyphens
replaced by underscores. An explicit `--profiles FILE` can supply external route
defaults; no site configuration is shipped or required by the library.

## Benchmarks and coverage

The reusable C++ benchmark primitives live in `test/conformance/benchmark*`.
The portable driver verifies output before timing and separates memory formatting
from printing. For a small host measurement:

```sh
python3 tools/benchmark.py --route host-buffer --family integer --mode memory \
  --capacity 128 --memory-iterations 128
```

Each group uses five warm-up and thirty measured batches. Host wall timing on an
accelerator includes launch and synchronization. First-batch timing is not a
cold-JIT claim. Resource probes distinguish compile-only targets from execution;
compiler frame size, scratch, registers and linked-image sizes are different
measurements. Site orchestration and captured benchmark analysis belong in
[ffmt-evaluation](https://github.com/nscottnichols/ffmt-evaluation).

`make coverage CXX=clang++` reports region, executable-line and branch-edge
coverage of instrumented headers. Uninstantiated templates, compile-time
evaluation and device-only bodies are outside that denominator. Source coverage
is separate from fmt feature coverage and accelerator execution.

Hosted coverage replaces a percentage regression gate with reporting; passing
CI no longer guarantees nondecreasing coverage. See the explicit
[coverage policy](contributing/COVERAGE.md). Independent compiler/runtime
[reproducers are preserved in evaluation](contributing/REPRODUCERS.md).
