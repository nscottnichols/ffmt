# Extract the qualified compiler configuration

The standalone checked engine needs the same compiler and backend configuration as the qualified implementation.

```cpp
#include <ffmt/detail/config.hpp>
```

Compiler macros and standard includes are copied without changes from aeff043. The extraction hash records the exact body.

This header is internal; callers continue including base.hpp. It does not activate the replacement engine.
