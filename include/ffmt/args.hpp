// Named arguments own their name view and refer to the supplied value.
#pragma once
#include <ffmt/base.hpp>

namespace ffmt {
template <typename Char, typename T> struct basic_named_arg {
  basic_string_view<Char> name;
  const T& value;
};
namespace detail {
template <typename Char, typename T>
inline constexpr bool named_arg<basic_named_arg<Char, T>> = true;
}

template <typename Char, typename T>
FMT_HD constexpr auto arg(const Char* name, const T& value)
    -> basic_named_arg<Char, T> { return {{name, detail::string_length(name)}, value}; }

template <typename Char, typename T>
FMT_HD constexpr auto arg(basic_string_view<Char> name, const T& value)
    -> basic_named_arg<Char, T> { return {name, value}; }
} // namespace ffmt
