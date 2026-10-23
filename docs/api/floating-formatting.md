# Format float and double with exact presentation rules

Floating fields need correct rounding even when requested precision exceeds stored output.

```cpp
format_to_n(buffer, 2, "{:.2000f}", 0.125); // required=2002
```

Shortest formatting uses Dragonbox normally and integer interval conversion for fast-math and SPIR-V. Requested trailing zeros stream through counted sinks.

The oracle stays strictly compiled. Host fast-math tests use separately established expected strings and do not qualify device runtimes.
