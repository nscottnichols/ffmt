# Shared formatting-family support

```cpp
// Internal support, used by optional formatter headers:
return ffmt::detail::family_padded(ctx, spec, [&](auto& body) {
  ffmt::detail::family_ascii(body, "body");
}, /* replayable = */ false);
```

The internal helpers share outer alignment parsing, display-column accounting,
debug defaults and nested formatting. Left-aligned output renders once on any
iterator. Bounded writable pointer buffers also support right/center alignment
in one pass by inserting padding while retaining the exact truncated prefix.
Other iterators require replayable values for right/center alignment; unknown
custom formatters and consuming ranges return `unsupported_type` before invoking
the formatter. These helpers are implementation details, not a public extension
API. Optional formatters should document which input operations are device callable.
