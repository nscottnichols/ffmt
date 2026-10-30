# ffmt

C++20 formatting for host code and accelerator kernels, with
[{fmt}](https://github.com/fmtlib/fmt) as the semantic reference.

```cpp
#include <ffmt/base.hpp>

ffmt::println("value = {0:+08.2f}, again = {0}", 1.25);

char output[8];
auto result = ffmt::format_to_n(output, sizeof output, "{:012x}", 42);
// Eight stored bytes, twelve required bytes; no NUL is appended.
```

The complete implementation is maintained directly in this one header. Copy
`include/ffmt/base.hpp` alone, or use the CMake interface target. Existing family
and internal include paths remain thin compatibility wrappers. Ordinary consumers
need neither fmt nor Python; those are independent test dependencies. Device use also requires the formatted objects and their
operations to be callable by the selected compiler.

| Start here | Contents |
|---|---|
| [API and output contracts](docs/API.md) | Checked results, buffers, custom formatters, format families and configuration |
| [Build and install](docs/BUILD.md) | Source and installed CMake targets; opt-in portable tests |
| [Backend contracts](docs/BACKENDS.md) | CUDA, HIP, SYCL, SPIR-V, OpenMP and other supported compiler routes |
| [Maintenance and infrastructure inventory](docs/MAINTENANCE.md) | One implementation header, the original shared-case harness, and the purpose of each retained test component |
| [Conformance guide](docs/CONFORMANCE.md) | Independent oracle, focused regressions, exact-byte and memory comparisons |
| [Contributor CI policy](docs/contributing/CI.md) | Ready review waves and the meaning of skipped hosted jobs |
| [Coverage policy](docs/contributing/COVERAGE.md) | Reporting and the limits of the instrumentation denominator |

System profiles, provisioning, scheduling, captured campaigns, independent
compiler reproducers and the manuscript belong in
[ffmt-evaluation](https://github.com/nscottnichols/ffmt-evaluation). They are not
needed to build or test the library. Preserved source snapshots and their
revision mapping remain documented there.

Numerical conversion and Unicode material incorporates work from fmt and
Dragonbox. See [NOTICE](NOTICE) and [component licenses](LICENSES).
