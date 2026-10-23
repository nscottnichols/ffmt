# Named arguments

```cpp
#include <ffmt/args.hpp>
char out[16];
auto result = ffmt::format_to_n(out, sizeof out, "{value:04x}", ffmt::arg("value", 42));
// out contains "002a"; result.written == result.required == 4.
```

`ffmt::arg(name, value)` stores a name view and a value reference. The name
characters and value must outlive use of the wrapper; temporary values remain
valid through the formatting expression. Names and named width/precision fields
are resolved at runtime. Literal parsing checks structural syntax, but cannot
fully validate a named field's type/specification because names are runtime data.
Use checked results to observe unresolved names or incompatible specifications.
