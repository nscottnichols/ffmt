#pragma once
namespace ffmt {
template <typename Char> using basic_string_view = std::basic_string_view<Char>;
using string_view = basic_string_view<char>;
using wstring_view = basic_string_view<wchar_t>;
namespace detail {
// Host char_traits::length is not device-callable on all standard libraries.
// Volatile loads in the runtime branch prevent loop-idiom lowering to an
// unresolved device strlen symbol (including at optimization level zero).
template <typename Char>
FMT_HD constexpr std::size_t string_length(const Char *text) {
  std::size_t size = 0;
  if (std::is_constant_evaluated()) {
    while (text[size] != Char())
      ++size;
  } else {
    const volatile Char *input = text;
    while (input[size] != Char())
      ++size;
  }
  return size;
}
template <typename Char>
FMT_HD constexpr auto to_string_view(const Char *text)
    -> basic_string_view<Char> {
  return {text, string_length(text)};
}
} // namespace detail

enum class format_error {
  none,
  invalid_format,
  argument_out_of_range,
  incompatible_type,
  invalid_dynamic_spec,
  invalid_code_point,
  unsupported_type,
  transport_error,
  output_overflow
};
struct format_result {
  std::size_t written = 0;
  std::size_t required = 0;
  format_error error = format_error::none;
  FMT_HD constexpr explicit operator bool() const {
    return error == format_error::none;
  }
};
template <typename OutputIt> struct format_to_result : format_result {
  OutputIt out;
  FMT_HD constexpr operator OutputIt() const { return out; }
};
template <typename Char> struct runtime_format_string {
  basic_string_view<Char> str;
};
template <typename Char>
FMT_HD constexpr auto runtime(basic_string_view<Char> str) {
  return runtime_format_string<Char>{str};
}
template <typename Char> FMT_HD constexpr auto runtime(const Char *str) {
  return runtime(basic_string_view<Char>{str, detail::string_length(str)});
}

template <typename T, typename Char = char> struct formatter;
}
