# JLSE allocations and dependency prefixes

Use `python3 tools/jlse/submit.py discovery rome h100 mi300x chiatta gh200` to
inspect one allocated node per hardware category. `--dry-run` prints the exact
command without creating files or submitting work. Each allocation uses Cobalt
`--mode script`; its launch directory and scheduler output live on `/home`
because the Cobalt service host cannot open `/vast` paths. The allocated node
then changes into the repository, which is mounted there. Submission records
retain both locations in `artifacts/submissions/`.

`job.sh` records selected compiler paths, versions, hardware, driver information,
and selected environment fields in `artifacts/<profile>/discovery-<jobid>.json`.
It uses `environment.sh` to select native toolchains. GH200 has an architecture
specific `/soft` mount and requires a native Python and GCC; an inherited
`~/.local/bin/python3` can be an incompatible x86 executable.

Examples:

```bash
python3 tools/jlse/submit.py baseline h100 --minutes 120
python3 tools/jlse/submit.py head mi300x --revision WORKTREE --extra --route hip --opt 2
python3 tools/jlse/submit.py provision rome --extra --component kokkos-serial --jobs 12
python3 tools/jlse/submit.py provision chiatta --extra --component intel-llvm --component kokkos-sycl
python3 tools/jlse/submit.py host-tests gh200 --revision WORKTREE
python3 tools/jlse/submit.py benchmark rome --revision WORKTREE --extra --multi-tu
```

Source dependencies are pinned by commit in `platforms/dependencies.json`;
binary archives are pinned by SHA-256 from upstream release metadata. Downloads,
source trees, build trees and install prefixes belong in ignored
`software/<profile>/{downloads,src,build,install}` directories. `provision.py`
requires an allocation for builds and binary installation; `--fetch-only`
allows source downloads before an allocation. Its per-component manifest never
turns a failed configuration/build into a successful installation.

AdaptiveCpp 25.10 requires an official LLVM at most version 20 and a shared
`libLLVM`. Both the shared JLSE LLVM20 installation and the upstream binary
archive lack that library; the recipe therefore builds pinned LLVM20 from source
with its shared library enabled. Intel LLVM's SYCL compiler and Intel oneAPI's OpenMP compiler are selected
separately. No device route permits CPU fallback.

The optional chipStar recipe is retained as an attempted configuration: LLVM 22
failed while linking its OCML bitcode and LLVM 23 exposed upstream API mismatches.
An LLVM 20 configuration uses the pinned SPIR-V translator rather than LLVM's
integrated SPIR-V backend. Installation is successful only when its per-job
manifest records `installed`; source downloads alone establish no support.

`host_tests.py` covers the engine, legacy emission hook, native extended float,
optional `__float128` and BitInt extensions, portable shortest conversion,
format families, printf formatting and the C++23 adapters at both optimization
levels. Optional compiler/library features returning 77 are recorded as
`unsupported`. The native Arm long-double suite exercises IEEE128 independently
of the x86 `__float128` extension.

`diagnose.py --omp-memory-probes` isolates mapped writes, integer/float conversion
and teams constructs with independent fmt output. `--omp-large-probes` repeats
the isolated paths at the full 8216-record allocation. `--omp-stack-probes`
retries an existing immutable executable with stack/teams settings and compares
its bytes with `--expected` oracle data. These produce measured failures when an
offload runtime faults; an executable that falls back to the host cannot pass.

The benchmark runner keeps formatting into memory separate from stdout emission,
compares exact output with an independently compiled fmt executable, and records
the first batch, five warm-ups and thirty measured batches. Intel SYCL additionally
builds an executable kernel bundle before timing batches and records that
preparation separately, including any backend cache lookup. SYCL persistent cache
is disabled; the first batch is not assumed to be a cold driver JIT. Accelerator times
include launch and synchronization; device-to-host copies and checksum work are
outside the interval. These are microbenchmarks of a single work-item, not
parallel throughput measurements. Native printf is compared for emission; fmt
is compared for preallocated host output. Resource reports and raw stdout remain
under ignored artifacts, with compact results imported into documentation.
Manifests retain source and binary hashes, compiler versions, device-image
metadata, and a hash of the job's hardware/runtime discovery evidence.
