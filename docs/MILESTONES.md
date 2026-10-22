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
Fourteen harness regressions pass. Further routes and evidence are integrated
with their corresponding implementation/qualification changes.

Build a binary-safe differential runner, diagnostic-aware negative tests,
reproducible per-system profiles, software directories, Cobalt job scripts,
CMake/CTest and Make entry points. Capture baseline output and manifests before
implementation. Add focused tests of harness failure detection. Open the first
small PR in the fork.

## M2 — shared formatting engine (implemented; qualification in progress)

Core regressions: 57,361 comparisons; portable shortest conversion, native
extended precision, BitInt, binary128, bounded canaries, wide output, and
legacy/custom formatter protocols have focused tests. Host strict warnings and
ASan/UBSan pass on captured source snapshots. Device evidence is tracked in M4.

Replace truncating output with bounded streaming, unify formatting semantics,
add checked bounded/runtime/wide APIs, remove arbitrary parser limits, fix
numerical and bounds defects, support host/CUDA/HIP/OMP/SYCL emission. Preserve
existing source calls and custom formatter protocol. Add regressions and open a
stacked implementation PR.

## M3 — expanded format families (implemented; integration in progress)

The family handoff includes 206 differential cases, 34,719 checked-printf
comparisons and six C++23 cases. Forwarding mutable ranges, move-only joins,
and noncontiguous registered string ranges are active followup work.

Extend named arguments, debug formatting, ranges/tuples/join, locale-independent
chrono/calendar values, device-callable standard types, numeric representations,
and explicit ANSI style. Provide fmt-like formatter contexts while keeping legacy
specializations. Test family semantics against pinned fmt; expose genuine device
type restrictions. Keep changes in focused stacked PRs.

## M4 — representative JLSE qualification (in progress)

Native CUDA and HIP have passed the 40-case core stdout corpus and 8,216
device-memory numeric records at both optimization levels on captured snapshots.
The expanded 52-case corpus and final committed snapshots still require runs.
Intel cold JIT, transport NUL limitations, OpenMP numeric execution failures,
and AdaptiveCpp/chipStar dependency builds remain under investigation.

Use Rome x86_64 host; H100 CUDA/OpenMP; MI300X native HIP/OpenMP;
Chiatta Intel Max SYCL/SPIR-V/OpenMP/chipStar; GH200 Arm host/CUDA.
Add AdaptiveCpp and Kokkos routes. Use primary alternatives when a node is down.
First inspect with 30-minute one-node jobs, then use two-hour qualification jobs
and longer local builds as needed within queue limits. Record selected devices,
driver/runtime/compiler versions, commands, flags, source state and outcomes.
Re-run affected configurations after fixes. Both -O0 and -O2, strict floating
point, host sanitizers; fast-math is a separate assessment.

## M5 — measurements and working manuscript (in progress)

Host/Arm/HIP timing and resource manifests exist under artifacts; they need a
final source pass and compact publication. First-batch timing is not cold JIT
measurement. An explicit cache-disabled SYCL kernel-bundle build measurement is
being added. The supplied manuscript still needs import and revision.

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
