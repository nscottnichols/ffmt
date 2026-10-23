# Locale-independent chrono formatting

```cpp
#include <ffmt/chrono.hpp>
ffmt::println("{:%T}", std::chrono::milliseconds{-123456}); // -00:02:03.456
ffmt::println("{:%G-W%V-%u}", std::chrono::year{2021}/1/1); // 2020-W53-5
```

The header supports durations, C++20 calendar values, `std::tm`, system time
points and local time points using civil-date arithmetic and explicit tokens.
Fractional precision, negative timestamps and ISO week years follow the pinned
fmt reference. This implementation provides locale-independent formatting; it
does not query clocks, a timezone database or locale services. Local time points
therefore reject zone/offset tokens. Device use requires the involved standard
chrono operations to be available on that device.
