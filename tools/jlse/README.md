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
Arm defaults to `/usr/bin/g++-14`; explicit `CXX`, `FFMT_HOST_CXX` and
`FFMT_CXX_HOST_BUFFER`/`FFMT_CXX_HOST_SPECIFIERS` selections are retained.
The oracle and host routes default to `CXX` when they have no separate override.

Examples:

```bash
python3 tools/jlse/submit.py baseline h100 --minutes 120
python3 tools/jlse/submit.py head mi300x --revision WORKTREE --extra --route hip --opt 2
python3 tools/jlse/submit.py provision rome --extra --component kokkos-serial --jobs 12
python3 tools/jlse/submit.py provision chiatta --extra --component intel-llvm --component kokkos-sycl
python3 tools/jlse/submit.py host-tests gh200 --revision WORKTREE
python3 tools/jlse/submit.py benchmark rome --revision WORKTREE --extra --multi-tu
```

The recorded NVIDIA fallback used an A40 (`sm_86`) while retaining the `h100`
artifact profile. Select both its queue and architecture explicitly. These
examples reproduce the native source snapshot and the focused Kokkos device-guard
smoke, respectively, using the installed A40-specific Kokkos prefix:

```sh
python3 tools/jlse/submit.py head h100 --queue gpu_a40 \
  --revision 21f9f5ef4b5fbbc173dcd2fc274f94d30d26f8c8 \
  --env FFMT_GPU_ARCH=sm_86 --extra --route cuda --route nvcc --opt 0 --opt 2
python3 tools/jlse/submit.py head h100 --queue gpu_a40 \
  --revision 21f9f5ef4b5fbbc173dcd2fc274f94d30d26f8c8 \
  --env FFMT_GPU_ARCH=sm_86 \
  --env FFMT_KOKKOS_CUDA_PREFIX="$PWD/software/h100/install/kokkos-cuda-sm86" \
  --extra --route kokkos-cuda --opt 2 --case bounded-family-padding --numeric-samples 1
```

Run these from the repository root after provisioning the required prefixes.
The default `h100` queue and its Hopper Kokkos installation target different
hardware. The Kokkos command above checks one stdout case and 26 numeric input
records; the full focused campaign's selected cases remain in its manifest.

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

chipStar 1.3 installs with pinned LLVM 20 and SPIR-V translator 20.1.18. The
recipe also freezes its transitive SPIRV-Tools source and fingerprints the
local CMake pin. `--llvm-prefix` can reuse a compatible installed LLVM 20.
Trials with LLVM 22 failed while linking OCML bitcode; LLVM 23 exposed upstream
API mismatches. Installation is successful only when its per-job manifest
records `installed`; source downloads alone establish no support.

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
Resource probes label explicit `.8e` and default shortest `{}` presentations
separately. Select `--resources-only --resource-presentation shortest` to inspect
the shortest-conversion path; ELF symbol sizes identify retained numeric tables
and their copies in each image.
Use `--family float --mode memory --capacity 128` for a focused follow-up
measurement after a numeric implementation change; the manifest records this
selection. Omitting the filters retains the complete matrix.
Add `--float-presentation shortest` with memory mode to time default `{}`
conversion against fmt. These rows carry a distinct presentation label and
have no printf comparison.

The chipStar recipe also records a versioned translator adapter selected through
an explicit `-B` compiler flag. It excludes `SPV_KHR_untyped_pointers` from
Clang20 HIPSPV's unrestricted extension list because the measured Intel IGC
rejects that extension. The original pinned translator remains unchanged.
Chiatta selects the Level Zero GPU backend explicitly. The qualified typed
translator does not require skipping chipStar backend cleanup.

For an isolated integration check of a committed tree, submit
`tools/jlse/build_checks.sh PROFILE COMMIT --cxx COMPILER --coverage` through
Cobalt script mode. For example, from a scheduler-accessible working directory:

```sh
qsub --mode script -q rome_7742 -n 1 -t 60 --cwd /home/nnichols \
  /bin/bash /vast/projects/datascience/nnichols/ffmt_project/ffmt/tools/jlse/build_checks.sh \
  rome 98dba67 --cxx /soft/compilers/llvm/release-22.1.8/bin/clang++ --coverage
```

The helper creates a detached checkout, runs CMake/CTest and Make controls, and
retains step commands, raw diagnostics and source coverage under a unique job
directory. Omit `--coverage` when using GCC.
