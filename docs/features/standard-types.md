# Standard-library type adapters

```cpp
#include <ffmt/std.hpp>
ffmt::println("{}", std::optional<int>{42});  // optional(42)
ffmt::println("{:.2f}", std::complex<double>{1, -2.2}); // (1.00-2.20i)
```

Optional and variant values use debug presentation for contained text; bitsets,
bytes, reference wrappers, atomics, complex numbers, source locations and pointer
views have explicit adapters. `ffmt::ptr` makes a raw/smart pointer printable
without dereferencing it. Atomic formatting loads the current value; a format
call is not an atomic snapshot across multiple arguments. These adapters do not
supply filesystem, streams, RTTI, thread-ID or exception-traversal services.
Standard-library types and their operations must themselves be device callable.
