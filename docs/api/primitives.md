# Format primitive values into bounded memory

The old print-only interface cannot safely target caller-owned memory or report the full required size.

```cpp
format_to_n(buffer, 2, "{:04x}", 42); // stores "00", required=4
```

Builtin formatters share one parser and counted sink. Signs, base prefixes, padding and digits have separate output responsibilities.

Floating, Unicode display rules and customization protocols are introduced separately.
