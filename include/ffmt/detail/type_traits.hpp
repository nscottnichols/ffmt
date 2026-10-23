#pragma once
namespace ffmt::detail {
template <typename T>
concept has_format_as = requires(const T &x) { format_as(x); };
template <typename T> FMT_HD constexpr decltype(auto) mapped_value(const T &x) {
  return format_as(x);
}
template <typename T, bool Mapped = has_format_as<T>> struct mapped_argument {
  using type = T;
};
template <typename T> struct mapped_argument<T, true> {
  using type =
      std::remove_cvref_t<decltype(mapped_value(std::declval<const T &>()))>;
};
template <typename T>
using mapped_argument_t = typename mapped_argument<T>::type;
template <typename T> inline constexpr bool legacy_formatter = false;

template <typename Context, typename T, typename Char>
FMT_HD void write_value(Context &, const T &, basic_format_spec<Char>);

} // namespace ffmt::detail
