# ffmt completion milestones

Working fork: https://github.com/nscottnichols/ffmt. Upstream starts at
`3fe258a87675c997667c899c2b0f0cad405aad5a`. The independent oracle is fmt
`0fc31633e7d53ac895ae0a688b83b7d44f0b0c1f` (immutable throughout this campaign).

## Accepted contract

- Expand toward all device-suitable fmt formatting features, including runtime
  formats, wide formats, ranges/tuples, locale-independent chrono, named fields,
  custom specs, debug formatting, standard data adapters, and explicit styling.
- Keep C++20 and allocation-free device operation. Optional C++23 adapters are
  separate. Host services (filesystem, locale/timezone databases, streams,
  allocating result strings) are outside the device surface.
- Existing kernel printing remains self-contained: no mandatory host setup or
  drain. Emit complete output in bounded chunks; concurrent calls may interleave.
- Runtime errors return status and written/required counts; required size is
  meaningful on success. Errors may leave an emitted prefix. Invalid literal
  formats are compilation errors. Wide buffers match fmt code units; wide stdout
  is UTF-8, with invalid code points reported.
- Compare exact bytes with an independent host fmt object. Never trim the oracle
  to ffmt capacity or interpret an arbitrary compiler failure as conformance.
- Distinguish unsupported functionality, compiler/runtime blockers, infrastructure
  failures, and not-run configurations. A compilation is not a hardware run.

## M1 — independent baseline and infrastructure (implemented)

PR: https://github.com/nscottnichols/ffmt/pull/1. The binary-safe runner,
diagnostic classifier, Cobalt profiles and baseline evidence are committed.
Twenty-five harness regressions pass, including descendant-process timeout cleanup,
interrupted auxiliary suites, failed-build isolation evidence and pinned submission
defaults with explicit revision overrides.
Further routes and evidence are integrated
with their corresponding implementation/qualification changes.

Build a binary-safe differential runner, diagnostic-aware negative tests,
reproducible per-system profiles, software directories, Cobalt job scripts,
CMake/CTest and Make entry points. Capture baseline output and manifests before
implementation. Add focused tests of harness failure detection. Open the first
small PR in the fork.

## M2 — shared formatting engine (complete for this campaign)

PR: https://github.com/nscottnichols/ffmt/pull/2 (stacked on PR1).

Core regressions: 57,365 comparisons; portable shortest conversion, native
extended precision, BitInt, binary128, bounded canaries, wide output, and
legacy/custom formatter protocols have focused tests. Host strict warnings and
ASan/UBSan pass on captured source snapshots. Device evidence is tracked in M4.

Replace truncating output with bounded streaming, unify formatting semantics,
add checked bounded/runtime/wide APIs, remove arbitrary parser limits, fix
numerical and bounds defects, support host/CUDA/HIP/OMP/SYCL emission. Preserve
existing source calls and custom formatter protocol. Add regressions and open a
stacked implementation PR.

## M3 — expanded format families (complete for this campaign)

PR: https://github.com/nscottnichols/ffmt/pull/3 (stacked on PR2).

The family handoff includes 218 differential cases, 1,625 range/bounded
comparisons, 34,719 checked-printf comparisons and six C++23 cases. Mutable
ranges, move-only joins and noncontiguous registered string ranges are
implemented. Pointer padding is single-pass; irreversible right/center streams
explicitly reject nonreplayable bodies.

Extend named arguments, debug formatting, ranges/tuples/join, locale-independent
chrono/calendar values, device-callable standard types, numeric representations,
and explicit ANSI style. Provide fmt-like formatter contexts while keeping legacy
specializations. Test family semantics against pinned fmt; expose genuine device
type restrictions. Keep changes in focused stacked PRs.

## M4 — representative JLSE qualification (complete)

PR: https://github.com/nscottnichols/ffmt/pull/4 (tooling, stacked on PR3).
Portability follow-up: https://github.com/nscottnichols/ffmt/pull/5.
Campaign tooling: https://github.com/nscottnichols/ffmt/pull/6.

The committed `21f9f5e` snapshot passes all 54 stdout cases and 8,216 memory
numeric records on native CUDA/NVCC (A40 and GH200), native HIP, Arm/x86 host,
Kokkos Serial and Kokkos HIP at both optimization levels. Intel SYCL and Kokkos
SYCL pass memory records but retain the stdout NUL transport gap. The
[selected report](results/qualification.md) contains 41 terminal configurations
from nine primary/focused campaigns, including failing configurations. Kokkos,
AdaptiveCpp, chipStar and OpenMP have measured outcomes. Final `98dba67`
chipStar checks pass all eight focused stdout cases and 8,216 numeric records
at both optimization levels. A minimized independent AMD OpenMP callback
failure preserves a compiler/runtime boundary at -O0; the same program passes
on NVIDIA, whose separate full-numeric -O0 failure remains unresolved.
The [gap ledger](results/gaps.md) records transport limits, compilation/JIT
timeouts and sampled scope. Reports retain actual hardware: the `h100` profile
used an A40 fallback. Final integration at `0c2cb76` passes all 15 CTest entries,
Make legacy/fast-math checks and 123 diagnostic-aware negative tests with three
positive controls. Full GCC13 sanitizer suites pass at -O0 and -O2.

Use Rome x86_64 host; H100 CUDA/OpenMP; MI300X native HIP/OpenMP;
Chiatta Intel Max SYCL/SPIR-V/OpenMP/chipStar; GH200 Arm host/CUDA.
Add AdaptiveCpp and Kokkos routes. Use primary alternatives when a node is down.
First inspect with 30-minute one-node jobs, then use two-hour qualification jobs
and longer local builds as needed within queue limits. Record selected devices,
driver/runtime/compiler versions, commands, flags, source state and outcomes.
Re-run affected configurations after fixes. Both -O0 and -O2, strict floating
point, host sanitizers; fast-math is a separate assessment.

## M5 — measurements and working manuscript (complete)

Evidence and manuscript PR: https://github.com/nscottnichols/ffmt/pull/7
(stacked on PR6).

The [compact measurements](results/benchmarks.json) contain 768 measured timing
groups and 112 resource probes from 20 explicitly selected runs. Source proofs
and an exact paper selection ledger retain historical and later snapshots.
First-batch timing is not cold JIT measurement. Intel SYCL records explicit
cache-disabled kernel-bundle preparation and device-event timing. The supplied
outline is preserved in `docs/paper`. The [revised manuscript](paper/ffmt.pdf)
compiles with generated tables from the selected qualification, measurements
and source coverage evidence. The instrumented host union covers 92.11% of
executable lines and 83.43% of branch edges; these are not GPU or fmt feature
coverage percentages. Publication, compact evidence and minimized reproducers
are the final stacked results change.

Measure formatting separately from printing, cold compilation/JIT separately
from warm execution, with five warm-up batches and thirty measured samples.
Collect one/two-call, multi-TU, integer/float and 16/64/128/512-byte staging
resource evidence where tools support it. Import the supplied outline into
docs/paper, update measured claims and support tables, and compile the PDF.
Commit compact run/per-case results and minimized regressions. Keep raw artifacts
and installations ignored but reproducible through recipes and checksums.

## Collaboration and integration

The orchestrator owns branch changes, integration, commits and PRs. Workers edit
only assigned files, communicate API changes before consuming them, and do not
reset or discard another worker's changes. PRs target the working fork, not the
upstream repository. Each worker reports exact validation performed and remaining
work. Engineering gaps remain implementation work, not external blockers.

JLSE uses **Cobalt**, not PBS: `qsub --mode script -q QUEUE -n 1 -t MINUTES --cwd /home/nnichols /bin/bash SCRIPT ARGS`.
Never borrow Aurora/Sunspot recipes for this system. A job must own a compute node
before using it. `qstat -u nnichols`, `qstat -Q`, and `nodelist` are read-only.
Software belongs under ignored `software/<system-id>/{downloads,src,build,install}`.
Hardware runs belong under ignored `artifacts/`; compact evidence is tracked in
docs/results. Avoid uncontrolled compiler parallelism on the login node.
