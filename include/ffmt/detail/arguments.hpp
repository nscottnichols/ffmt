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
#if defined(__SIZEOF_INT128__)
template <> inline constexpr bool extended_integer<__int128> = true;
template <> inline constexpr bool extended_integer<unsigned __int128> = true;
#endif
template <typename T> struct floating_traits {
  static constexpr bool value = std::is_floating_point_v<T>;
  static constexpr int precision = std::numeric_limits<T>::digits;
  static constexpr int decimal_digits = std::numeric_limits<T>::digits10;
};
#if defined(__SIZEOF_FLOAT128__) && !defined(__CUDACC__) &&                    \
    !defined(__HIPCC__) && !defined(SYCL_LANGUAGE_VERSION) &&                  \
    !FFMT_COMPILER_ACPP
#define FFMT_HAS_FLOAT128 1
template <> struct floating_traits<__float128> {
  static constexpr bool value = true;
  static constexpr int precision = 113, decimal_digits = 33;
};
#else
#define FFMT_HAS_FLOAT128 0
#endif
template <typename T>
inline constexpr bool floating = floating_traits<T>::value;
template <typename T>
inline constexpr bool integer = std::is_integral_v<T> || extended_integer<T>;
#if defined(__BITINT_MAXWIDTH__)
// Match fmt/std.h's native bit-precise integer surface (up to 64 bits).
template <int N> inline constexpr bool extended_integer<_BitInt(N)> = N <= 64;
template <int N>
inline constexpr bool extended_integer<unsigned _BitInt(N)> = N <= 64;
// libc++ recognizes every _BitInt width as integral, unlike libstdc++.
// Keep the supported surface independent of that standard-library trait.
template <int N> inline constexpr bool integer<_BitInt(N)> = N <= 64;
template <int N> inline constexpr bool integer<unsigned _BitInt(N)> = N <= 64;
#endif
template <typename T> struct unsigned_type {
  using type = std::make_unsigned_t<T>;
};
#if defined(__SIZEOF_INT128__)
template <> struct unsigned_type<__int128> {
  using type = unsigned __int128;
};
template <> struct unsigned_type<unsigned __int128> {
  using type = unsigned __int128;
};
#endif
#if defined(__BITINT_MAXWIDTH__)
template <int N> struct unsigned_type<_BitInt(N)> {
  using type = unsigned _BitInt(N);
};
template <int N> struct unsigned_type<unsigned _BitInt(N)> {
  using type = unsigned _BitInt(N);
};
#endif
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
