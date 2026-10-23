# Explicit ANSI styling

```cpp
#include <ffmt/color.hpp>
ffmt::println("{}", ffmt::styled(42, ffmt::fg(ffmt::rgb(255,20,30)) | ffmt::emphasis::bold));
```

Styles explicitly emit ANSI escape bytes around the formatted value and reset
when a nonempty style ends. There is no terminal detection; captured output also
contains those bytes. RGB, terminal palette colors and emphasis flags compose
with `operator|`. Incompatible repeated terminal colors report `invalid_format`.
The wrapped formatter still owns ordinary value parsing and width semantics.
