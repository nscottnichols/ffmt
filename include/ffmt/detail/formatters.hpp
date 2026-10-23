#pragma once
namespace ffmt {
// The primary formatter covers core types, ADL format_as, and the historical
// static format(T)->formatted protocol. Family headers specialize this class.
template <typename T, typename Char> struct formatter {
  using ffmt_primary_type = T;
  detail::basic_format_spec<Char> specs{};
  bool debug = false;
  FMT_HD constexpr void set_debug_format(bool enabled = true) {
    debug = enabled;
  }
  FMT_HD constexpr auto parse(basic_format_parse_context<Char> &pc)
      -> const Char * {
    specs = detail::parse_standard_specs(pc);
    if constexpr (detail::primitive<T, Char>) {
      if (!detail::compatible<T>(specs))
        pc.on_error(format_error::incompatible_type);
    } else if constexpr (detail::has_format_as<T>) {
      using U = std::remove_cvref_t<decltype(detail::mapped_value(
          std::declval<const T &>()))>;
      if constexpr (detail::primitive<U, Char>)
        if (!detail::compatible<U>(specs))
          pc.on_error(format_error::incompatible_type);
    } else
      pc.on_error(format_error::unsupported_type);
    return pc.begin();
  }
  template <typename Context>
  FMT_HD auto format(const T &value, Context &ctx) const ->
      typename Context::iterator {
    auto s = specs;
    if constexpr (detail::has_format_as<T>) {
      const auto &mapped = detail::mapped_value(value);
      using U = std::remove_cvref_t<decltype(mapped)>;
      if constexpr (detail::character<U, Char> ||
                    detail::string_like<U, Char> ||
                    detail::char_pointer<U, Char>)
        if (debug && !s.type)
          s.type = '?';
      detail::write_value(ctx, mapped, s);
    } else {
      if constexpr (detail::character<T, Char> ||
                    detail::string_like<T, Char> ||
                    detail::char_pointer<T, Char>)
        if (debug && !s.type)
          s.type = '?';
      detail::write_value(ctx, value, s);
    }
    return ctx.out();
  }
};

namespace detail {
template <typename Tuple, typename F, std::size_t... I>
FMT_HD constexpr bool visit_index_impl(const Tuple &args, int index, F &&f,
                                       std::index_sequence<I...>) {
  bool found = false;
  (((index == static_cast<int>(I))
        ? (f(unwrap(std::get<I>(args))), found = true, void())
        : void()),
   ...);
  return found;
}
template <typename Tuple, typename F>
FMT_HD constexpr bool visit_index(const Tuple &args, int index, F &&f) {
  if constexpr (std::is_same_v<Tuple, empty_args>)
    return false;
  else
    return visit_index_impl(
        args, index, static_cast<F &&>(f),
        std::make_index_sequence<std::tuple_size_v<Tuple>>{});
}
template <typename Char>
FMT_HD constexpr bool equal_text(basic_string_view<Char> a,
                                 basic_string_view<Char> b) {
  if (a.size() != b.size())
    return false;
  if (std::is_constant_evaluated()) {
    for (std::size_t i = 0; i < a.size(); ++i)
      if (a[i] != b[i])
        return false;
  } else {
    const volatile Char *x = a.data();
    const volatile Char *y = b.data();
    for (std::size_t i = 0; i < a.size(); ++i)
      if (x[i] != y[i])
        return false;
  }
  return true;
}
template <typename Tuple, typename Char, std::size_t... I>
FMT_HD constexpr int named_index_impl(const Tuple &args,
                                      basic_string_view<Char> name,
                                      std::index_sequence<I...>) {
  int result = -1;
  [[maybe_unused]] auto one = [&]<std::size_t N>() {
    const auto &arg = std::get<N>(args);
    if constexpr (named_arg<std::remove_cvref_t<decltype(arg)>>)
      if (equal_text(basic_string_view<Char>{arg.name}, name))
        result = static_cast<int>(N);
  };
  (one.template operator()<I>(), ...);
  return result;
}
template <typename Tuple, typename Char>
FMT_HD constexpr int named_index(const Tuple &args,
                                 basic_string_view<Char> name) {
  if constexpr (std::is_same_v<Tuple, empty_args>)
    return -1;
  else
    return named_index_impl(
        args, name, std::make_index_sequence<std::tuple_size_v<Tuple>>{});
}
}

}
