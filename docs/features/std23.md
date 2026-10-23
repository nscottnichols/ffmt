# C++23 expected values

```cpp
#include <ffmt/std23.hpp>
ffmt::println("{}", std::expected<int, int>{42}); // expected(42)
```

`std23.hpp` adds `std::expected` and `std::unexpected` adapters when the selected
standard library exposes C++23 expected support. C++20 users can omit the header.
The contained value and error must be formattable. Optional tests report skip
(exit 77) when the standard library lacks the feature; that skip is not evidence
of conformance. Enable the suite with a C++23 compiler mode.

Make keeps C++23 registrations out of the default C++20 capability target.
Use `make test-cxx23` explicitly; `make test-capabilities` and `test-focused`
do not require a compiler to accept C++23 mode.
