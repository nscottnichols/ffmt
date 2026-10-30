# C++23 expected values

```cpp
#include <ffmt/base.hpp>
ffmt::println("{}", std::expected<int, int>{42}); // expected(42)
```

`base.hpp` provides `std::expected` and `std::unexpected` adapters when the selected
standard library exposes C++23 expected support. The declarations are absent in C++20 mode; including the header never raises the C++20 minimum.
The contained value and error must be formattable. Optional tests report skip
(exit 77) when the standard library lacks the feature; that skip is not evidence
of conformance. Enable the suite with a C++23 compiler mode.

Make keeps C++23 tests out of the default C++20 test target.
Use `make test-cxx23` explicitly; `make test-portable` and `test-direct`
do not require a compiler to accept C++23 mode.
