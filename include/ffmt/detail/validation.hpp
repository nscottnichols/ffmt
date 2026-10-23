#pragma once
namespace ffmt {
namespace detail {
template <typename T, typename Char>
FMT_HD constexpr format_error
validate_one(basic_format_parse_context<Char> &pc) {
  using U = std::remove_cvref_t<T>;
  if constexpr (!formatter_invocable<T, Char>())
    return format_error::incompatible_type;
  {
    formatter<U, Char> fmt;
    auto it = fmt.parse(pc);
    pc.advance_to(it);
    return pc.error();
  }
}
template <typename Char, typename... Args> struct validation_handler {
  inline static constexpr bool dynamic_types[sizeof...(Args) + 1] = {
      (integer<mapped_argument_t<unwrapped_t<Args>>> &&
       !std::is_same_v<mapped_argument_t<unwrapped_t<Args>>, bool> &&
       !character<mapped_argument_t<unwrapped_t<Args>>, Char>)...,
      false};
  FMT_HD constexpr void initialize(index_state &state) {
    state.available = sizeof...(Args);
    state.dynamic_types = dynamic_types;
    state.has_dynamic_types = true;
  }
  FMT_HD constexpr void literal(Char) {}
  FMT_HD constexpr format_error argument(int index,
                                         basic_string_view<Char> name,
                                         basic_format_parse_context<Char> &pc) {
    if (!name.empty()) { // Named argument names are runtime values, not
                         // template arguments.
      constexpr bool named = (named_arg<std::remove_cvref_t<Args>> || ...);
      if constexpr (named) {
        pc.advance_to(pc.end() - 1);
        return format_error::none;
      }
      return format_error::argument_out_of_range;
    }
    format_error error = format_error::argument_out_of_range;
    int current = 0;
    ((index == current++
          ? (error = validate_one<unwrapped_reference_t<Args>>(pc), void())
          : void()),
     ...);
    return error;
  }
};
namespace consteval_error {
[[noreturn]] FMT_HD inline void invalid_format_spec() { __builtin_trap(); }
} // namespace consteval_error
template <typename Char, typename... Args>
consteval void validate_literal(basic_string_view<Char> s) {
  validation_handler<Char, Args...> handler;
  if (walk_format(s, handler) != format_error::none)
    consteval_error::invalid_format_spec();
}
} // namespace detail

template <typename Char, typename... Args> struct basic_format_string {
  basic_string_view<Char> str;
  template <std::size_t N>
  consteval basic_format_string(const Char (&s)[N]) : str(s, N - 1) {
    detail::validate_literal<Char, Args...>(str);
  }
};
template <typename... Args>
using format_string = basic_format_string<char, std::type_identity_t<Args>...>;
template <typename... Args>
using wformat_string =
    basic_format_string<wchar_t, std::type_identity_t<Args>...>;
namespace detail {
template <typename... Args>
using print_string = basic_format_string<char, Args...>;
}


}
