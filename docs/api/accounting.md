# Count bounded output independently of formatting

Truncation must retain the complete required length without writing beyond caller capacity.

```cpp
sink.repeat('x', 7); // capacity 2: written=2, required=7
```

All iterator copies share a sink. Required-length overflow is detected before arithmetic wraps.

Counts describe code units, not display columns; family alignment hooks arrive with shared family support.
