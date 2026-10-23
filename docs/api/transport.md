# Deliver counted bytes through host and accelerator stdout

Long output and percent signs must reach stdout without reinterpretation as format syntax.

```cpp
try_print("a{}b", char(0)); // counted chunk hooks preserve the NUL
```

A bounded sink emits counted chunks. Backend printf uses fixed conversion formats; compatibility hooks receive safe escaped pieces.

Some backend or legacy transports cannot preserve embedded NULs and report errors. Host captures validate contracts, not accelerator runtime behavior.
