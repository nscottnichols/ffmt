# Ranges, tuples and joins

```cpp
#include <ffmt/ranges.hpp>
std::array<int, 3> values{1, -2, 33};
ffmt::println("{::03}", values);  // [001, -02, 033]
ffmt::println("{:02x}", ffmt::join(values, ","));
```

Ranges select sequence, set, map, string or debug-string presentation. Tuples
and container adaptors have explicit adapters; nested specifications follow the
second colon, while `n` removes delimiters. `range_format_kind` customizes string
versus debug-string treatment. Joins hold iterator/delimiter views; keep their
underlying objects alive. Mutable and move-only input iterators are preserved.
Right/center alignment on streaming iterators requires repeatable element
formatters and forward ranges; bounded writable buffers handle consuming ranges
without replay. The standard-library operations used by the range must be
callable on the selected device.
