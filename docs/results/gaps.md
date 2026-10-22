# Observed gaps and qualification limits

Use the [per-configuration report](qualification.md) for exact counts and the
[JSON](qualification.json) for source, compiler, device and failure evidence.
A backend name alone does not imply complete support.

| Area | Observed behavior | Practical scope |
|---|---|---|
| Intel SPIR-V stdout | The measured printf transport drops embedded NUL. ffmt detects it and returns `transport_error`; the exact stdout test remains a mismatch. | Memory formatting preserves bytes. This affects the sampled SYCL/OpenMP/Kokkos-SYCL transport. |
| AdaptiveCpp SSCP stdout | Its supported print builtin accepts null-terminated text. A raw builtin probe truncates at NUL; ffmt rejects the chunk with `transport_error`. Direct variadic printf failed even for ASCII in this stack. | Native HIP printf is a separate, binary-safe tested route. SSCP memory formatting preserves NUL. |
| AdaptiveCpp calendar probe | The full AMD `-O0` campaign timed out after 600 seconds on `chrono-calendar`; the other 52 non-NUL stdout cases and all 8,216 numeric records passed. Focused NVIDIA `-O0`/`-O2` runs also time out at 600 seconds, and focused AMD runs at 180 seconds. | These are execution/JIT timeout observations, without a proven formatter or compiler root cause. All selected AdaptiveCpp numeric runs pass 8,216 records. |
| AMD OpenMP at `-O0` | The sampled numeric path faults. A standalone 97-line callback reproducer with no fmt/ffmt dependency also faults at a null GPU address; identical source passes at `-O2`. | See [reproducer and matched toolchain](../../test/regressions/README.md). Host ASan/UBSan control passes. No library workaround is claimed. |
| NVIDIA OpenMP at `-O0` | The focused stdout probes pass, but the numeric memory run reports an illegal GPU access. The corresponding optimized numeric run passes. | The independent AMD callback source passes on NVIDIA at both optimization levels; the NVIDIA numeric fault remains separately unresolved. |
| Focused OpenMP at `-O2` | Selected chrono/padding builds exceed the recorded 180-second compiler cap. Intel's bounded-padding binary also fails module creation with an unresolved `old_llvm.umul.with.overflow` intrinsic, and its left-padding run exceeds the 300-second execution/JIT cap. | These remain compilation, module-load or execution/JIT failures, without output-conformance claims. The corresponding primary native routes exercise the same logical cases. |
| Right/center family padding on irreversible sinks | Unknown custom formatters and consuming ranges return `unsupported_type` before rendering the body. | Pointer buffers and left-aligned streaming render once. This avoids doubled mutations while retaining allocation-free, unbounded streaming. |
| Standard type adapters | Availability depends on whether the selected standard-library operations are device callable. Optional extension/C++23 tests retain explicit skips. | An adapter does not make all host standard-library implementations device callable. Host services remain outside the accepted device surface. |

The chipStar shortest-format mismatch found during qualification was repaired
by initializing the complete returned candidate storage. Its before/after
numeric evidence and focused host validation are separate from the earlier
54-case primary snapshot; see [portability evidence](portability-regressions.json).

The corpus samples format strings, types, Unicode and floating encodings. Stdout
probes use one active work item and finite output volumes; concurrent interleaving
and printf FIFO saturation were not qualified. Source coverage excludes
uninstantiated templates, compile-time evaluation and device-only code. Compile-only
resource/CI probes do not establish execution on their target GPU architecture.

Timings retain captured source hashes. Historical first-batch timing is not cold
JIT. Intel kernel-bundle preparation disables the persistent SYCL cache, but driver
cache state is not established cold. No separate no-op launch baseline,
`std::format` or `FMT_COMPILE` benchmark was collected. These measurements do not
establish performance superiority or cross-vendor occupancy equivalence.
