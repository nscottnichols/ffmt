#pragma once
namespace ffmt::detail {
template <typename T> inline constexpr bool has_format_as = false;
template <typename T> FMT_HD constexpr decltype(auto) mapped_value(const T &x) {
  return x;
}
template <typename T> using mapped_argument_t = T;
template <typename T> inline constexpr bool legacy_formatter = false;
template <typename Context, typename T, typename Char>
FMT_HD void write_value(Context &, const T &, basic_format_spec<Char>);

} // namespace ffmt::detail
