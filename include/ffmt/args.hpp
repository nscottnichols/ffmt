// Named arguments hold a name view and a reference to the supplied value.
// Neither the characters nor the value are copied. Keep both alive while the
// wrapper is used; a temporary value is safe within the formatting expression.
#pragma once
#include <ffmt/base.hpp>

namespace ffmt {
template <typename Char, typename T> struct basic_named_arg {
  basic_string_view<Char> name;
  T &value;
};
namespace detail {
template <typename Char, typename T>
inline constexpr bool named_arg<basic_named_arg<Char, T>> = true;
}

template <typename Char, typename T>
FMT_HD constexpr auto arg(const Char *name, T &&value)
    -> basic_named_arg<Char, std::remove_reference_t<T>> {
  return {{name, detail::string_length(name)}, value};
}

template <typename Char, typename T>
FMT_HD constexpr auto arg(basic_string_view<Char> name, T &&value)
    -> basic_named_arg<Char, std::remove_reference_t<T>> {
  return {name, value};
}
} // namespace ffmt
