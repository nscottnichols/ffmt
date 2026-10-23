// Explicit underlying enum representation. Equivalent to fmt::underlying and
// fmt::enums::format_as; identifier reflection requires C++26 and is not part
// of the C++20/C++23 device interface. See LICENSES/fmt.txt.
#pragma once
#include <ffmt/base.hpp>

namespace ffmt {
template <typename Enum>
  requires std::is_enum_v<Enum>
FMT_HD constexpr auto underlying(Enum value) noexcept
    -> std::underlying_type_t<Enum> {
  return static_cast<std::underlying_type_t<Enum>>(value);
}
namespace enums {
template <typename Enum>
  requires std::is_enum_v<Enum>
FMT_HD constexpr auto format_as(Enum value) noexcept
    -> std::underlying_type_t<Enum> {
  return ::ffmt::underlying(value);
}
} // namespace enums
} // namespace ffmt
