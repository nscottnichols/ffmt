# Named arguments with optional families

```cpp
#include <ffmt/args.hpp>
#include <ffmt/ranges.hpp>
std::array<int, 3> values{1, -2, 33};
ffmt::println("{values:*>{width}:03}", ffmt::arg("values", values), ffmt::arg("width", 25));
```

Named argument wrappers preserve the referenced family's mutability and provide
runtime names for outer width fields. The interaction tests combine names with
ranges and standard-type adapters. Each family and named arguments also has an
independent test suite, so this integration adds no prerequisite between those
features.
