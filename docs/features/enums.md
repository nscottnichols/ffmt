# Enum underlying-value helpers

```cpp
#include <ffmt/enum.hpp>
namespace status_example {
using ffmt::enums::format_as;
enum class status : int { warning = -3 };
}
ffmt::println("{:+05}", status_example::status::warning); // -0003
```

`ffmt::underlying(value)` explicitly exposes an enum's underlying integer.
Opting into `ffmt::enums::format_as` in the enum's namespace enables ADL mapping
with the integer formatter's specifications. This prints integer values rather
than discovering symbolic names. The opt-in avoids globally making every enum
formattable.
