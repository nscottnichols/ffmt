# Reproduce conformance evidence

The oracle is a separate host build of fmt commit
`0fc31633e7d53ac895ae0a688b83b7d44f0b0c1f`. Generated device sources do not
include fmt headers. The corpus lives in `test/conformance/cases.py`; each ID
denotes the same logical case across compiler/backend configurations.

Prepare the reference once, using an empty destination:

```sh
git clone https://github.com/fmtlib/fmt.git third_party/fmt
git -C third_party/fmt checkout 0fc31633e7d53ac895ae0a688b83b7d44f0b0c1f
```

Use a C++20-capable compiler (GCC 13 or later, or recent Clang). On the JLSE
x86_64 login host, `module use /soft/modulefiles; module load gcc/13.3.0` selects
the tested GCC installation. Do substantial compilation and hardware tests in
allocations. The oracle compiler can be selected independently with
`--host-cxx` or `FFMT_HOST_CXX`.

```sh
python3 -m unittest discover -s test/conformance -p test_runner.py
python3 tools/conformance.py --profile local --route host-buffer --opt 2
python3 tools/conformance.py --profile rome --revision 3fe258a87675c997667c899c2b0f0cad405aad5a --label baseline
```

Every run gets a new directory under `artifacts/<profile>/`. The runner exports
an immutable revision or copies the working include tree before compiling. Its
manifest records source hashes, the oracle pin, compile/execution commands,
diagnostic paths, actual selected device, and per-case results. Failed shards
are split to identify independently failing cases. Execution-failing shared
binaries are also isolated, preserving the original shard failure in the
manifest. No test payload is parsed as
an output-control marker. Empty output, embedded NULs, and missing newlines
remain observable.

The new engine also runs the device-memory numeric suite automatically: 4,096
seeded bit patterns each for float and double plus explicit boundary encodings,
with five format specifications per input. This tests data loaded from device
memory separately from stdout. `--numeric-samples` changes the sample count;
`--no-memory` disables it for focused debugging. The immutable baseline lacks
the bounded-memory API, so this suite is not applicable to that revision.

Literal-invalid tests must reject in both ffmt and fmt. A positive control must
compile and execute before backend cases run. Compiler crashes, missing tools,
missing targets, and unrelated diagnostics cannot satisfy rejection tests.
`make test-negative CXX=...` additionally runs the original negative corpus with
positive and per-file syntax controls, preserving full diagnostics.

## JLSE jobs and local software

Read `platforms/profiles.json` for queue/routes and `platforms/dependencies.json`
for pinned optional toolchains. The submission helper uses Cobalt script mode
and launches from `/home/nnichols`, because the scheduler service cannot open
the `/vast` working directory before starting a node. The job then enters the
checkout on the allocated node.

```sh
python3 tools/jlse/submit.py discovery rome h100 mi300x chiatta gh200 --dry-run
python3 tools/jlse/submit.py baseline rome h100 mi300x chiatta gh200
python3 tools/jlse/submit.py head rome --revision WORKTREE
```

Raw scheduler I/O is retained under `/home/nnichols/.cache/ffmt-cobalt`; job
evidence is under `artifacts/`. Local software belongs in
`software/<profile>/{downloads,src,build,install}`. Both directories are ignored;
recipes and environment selection remain tracked. GPU routes select GPUs
explicitly, check launch/synchronization errors, and prohibit OpenMP host
fallback. Arm jobs use native tools rather than inherited x86 executables.

## Reports and CMake

```sh
python3 tools/report.py --label baseline --output docs/results/baseline
cmake -S . -B build/cmake -DFFMT_BUILD_TESTS=ON -DCMAKE_CXX_COMPILER=g++
cmake --build build/cmake -j2
ctest --test-dir build/cmake --output-on-failure
```

Reports include only completed runs unless requested otherwise. Counts distinguish
output mismatch, unsupported features, compile/execution failures and controls
that prevented execution. A compile-only result is never hardware qualification.
The JSON retains per-case evidence and raw-artifact checksums. Semantic/backend
coverage, source-code coverage and performance have separate denominators.

The CMake interface target is `ffmt::ffmt`; normal consumers need no Python or
fmt dependency. CTest invokes the same runner and corpus as the Make front end.
It also runs the focused numeric, bounded-output, transport, and custom-type
regressions. Unsupported optional compiler extensions use CTest's explicit
skipped status (exit 77). Assertions remain enabled in release test builds.
