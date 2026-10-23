#pragma once
namespace ffmt::detail {
template <typename T, typename Char>
inline constexpr bool character =
    std::is_same_v<T, Char> ||
    (std::is_same_v<T, char> && !std::is_same_v<Char, char>);
template <typename T, typename Char>
inline constexpr bool string_like =
    std::is_convertible_v<const T &, basic_string_view<Char>>;
template <typename T, typename Char>
inline constexpr bool char_pointer =
    std::is_pointer_v<T> &&
    std::is_same_v<std::remove_cv_t<std::remove_pointer_t<T>>, Char>;
template <typename T, typename Char>
FMT_HD constexpr bool compatible(const basic_format_spec<Char> &s) {
  constexpr bool ch = character<T, Char>;
  if constexpr ((std::is_same_v<T, wchar_t> || std::is_same_v<T, char16_t> ||
                 std::is_same_v<T, char32_t>) &&
                !std::is_same_v<T, Char>)
    return false;
  constexpr bool str = string_like<T, Char> || char_pointer<T, Char>;
  constexpr bool flt = false;
  bool num = (integer<T> && !ch) || flt ||
             (ch && s.type && s.type != 'c' && s.type != '?');
  if (s.sign && !(flt || (signed_integer<T> && !ch)))
    return false;
  if ((s.alt || s.zero_pad) && !num)
    return false;
  if ((s.precision >= 0 || s.precision_arg >= 0 || !s.precision_name.empty()) &&
      !(flt || str))
    return false;
  char t = s.type;
  if constexpr (str)
    return !t || t == 's' || t == 'p';
  if constexpr (std::is_pointer_v<T> || std::is_null_pointer_v<T>)
    return !t || t == 'p';
  if constexpr (std::is_same_v<T, bool>)
    return !t || t == 's' || t == 'd' || t == 'b' || t == 'B' || t == 'o' ||
           t == 'x' || t == 'X';
  if constexpr (integer<T>)
    return !t || t == 'd' || t == 'b' || t == 'B' || t == 'o' || t == 'x' ||
           t == 'X' || t == 'c';
  if constexpr (flt)
    return !t || t == 'f' || t == 'F' || t == 'e' || t == 'E' || t == 'g' ||
           t == 'G' || t == 'a' || t == 'A';
  return false;
}
template <typename T, typename Char>
inline constexpr bool primitive =
    integer<T> || std::is_pointer_v<T> ||
    std::is_null_pointer_v<T> || string_like<T, Char>;

}
