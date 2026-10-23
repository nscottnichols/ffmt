# Convert binary floating values with bounded integer storage

Device environments cannot consistently provide host conversion services.

```cpp
portable_shortest(buffer, std::numeric_limits<double>::denorm_min()); // "5e-324"
```

Representation bits drive classification and exact interval comparisons. Bounded integer limbs hold decimal expansions without output-capacity limits.

This uses more code than forwarding to libc, but avoids locale and allocation dependencies. Internal conversion accepts supported finite representations and renders their magnitude; callers handle signs and nonfinite spellings. Capacities 1100 and 16520 count decimal digits needed for exact supported IEEE values and interval endpoints, not requested output precision. Binary128 midpoint coefficients require fewer than 11570 digits, leaving conservative headroom. Direct oracle tests cover adjacent powers of two, subnormal/normal transitions, maximum finite values and notation transitions; decimal tests exercise rounding carry and ties.
