# Portable conformance and regression tests

The original harness is the common foundation: one clean pinned fmt reference
compiled for the host, shared host/device cases, backend bodies, exact stdout
comparison, negative compilation controls, and Make/CI integration. The library
builds and tests without the evaluation repository.

## Reference and commands

The reference is fmt `0fc31633e7d53ac895ae0a688b83b7d44f0b0c1f`. Set
`FMT_DIR=/path/to/fmt` to reuse a clean checkout; it is verified without modification.
Otherwise Make fetches the pin. Consumers do not depend on fmt.

```sh
make -j2 test-portable CXX=g++
make -j2 test-portable CXX=clang++ HOST_OPT=-O0 \
  EXTRA_CXXFLAGS='-fsanitize=address,undefined -fno-sanitize-recover=all'
make test-cxx23 CXX=g++
```

`test-portable` runs the original two host compatibility configurations, direct
API assertions, fast-math probes, negative tests, small infrastructure checks,
and an isolated single-header consumer. `test-host`, `test-direct`,
`test-negative`, `test-fast-math` and `test-single-header` remain useful individually.
CMake/CTest builds the same explicit inventory; see [build integration](BUILD.md).

## Where a case belongs

Text formatting cases go in the original `test/test_*.cpp` shared case bodies.
They run against the host fmt object, then against ffmt on the selected backend,
using the existing capture/diff reporter. The added corpus contributes 40 cases
in those shared suites and 14 richer host-only cases in `test/test_features.cpp`.
Host-only standard objects are not claimed as GPU execution coverage.

Direct tests under `test/direct/` inspect state that stdout cannot establish:
`written`, `required`, advanced iterators, bounded prefixes, untouched sentinels,
error/no-write behavior, conversion boundaries, forwarding and single-pass range
semantics. Existing seeded numeric and family sweeps remain. `test/reference.hpp`
shares their small host-only oracle/comparison helper; it is not a backend runner.

`test/direct/tests.mk` is the one explicit Make/CMake source inventory. Add a test
there with nondefault flags or optional capability markers only when needed.
Assertions remain enabled; optional compiler/library extensions return 77 and
are reported as skips. Default tests require C++20; C++23 remains opt-in.

## Negative and backend checks

`make test-negative` compiles the existing rejection corpus against both ffmt
compatibility macros and fmt. Each variant first compiles a valid control, and
each rejection source has a syntax control. Expected format diagnostics must be
present: an unrelated error, missing header, timeout or compiler crash fails.
Diagnostics stay in `build/negative/`.

Existing backend builds reuse the same cases and fmt host object:

```sh
make -j2 USE_CUDA_CLANG=1 CXX=clang++ CUDA_PATH=/path/to/cuda build/test_main_cuda
make -j2 USE_HIP=1 CXX=hipcc OFFLOAD_ARCH=gfx942 compile-hip
make -j2 USE_OMP_CLANG=1 CXX=clang++ OFFLOAD_ARCH=sm_80 compile-omp
make -j2 CXX=icpx AOT_DEVICE=pvc build
make -j2 USE_OMP_ICPX=1 CXX=icpx AOT_DEVICE=pvc build/test_main_omp
```

Compile/link checks do not establish device execution. Runtime-specific gates
remain visible as skips. Device-callable standard-library operations still depend
on the compiler/runtime. Accelerator execution and independently selected source
snapshots belong in [ffmt-evaluation](https://github.com/nscottnichols/ffmt-evaluation).

## Benchmarks, coverage and campaign evidence

Reusable C++ memory/printing benchmark primitives are in `benchmarks/`.
Timing campaigns, source snapshot management, backend campaign adapters, the
seeded cross-device memory corpus, manifests, and evidence exports live in
ffmt-evaluation. They are not library test or build dependencies. Historical
runners and measurements retain their original source/fixture hashes.

`make coverage CXX=clang++` instruments the same shared/direct tests and reports
regions, executable lines and branch edges. Uninstantiated templates and
device-only code are outside its denominator. The existing
[coverage policy](contributing/COVERAGE.md) remains reporting rather than a blocking
percentage regression gate. See the [infrastructure inventory](MAINTENANCE.md)
for exact retention and relocation decisions.
