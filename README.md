# ffmt

C++20 formatting for host code and accelerator kernels. Formatting uses the
pinned [{fmt}](https://github.com/fmtlib/fmt) implementation as its semantic
reference. Consumers include ffmt headers; fmt is needed only by the tests.

```cpp
#include <ffmt/base.hpp>

FFMT_PRINTLN("value = {0:+08.2f}, again = {0}", 1.25);
```

The same call can appear inside a CUDA/HIP kernel, an OpenMP target region,
or a SYCL kernel. The backend is selected by the compiler. Launch and wait
for kernels using the corresponding runtime; no ffmt host initialization or
output-drain service is required. Independent calls may interleave.

## Output and errors

Literal formats are checked at compilation. Runtime formats report errors:

```cpp
char output[8];
auto result = ffmt::format_to_n(output, sizeof output,
                                ffmt::runtime("{:012x}"), 42);
if (result) {
  // written == 8, required == 12; output contains eight bytes.
}
```

`format_to_n` writes at most the supplied count and does not append a NUL.
A short destination is successful bounded formatting: `written` is the number
stored and `required` is the complete length. `result.out` is the advanced
iterator. `format_to` accepts an output iterator without a bound, and
`formatted_size` returns a result whose `required` field gives the size.

`try_print` and `try_println` return the same status/count contract for stdout.
`print`, `println`, `FFMT_PRINT`, and `FFMT_PRINTLN` discard that status.
`FFMT_PRINTF` and `FFMT_PRINTLNF` are compatibility aliases for brace formatting.
The template calls `ffmt::print<"{}">(value)` and
`ffmt::println<"{}">(value)` are also supported.

On an error, `error` identifies invalid syntax, an invalid argument/specification,
an unsupported type, invalid Unicode, output overflow, or transport failure.
A prefix may already have been written; `required` is meaningful only on success.

Stdout uses bounded staging chunks, with `FFMT_BUFFER_SIZE=128` by default.
Long output is streamed completely; the staging size is not an output limit.
Use the same configuration macros across translation units.
`FFMT_BUFFER_PATH=0` and `=1` remain accepted and select the shared engine.

Wide buffer formatting preserves wide code units. Wide stdout is encoded as
UTF-8; invalid code points or unmatched UTF-16 surrogates return an error.
Narrow memory output preserves embedded NULs. Device stdout transport
capabilities are qualified separately: the tested Intel SPIR-V printf runtime
drops NUL characters, so ffmt reports `transport_error` for such chunks.
`FFMT_SPIRV_NUL_SUPPORTED=1` is an opt-in for a separately validated runtime.

## Formatting and custom types

Core formatting includes positional/repeated fields, dynamic width/precision,
Unicode padding and debug escaping, integer bases, pointer values, decimal
and hexadecimal floating point, signed zero, subnormals, infinities and NaNs.
Native host extended floating types and compiler integer extensions are tested
when available. No arbitrary format-text or placeholder-count limit is imposed.
This is a versioned compatibility effort; the qualification corpus is not a
proof of all fmt behavior.

A custom formatter can reuse an existing formatter's parser and output:

```cpp
struct index { int value; };

template <> struct ffmt::formatter<index> : ffmt::formatter<int> {
  template <typename Context>
  FMT_HD auto format(const index& value, Context& ctx) const {
    return ffmt::formatter<int>::format(value.value, ctx);
  }
};
```

Mark custom operations as device callable when the compiler requires it.
The original static `formatter<T>::format(value)` protocol returning
`ffmt::formatted<...>` remains supported, including the existing SYCL
id/range/item formatters. Its outer format accepts an empty specification.
ADL `format_as` can map a type to another formattable value.

Include the headers for additional families explicitly:

| Header | Surface |
|---|---|
| `ffmt/args.hpp` | Named arguments with `ffmt::arg` |
| `ffmt/ranges.hpp` | Ranges, sets/maps, tuples, joins and container adapters |
| `ffmt/chrono.hpp` | Locale-independent durations, calendar values, `tm`, system/local time points |
| `ffmt/std.hpp` | Optional/variant, bitsets, bytes, reference wrappers, atomics, complex, source location and pointer views |
| `ffmt/std23.hpp` | Optional C++23 expected/unexpected |
| `ffmt/color.hpp` | Explicit ANSI color, RGB and emphasis with `ffmt::styled` |
| `ffmt/enum.hpp` | Enum underlying-value helpers |
| `ffmt/printf.hpp` | Typed, runtime-checked percent formatting |

For example, including `ffmt/ranges.hpp` allows
`FFMT_PRINTLN("{:n}", std::array{1, 2, 3})` to print `1, 2, 3`.
The contained objects and their operations must also be usable on the device.
Left-aligned family bodies render once on every destination. Bounded writable
pointer buffers also handle right/center alignment in one pass, including an
exact truncated prefix. On stdout or other iterators, right/center family
alignment requires replayable built-in values and forward ranges. Unknown custom
formatters and consuming ranges return `unsupported_type` for that combination.
The library does not implement host filesystem, stream, locale/timezone-database,
RTTI, thread-ID or exception-traversal services.

Checked percent formatting uses `ffmt::printf_to_n`, `ffmt::printf_to` and
`ffmt::try_printf`, with the same result contract as brace formatting. Dynamic
width/precision and positional arguments are supported; `%n` is rejected.

## Build and test

The library is header-only. Link the CMake interface target:

```cmake
add_subdirectory(ffmt)
target_link_libraries(my_program PRIVATE ffmt::ffmt)
```

Installed packages also support `find_package(ffmt CONFIG REQUIRED)`.
To prepare and test the independent oracle:

```sh
git clone https://github.com/fmtlib/fmt.git third_party/fmt
git -C third_party/fmt checkout 0fc31633e7d53ac895ae0a688b83b7d44f0b0c1f
cmake -S . -B build/cmake -DFFMT_BUILD_TESTS=ON -DCMAKE_CXX_COMPILER=g++
cmake --build build/cmake -j2
ctest --test-dir build/cmake --output-on-failure
```

Use a C++20-capable compiler, such as GCC 13 or recent LLVM.
NVCC qualification uses `-std=c++20 --extended-lambda --expt-relaxed-constexpr`.
`make test-host test-focused test-negative CXX=g++` runs the historical
compatibility, focused regression, and diagnostic-aware rejection suites.
`make coverage CXX=clang++` reports region, line and branch-edge coverage of
instrumented headers. Uninstantiated templates and device-only bodies are
outside that coverage denominator. Optional C++23 tests use
`-DFFMT_BUILD_CXX23_TESTS=ON` or `make test-cxx23`.

See [the conformance guide](docs/CONFORMANCE.md) for raw-byte comparisons,
the seeded device-memory numeric corpus, and reproducible commands.
[Baseline evidence](docs/results/baseline.md) records the original implementation;
[the milestone ledger](docs/MILESTONES.md) tracks current qualification work.
Compiler success, output agreement, device execution, source coverage, and
performance are reported separately. Fast-math has a separate assessment;
strict runs use `-fno-fast-math`.

## JLSE development

[Profiles](platforms/profiles.json) cover Rome x86_64, H100 CUDA, MI300X HIP,
Intel Max SYCL/SPIR-V, and GH200 Arm/CUDA. Additional routes include OpenMP,
Kokkos, AdaptiveCpp and chipStar. A configured route is not a passing result.

```sh
python3 tools/jlse/submit.py discovery rome h100 mi300x chiatta gh200
python3 tools/jlse/submit.py head mi300x --revision HEAD
```

Jobs use Cobalt allocations. Local installations live under ignored
`software/<system>/{downloads,src,build,install}`; raw results live under
ignored `artifacts/<system>/`. The [project skill](skills/ffmt-jlse-development/SKILL.md)
and [JLSE scripts](tools/jlse/README.md) describe the shared workflow.

ffmt incorporates numerical conversion and Unicode material derived from fmt
and Dragonbox. See [NOTICE](NOTICE) and [LICENSES](LICENSES) for attribution
and the licenses of those components.
