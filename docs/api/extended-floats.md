# Preserve native long-double and binary128 representations

Passing extended values through double loses range and precision before formatting.

```cpp
format_to(buffer, "{:.21g}", 1.234567890123456789L);
```

Native 80-bit and 128-bit decomposition feeds the same exact decimal algorithm. Formatting does not require quadmath conversion.

Layouts and availability are target dependent. Unsupported representations return a checked status.
