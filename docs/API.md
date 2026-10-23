# API and output contracts

Include `<ffmt/base.hpp>` for brace formatting. Literal syntax and positional
fields are checked at compilation; `ffmt::runtime(text)` accepts counted runtime
format text and reports errors through the result. Named fields use runtime
names; see [named arguments](features/named-arguments.md) for their validation
and lifetime rules.

## Buffers and checked results

```cpp
char output[8];
auto result = ffmt::format_to_n(output, sizeof output,
                                ffmt::runtime("{:012x}"), 42);
if (result) {
  // written == 8, required == 12; output is a counted prefix, without a NUL.
}
```

`format_to_n` stores at most the supplied number of code units. A short
destination is successful bounded formatting: `written` counts stored units,
`required` is the complete size, and `out` is the advanced iterator. It never
adds a terminator. `format_to` accepts an output iterator without a bound;
`formatted_size` returns a result whose `required` field gives the size.

`try_print` and `try_println` return checked stdout results. `print`, `println`,
`FFMT_PRINT` and `FFMT_PRINTLN` discard that status. Template calls such as
`ffmt::print<"{}">(value)` remain supported. `FFMT_PRINTF` and `FFMT_PRINTLNF` are
compatibility aliases for brace formatting; typed percent formatting is in the
separate [printf header](features/printf.md).

Errors distinguish invalid syntax, invalid arguments/specifications, unsupported
types, invalid code points, output overflow and transport failure. A prefix may
already have been written; `required` is meaningful only on success. Narrow
memory output preserves embedded NULs. Wide memory output preserves wide code
units; wide stdout encodes UTF-8 and rejects invalid scalar values.

## Custom formatters

A formatter can reuse a builtin parser and output implementation:

```cpp
struct index { int value; };

template <> struct ffmt::formatter<index> : ffmt::formatter<int> {
  template <typename Context>
  FMT_HD auto format(const index& value, Context& ctx) const {
    return ffmt::formatter<int>::format(value.value, ctx);
  }
};
```

The parse/context protocol preserves cv-qualified references and does not copy
formatter values. Mark custom operations device callable where the selected
compiler requires it. `formatted_size` invokes formatter logic; measuring and
formatting separately invokes it twice. The historical static
`formatter<T>::format(value)` protocol returning `ffmt::formatted<...>` remains
supported for empty outer specs, including existing SYCL id/range/item
formatters. ADL `format_as(value)` maps a user type to a primitive representation.

## Optional headers

| Header | Guide |
|---|---|
| `ffmt/args.hpp` | [Referenced named arguments](features/named-arguments.md) |
| `ffmt/ranges.hpp` | [Ranges, tuples, joins and container adapters](features/ranges.md) |
| `ffmt/chrono.hpp` | [Locale-independent durations and calendar values](features/chrono.md) |
| `ffmt/std.hpp` | [Standard wrappers, bitsets, complex and pointer views](features/standard-types.md) |
| `ffmt/std23.hpp` | [Optional C++23 expected/unexpected](features/std23.md) |
| `ffmt/color.hpp` | [Explicit ANSI color and emphasis](features/styling.md) |
| `ffmt/enum.hpp` | [Opt-in underlying enum values](features/enums.md) |
| `ffmt/printf.hpp` | [Typed checked percent formatting](features/printf.md) |

Contained standard-library types and their operations must themselves be device
callable. Host filesystem, stream, locale/timezone-database, RTTI, thread-ID and
exception-traversal services are outside this device API.

Optional families share [nested alignment support](features/family-support.md).
Left-aligned bodies render once on every destination. Bounded writable pointer
buffers also support right/center alignment in one pass, retaining the exact
truncated prefix. Irreversible iterators and stdout require replayable builtin
values and forward ranges for right/center alignment; unknown custom formatters
and consuming ranges return `unsupported_type` before invoking that body.

## Configuration and stdout

Use consistent macros across translation units. `FFMT_BUFFER_SIZE` defaults to
128 and controls staging capacity, not total output length. Long output streams
through multiple chunks. Independent calls may interleave; runtime printf FIFO
capacity and kernel synchronization remain application/runtime concerns.
`FFMT_BUFFER_PATH=0` and `=1` remain accepted and use the same engine.
`FFMT_UNICODE` controls narrow-string Unicode measurement and defaults to 1.

Some SPIR-V printf runtimes drop embedded NULs; AdaptiveCpp SSCP device printing
accepts null-terminated strings. Checked printing reports transport errors for
unsupported NUL chunks, while memory formatting stays binary safe.
`FFMT_SPIRV_NUL_SUPPORTED=1` opts into a separately validated SPIR-V runtime and
does not change the SSCP string contract. See [backend contracts](BACKENDS.md)
for compiler flags and route-specific requirements.
