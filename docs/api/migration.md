# Select the checked engine through the public header

The separately qualified engine must replace both old output paths while preserving API spellings.

```cpp
#include <ffmt/base.hpp>
auto result = ffmt::try_print("{:04x}", 42);
```

The public wrapper selects the same engine tested throughout this stack. Existing macros, static formatters and built-in SYCL formatters remain available.

Void compatibility calls discard checked status. Host checks do not replace preserved accelerator campaign evidence.
