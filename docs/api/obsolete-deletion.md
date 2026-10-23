# Delete the retired formatter implementation

The public header no longer references the old implementation after migration.

```cpp
#include <ffmt/base.hpp> // unchanged for callers
```

Only the unused legacy implementation is deleted; archived snapshots retain it for comparison.

The static formatter protocol and legacy emit-hook adapter remain supported by the new implementation.
