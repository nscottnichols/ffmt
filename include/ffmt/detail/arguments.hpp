#pragma once
namespace ffmt::detail {
template <std::size_t N, typename Char = char> struct fixed_string {
  Char data[N]{};
  FMT_HD constexpr fixed_string() {}
  FMT_HD constexpr fixed_string(const Char (&str)[N]) {
    for (std::size_t i = 0; i < N; ++i)
      data[i] = str[i];
  }
  FMT_HD constexpr Char operator[](std::size_t i) const { return data[i]; }
};
template <std::size_t N, typename Char>
fixed_string(const Char (&)[N]) -> fixed_string<N, Char>;
template <std::size_t N, typename Char>
consteval auto flen(const fixed_string<N, Char> &) {
  return N - 1;
}

struct index_state {
  int next = 0;
  bool automatic = false, manual = false;
  // GCC ASan cannot constant-evaluate a null comparison of the static table.
  bool has_dynamic_types = false;
  int available = -1;
  const bool *dynamic_types = nullptr;
};
struct empty_args {};
template <typename T> inline constexpr bool named_arg = false;
template <typename T> FMT_HD constexpr decltype(auto) unwrap(T &&x) {
  if constexpr (named_arg<std::remove_cvref_t<T>>)
    return (x.value);
  else
    return static_cast<T &&>(x);
}
template <typename T>
using unwrapped_reference_t = decltype(unwrap(std::declval<T &>()));
template <typename T>
using unwrapped_t = std::remove_cvref_t<decltype(unwrap(std::declval<T &>()))>;
template <typename T> inline constexpr bool extended_integer = false;
template <typename T> struct floating_traits {
  static constexpr bool value = std::is_same_v<T,float> || std::is_same_v<T,double>;
  static constexpr int precision = std::numeric_limits<T>::digits;
  static constexpr int decimal_digits = std::numeric_limits<T>::digits10;
};
template <typename T>
inline constexpr bool floating = floating_traits<T>::value;
template <typename T>
inline constexpr bool integer = std::is_integral_v<T> || extended_integer<T>;
template <typename T> struct unsigned_type {
  using type = std::make_unsigned_t<T>;
};
template <> struct unsigned_type<bool> {
  using type = unsigned;
};
template <typename T> using unsigned_t = typename unsigned_type<T>::type;
template <typename T>
inline constexpr bool signed_integer = [] {
  if constexpr (integer<T>)
    return T(-1) < T(0);
  else
    return false;
}();


}
