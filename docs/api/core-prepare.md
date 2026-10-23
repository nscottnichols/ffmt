# Preserve the existing implementation behind a wrapper

The replacement engine needs independent qualification while current callers remain usable.

```cpp
#include <ffmt/base.hpp> // still selects the original implementation
```

The legacy header is copied byte for byte from upstream 3fe258a; the public wrapper only selects it.

The temporary extra include is removed once migration is validated.
