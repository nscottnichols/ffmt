# Checked typed percent formatting

```cpp
#include <ffmt/printf.hpp>
char out[32];
auto result = ffmt::printf_to_n(out, sizeof out, "%#08x %.3f", 42u, 1.25);
// out contains "0x00002a 1.250".
```

The percent parser uses typed arguments rather than C varargs and shares the
bounded output/error contract of brace formatting. It supports positional
arguments, dynamic width/precision, integer length modifiers and narrow/wide
output. Invalid conversions report errors; `%n` is rejected and never writes
through a pointer. Floating arguments follow printf's float-to-double promotion.
This parser is intentionally separate from brace parsing so each grammar can be
reviewed and tested independently.
