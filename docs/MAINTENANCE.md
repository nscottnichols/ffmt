# Maintenance model and infrastructure inventory

Maintain production formatting directly in `include/ffmt/base.hpp`. Its sections
follow dependency order: backend configuration, results and accounting, parsing,
primitive/numeric/Unicode formatting, formatter protocols, execution/transport,
and the extended format families. There is no generator or second canonical
source. All 37 older include paths are three-line compatibility wrappers.
C++20 remains the minimum; C++23 expected adapters retain their feature guards.
The header embeds its applicable notices so copying it alone preserves attribution.

This choice trades a larger standard-library parse surface for one implementation
to inspect, edit and distribute. Families that previously needed another include
are available from `base.hpp`. A standard adapter still cannot make a host-only
standard-library operation callable on a device.

## Test maintenance

The original harness remains the foundation. Its independent pinned fmt object
is compiled for the host, shared case bodies run first against fmt and then ffmt,
and the original backend bodies launch the ffmt work. Exact stdout comparisons
retain all output, including NUL and bytes outside numbered frames. Known compiler
workarounds remain explicit skips. Source snapshots and campaign manifests are not
required to build or run the library tests.

Add text cases to the existing shared test files. The 54 added fixtures are now
40 shared host/device cases and 14 host-only feature cases, rather than Python
program-generation fixtures. The nine added invalid formats map to the existing
negative corpus. This migration does not claim that host-only standard objects
have been executed on GPUs.

Keep direct C++ tests when their result cannot be established through stdout:
buffer bounds, full required size, errors, iterator position, replay counts,
forwarding, numeric representation or seeded boundary sweeps. Those are ordinary
executables built by the same Makefile, not another campaign runner. Assertions
stay enabled in Release builds. All 33 original direct sources and all 34 build
variants remain; retaining them preserves meaningful coverage rather than
replacing it with a few smoke tests.

## Retained components and their concrete purpose

| Component | Why it remains |
| --- | --- |
| `test/capture.hpp` | One byte-preserving stdout capture/diff reporter and visible compiler-workaround gates. It restores stdout on exceptions. |
| `test/test_body*.inc`, `test/test_reference_body.inc` | One host fmt reference body; runtime-specific launch bodies for the original shared cases. CUDA/HIP share one launch body. |
| `test/test_*.cpp`, `test/test_main_*.cpp` | Original primitive/layout/customization cases, their shared backend entry points, buffer cases, and host-only rich features. |
| `test/reference.hpp` | Small host-only direct-test helper for oracle comparison, sizes and truncation/sentinel checks. It does not select backends or collect campaigns. |
| `test/direct/tests.mk` | One explicit source/variant inventory read by both Make and CMake. Only nondefault flags, reference linkage and optional extension outcomes need metadata. Missing sources fail. |
| `test/direct/test_core_accounting.cpp`, `test_core_parsing.cpp` | Counted sinks, overflow, bounded runtime format text and parser/error state. |
| `test/direct/test_core_primitives.cpp`, `test_core_conversion.cpp`, `test_core_floating.cpp` | Full/zero/partial buffer comparisons, binary boundaries, ties/carries and seeded floating sweeps. The floating source also builds with forced portable conversion. |
| `test/direct/test_core_int128.cpp`, `test_engine_bitint.cpp`, `test_engine_extended.cpp`, `test_engine_quad.cpp` | Distinct native representations and extension guards; explicitly unavailable compiler types return 77. |
| `test/direct/test_core_unicode.cpp`, `test_core_migration.cpp` | Wide/code-unit behavior and public API size/error regressions. |
| Direct formatter, format-as, legacy, forwarding and composition tests | Parse/context/static protocols, cv/ref preservation and noncopyable user types need observable state checks beyond printed text. |
| Direct transport, legacy-hook and fast-math tests | Chunk/error hooks and denormal behavior under deliberately different compiler flags. |
| Direct named, family, ranges/iterators/forwarding, chrono, std/std23, enum, style, printf and integration tests | Family errors, single-pass iteration/replay, custom type adaptation and larger differential sweeps that shared text smoke cases do not replace. |
| `test/negative/`, `tools/negative.py`, `tools/compile_diagnostics.py` | Existing compile-rejection corpus, positive/syntax controls and diagnostic classification; crashes or unrelated errors cannot pass. |
| `test/test_infrastructure.py` | Small regressions for pin integrity, diagnostic classification, coverage accounting, missing sources, rebuild dependencies, C++23 opt-in and optional exit handling. |
| `test/test_single_header.cpp` | Compiles/runs against an isolated copy of only `base.hpp`, exercising named/range/chrono/std/style/printf/bounded/error APIs without fmt. |
| `Makefile`, `cmake/ffmtTests.cmake`, portable CI | Existing build/backend/coverage integration with one shared inventory. CI calls the same portable tests instead of another runner. |
| `benchmarks/` | Handwritten C++ memory and printing workloads, optional second translation unit and compile-resource primitive. They define workloads rather than measurement conclusions. |
| `.github/scripts/coverage_union.py` | Existing instrumented source coverage reporting; the reporting policy and its limitations are unchanged. |

## Removed or relocated infrastructure

| Former component | Disposition and reason |
| --- | --- |
| `include/ffmt/detail/*.hpp` and optional implementation headers | Bodies consolidated into directly maintained `base.hpp`; compatibility paths remain wrappers. |
| `test/conformance/suites/*.mk`, `mk/portable-tests.mk`, `mk/portable-coverage.mk` | Removed the parallel capability registry/build loop. One flat inventory and the original Make runner cover shared and direct tests. |
| `test/conformance/core_compare.hpp`, `family_test_support.hpp` | Consolidated shared comparison code into `test/reference.hpp`. Bounded checks retain the entire unwritten suffix sentinel check. |
| `test/conformance/cases*.py`, `case_support.py` | Useful library fixtures migrated to the original harness; exact old fixtures are retained with the evaluation campaign runner for historical reproduction. |
| `tools/conformance.py`, `process_evidence.py`, `oracle.py` | Source snapshotting, campaign selection, isolation and manifests moved to `ffmt-evaluation/tools/campaign_runner`. |
| `tools/backend_*.py`, `backends.py`, `device_arch.py` | Campaign adapters moved to evaluation. Ordinary shared test launches use the existing Make/backend bodies. Native HIP now uses the original CUDA-shaped body. |
| `tools/numeric_memory.py`, generated memory template | Cross-device seeded records and framing are evaluation tools. Library direct numeric/buffer regressions remain. |
| `tools/benchmark.py` | Timing/resource campaign orchestration moved to evaluation; reusable handwritten C++ primitives remain in `benchmarks/`. |
| Python runner regression modules | Retained and exercised in evaluation. Obsolete registry-specific tests remain archival inputs; their meaningful current Make contracts are tested in `test/test_infrastructure.py`. |
| Component/runner PR-stage notes | Byte-preserved in evaluation's `docs/history/library-stack-tip-notes`; active library documentation describes the current API and maintenance model. |

All moves and fixture/test mappings are documented with hashes in the
[follow-up provenance record](https://github.com/nscottnichols/ffmt-evaluation/blob/provenance/single-header-20261030/docs/provenance/single-header-followup.md).
The original stack and its snapshots remain untouched. New validation identifies
the new source and runner separately; historical measurements retain their
original pins, commands, failures and hashes. No source reorganization by itself
relabels an old hardware result.
