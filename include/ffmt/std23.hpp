// Optional C++23 adapters are kept out of the C++20 header surface.
#pragma once
#include <ffmt/std.hpp>
#if __cplusplus > 202002L && __has_include(<expected>)
#include <expected>
#endif

#if defined(__cpp_lib_expected) && __cpp_lib_expected >= 202202L
namespace ffmt {
template <typename T, typename E, typename Char>
  requires((std::is_void_v<T> || is_formattable<T, Char>) &&
           is_formattable<E, Char>)
struct formatter<std::expected<T, E>, Char> {
  using ffmt_family_type = std::expected<T, E>;
  static constexpr bool ffmt_family_repeatable =
      (std::is_void_v<T> || detail::family_repeatable<T, Char>) &&
      detail::family_repeatable<E, Char>;
  FMT_HD constexpr auto parse(basic_format_parse_context<Char> &ctx)
      -> const Char * {
    return ctx.begin();
  }
  template <typename Context>
  FMT_HD constexpr auto format(const std::expected<T, E> &value,
                               Context &ctx) const -> decltype(ctx.out()) {
    if (value.has_value()) {
      detail::family_ascii(ctx, "expected(");
      if constexpr (!std::is_void_v<T>)
        detail::family_default<T, Char>(ctx, *value);
    } else {
      detail::family_ascii(ctx, "unexpected(");
      detail::family_default<E, Char>(ctx, value.error());
    }
    detail::family_put(ctx, Char(')'));
    return ctx.out();
  }
};
template <typename E, typename Char>
  requires(is_formattable<E, Char>)
struct formatter<std::unexpected<E>, Char> {
  using ffmt_family_type = std::unexpected<E>;
  static constexpr bool ffmt_family_repeatable =
      detail::family_repeatable<E, Char>;
  FMT_HD constexpr auto parse(basic_format_parse_context<Char> &ctx)
      -> const Char * {
    return ctx.begin();
  }
  template <typename Context>
  FMT_HD constexpr auto format(const std::unexpected<E> &value,
                               Context &ctx) const -> decltype(ctx.out()) {
    detail::family_ascii(ctx, "unexpected(");
    detail::family_default<E, Char>(ctx, value.error());
    detail::family_put(ctx, Char(')'));
    return ctx.out();
  }
};
} // namespace ffmt
#endif
