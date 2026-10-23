# Backend contracts

ffmt selects device annotations and output transport from the compiler. There is
no required host logger initialization or drain API. Launch and synchronize using
the selected runtime. Headers and compiler flags must agree across translation
units. This document describes API requirements, not a blanket conformance claim
for every compiler/runtime release.

| Route | Toolchain requirements | Output and launch behavior |
|---|---|---|
| Host | C++20 compiler | Counted byte output preserves embedded NUL. |
| CUDA | Clang CUDA or NVCC; NVCC uses `--extended-lambda --expt-relaxed-constexpr` | Literal printf conversion strings emit formatted bytes in bounded chunks. CUDA launch/synchronization errors are checked by the test runner. |
| HIP | HIP-capable compiler and a matching runtime | Same bounded-byte emission contract; test launch/synchronization errors remain observable. |
| SYCL / SPIR-V | GPU-capable SYCL compiler/runtime | Tests select a GPU and propagate asynchronous errors. Some SPIR-V printf runtimes drop NUL. |
| OpenMP target | Compiler, device runtime and target libraries built for the same stack | Tests reject host fallback; applications manage their target runtime. |
| Kokkos | Kokkos built for the requested execution space | Tests verify the actual default execution space and selected device. |
| AdaptiveCpp SSCP | SSCP-capable compiler/runtime | Its supported device print builtin accepts null-terminated strings. |
| chipStar | Matching HIP/SPIR-V compiler, translator and runtime | Generic printf pointer handling follows the chipStar device ABI. |

`try_print`/`try_println` report `transport_error` when a chunk contains NUL on a
transport that cannot deliver it. Buffer formatting remains binary safe. The
SPIR-V default is conservative; `FFMT_SPIRV_NUL_SUPPORTED=1` opts into a separately
validated runtime. It does not change the SSCP builtin's string contract.

The default `FFMT_BUFFER_SIZE=128` controls staging, not total output length.
Separate calls may interleave between chunks. Runtime printf FIFO capacity and
kernel synchronization remain runtime concerns. Wide stdout encodes UTF-8 and
rejects invalid scalar values; wide memory output preserves code units.

Custom formatters and standard-library operations must be device callable for
the chosen implementation. A standard type adapter does not supply device code
for a host-only standard library. Host filesystem, streams, locale/timezone
databases, RTTI and thread services are outside the device API.

Bounded writable pointer destinations render family bodies once. Left-aligned
streaming destinations also render each body once. Right/center alignment on irreversible iterators requires replayable
built-ins and forward ranges; unknown custom formatters and consuming ranges
return `unsupported_type` before invoking the body.

Compiler/runtime investigations, captured device results and independent
reproducers are maintained in
[ffmt-evaluation](https://github.com/nscottnichols/ffmt-evaluation). The portable
[test guide](CONFORMANCE.md) describes how to exercise a local toolchain without
that repository or any scheduler.
