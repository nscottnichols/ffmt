# Isolate the qualified decimal conversion helpers

The qualified conversion routines were embedded in the public base header.

```cpp
// Existing handwritten bodies move into detail/dragonbox.hpp and detail/exact_decimal.hpp.
```

Bodies are copied unchanged from aeff043. Only namespace and OpenMP-region wrappers are added; extraction hashes are recorded.

The helpers are not activated until conversion integration. Handwritten logic remains explicitly handwritten.
