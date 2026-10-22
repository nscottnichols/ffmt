# Evidence guide

The [qualification report](qualification.md) separates exact stdout agreement
from device-memory numeric agreement and preserves failures and unsupported
cases. Its [JSON](qualification.json) identifies the selected runs, immutable
header snapshots, oracle, fixture hashes, commands, actual devices and raw-log
hashes. A profile selects a queue; the `h100` profile used an **NVIDIA A40**
fallback in this campaign. It is not evidence of H100 execution.

[Observed gaps](gaps.md) explains transport limits, the isolated OpenMP failure,
streaming restrictions and unmeasured areas alongside the per-case outcomes.

The oracle is fmt `0fc31633e7d53ac895ae0a688b83b7d44f0b0c1f`. Generated target
programs do not include fmt. The final stdout corpus has 54 logical cases;
each configuration has its own execution counts. The numeric suite has 8,216
input records, each combining five reference formats. A failed stdout control
prevents a stdout claim but does not suppress an independent memory result.
Shared-shard failures remain recorded even when isolated cases pass.

The [baseline report](baseline.md) and [baseline JSON](baseline.json) retain the
original implementation at `3fe258a87675c997667c899c2b0f0cad405aad5a` on five
systems. That earlier corpus has 40 logical stdout cases. Do not subtract its
counts from the expanded corpus or count control failures as exercised cases.

[Host final results](host-final.json) capture strict native x86 and Arm suites
on the final qualification snapshot; unsupported compiler extension types are
explicit. [Host regressions](host-regressions.json) retain the larger differential
and sanitized development suites with their original source hashes. They are
complementary evidence, not interchangeable source snapshots.

[Portability regressions](portability-regressions.json) record subsequent GCC
sanitizer constant evaluation, libc++ BitInt classification and initialized
shortest-candidate storage checks. The frozen host headers match `98dba67`;
strict ASan/UBSan passes 57,365 core and 1,276 native-extended comparisons.
These focused checks do not relabel the earlier 54-case campaign's source.
The subsequent counted-view fix at `0c2cb76` passes the full GCC13 sanitizer
Make suites at both optimization levels, with a separate proof of every header
and the new transport regression. Its invalid-format subprocesses use sanitizer
flags at compiler-default optimization, recorded separately from the positive
`-O0`/`-O2` builds.

[Supplemental qualification](supplemental-qualification.json) preserves Kokkos
actual-device guards, intentional wrong-space rejection, independent AdaptiveCpp
transport controls and final HIP/native-compiler smokes. Compile-only CUDA/NVCC
records remain distinct from accelerator execution.

[Build integration](build-integration.json) records 15 passing CTest entries,
Make legacy/fast-math tests and diagnostic-aware negative controls from an
isolated committed tree. [Source coverage](coverage.json) comes from that job.
It unions instrumented positions across
header/template instantiations at `0c2cb76`. It covers 3,445 of 3,740 executable lines
(92.11%) and 2,119 of 2,540 branch edges (83.43%). Uninstantiated templates,
compile-time evaluation and device-only code are outside that denominator.
These percentages do not measure fmt feature coverage or GPU execution.

[Benchmarks](benchmarks.json) retain all explicitly selected timing groups,
builds and resource probes, including historical source snapshots. Timing
requires exact verification, five warm-up batches and thirty measured batches.
Accelerator host-wall timing includes launch and synchronization; it is not
individual call latency. First-batch timing is not cold JIT. Intel SYCL records
separate device-event timing and cache-disabled kernel-bundle preparation;
driver cache state remains a limitation.

The [paper selection ledger](paper-selection.json) selects the latest measured
one-call, single-translation-unit, 128-byte group per system/route/engine/data/sink/presentation
for the manuscript. Each displayed row has a header fingerprint and input
manifest. [Source proof](benchmark-source-proof.json) identifies measured groups
whose captured headers exactly match the final qualification snapshot. Other
measurements keep their original hashes. Resource probes identify compile-only
scope and target architecture, independently of the compilation node's hardware.
The [postfix proof](benchmark-postfix-proof.json) identifies the later `98dba67`
shortest-memory measurements on Intel SYCL/chipStar. Presentation labels keep
these distinct from `.8e` timing. Historical `source_commit` fields recorded the
runner checkout; their captured library `revision` and file hashes are authoritative.
Compiler frame size, GPU scratch, registers and linked table copies describe
different resources and are not cross-vendor occupancy estimates.

The [working manuscript](../paper/ffmt.pdf) discusses the design, method and
limitations; its source and historical supplied outline are in [paper/](../paper/).
Raw installations, binaries, stdout and diagnostics stay under ignored
`software/<profile>/` and `artifacts/`. Recipes and submission helpers live in
[tools/jlse/](../../tools/jlse/README.md). Run `tools/report.py --help`,
`tools/benchmark_report.py --help`, and `tools/paper_tables.py --help` to reproduce
the compact exports; final exports select exact manifests rather than silently
choosing successful retries.
