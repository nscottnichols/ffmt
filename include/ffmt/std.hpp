// Device-suitable standard data adapters. No streams, RTTI, filesystem, or
// exception services are used. Individual standard-library types must
// themselves be callable on the selected device. Semantics follow {fmt}/std.h.
#pragma once
#include <atomic>
#include <bitset>
#include <complex>
#include <ffmt/detail/family.hpp>
#include <functional>
#include <memory>
#include <optional>
#include <source_location>
#include <variant>

namespace ffmt {

template <typename T>
  requires std::is_pointer_v<T>
FMT_HD constexpr auto ptr(T value) -> const void * {
  static_assert(sizeof(T) <= sizeof(std::uintptr_t),
                "unsupported native pointer representation");
  return reinterpret_cast<const void *>(
      reinterpret_cast<std::uintptr_t>(value));
}
template <typename T, typename Deleter>
FMT_HD auto ptr(const std::unique_ptr<T, Deleter> &value) -> const void * {
  return ::ffmt::ptr(value.get());
}
template <typename T>
FMT_HD auto ptr(const std::shared_ptr<T> &value) -> const void * {
  return ::ffmt::ptr(value.get());
}

template <typename T, typename Char>
  requires(std::is_convertible_v<T, bool> &&
           std::is_nothrow_assignable_v<T, bool> &&
           requires(T value) { value.flip(); })
struct formatter<T, Char> : formatter<bool, Char> {
  template <typename Context>
  FMT_HD auto format(const T &value, Context &ctx) const
      -> decltype(ctx.out()) {
    return formatter<bool, Char>::format(static_cast<bool>(value), ctx);
  }
};

template <typename T, typename Char> struct formatter<std::optional<T>, Char> {
  using ffmt_family_type = std::optional<T>;
  static constexpr bool ffmt_family_repeatable =
      detail::family_repeatable<T, Char>;
  detail::formatter_adapter<std::remove_cv_t<T>, Char> underlying_;
  FMT_HD constexpr auto parse(basic_format_parse_context<Char> &ctx)
      -> const Char * {
    detail::family_debug(underlying_);
    return underlying_.parse(ctx);
  }
  template <typename Context>
  FMT_HD constexpr auto format(const std::optional<T> &value,
                               Context &ctx) const -> decltype(ctx.out()) {
    if (!value) {
      detail::family_ascii(ctx, "none");
      return ctx.out();
    }
    detail::family_ascii(ctx, "optional(");
    ctx.advance_to(underlying_.format(*value, ctx));
    detail::family_put(ctx, Char(')'));
    return ctx.out();
  }
};

template <typename Char> struct formatter<std::monostate, Char> {
  using ffmt_family_type = std::monostate;
  static constexpr bool ffmt_family_repeatable = true;
  FMT_HD constexpr auto parse(basic_format_parse_context<Char> &ctx)
      -> const Char * {
    return ctx.begin();
  }
  template <typename Context>
  FMT_HD constexpr auto format(std::monostate, Context &ctx) const
      -> decltype(ctx.out()) {
    detail::family_ascii(ctx, "monostate");
    return ctx.out();
  }
};

template <typename... T, typename Char>
  requires((is_formattable<T, Char> && ...))
struct formatter<std::variant<T...>, Char> {
  using ffmt_family_type = std::variant<T...>;
  static constexpr bool ffmt_family_repeatable =
      (detail::family_repeatable<T, Char> && ...);
  FMT_HD constexpr auto parse(basic_format_parse_context<Char> &ctx)
      -> const Char * {
    return ctx.begin();
  }
  template <std::size_t I = 0, typename Context>
  FMT_HD constexpr void alternative(const std::variant<T...> &value,
                                    Context &ctx) const {
    if constexpr (I < sizeof...(T)) {
      if (const auto *active = std::get_if<I>(&value))
        detail::family_default<
            std::variant_alternative_t<I, std::variant<T...>>, Char>(ctx,
                                                                     *active);
      else
        alternative<I + 1>(value, ctx);
    }
  }
  template <typename Context>
  FMT_HD constexpr auto format(const std::variant<T...> &value,
                               Context &ctx) const -> decltype(ctx.out()) {
    detail::family_ascii(ctx, "variant(");
    if (value.valueless_by_exception())
      detail::family_ascii(ctx, "valueless by exception");
    else
      alternative(value, ctx);
    detail::family_put(ctx, Char(')'));
    return ctx.out();
  }
};

template <std::size_t N, typename Char> struct formatter<std::bitset<N>, Char> {
  using ffmt_family_type = std::bitset<N>;
  static constexpr bool ffmt_family_repeatable = true;
  detail::basic_format_spec<Char> specs_{}, inner_{};
  FMT_HD constexpr auto parse(basic_format_parse_context<Char> &ctx)
      -> const Char * {
    detail::family_outer_specs(ctx, specs_);
    inner_ = detail::parse_standard_specs(ctx);
    if (!detail::compatible<basic_string_view<Char>>(inner_) ||
        (inner_.type && inner_.type != 's' && inner_.type != '?'))
      ctx.on_error(format_error::invalid_format);
    return ctx.begin();
  }
  template <typename Context>
  FMT_HD constexpr auto format(const std::bitset<N> &value, Context &ctx) const
      -> decltype(ctx.out()) {
    return detail::family_padded(ctx, specs_, [&](auto &bodyctx) {
      auto spec = detail::resolved(bodyctx, inner_);
      bool debug = spec.type == '?';
      std::size_t size = N + (debug ? 2 : 0);
      if (spec.precision >= 0 && !(debug && sizeof(Char) != 1) &&
          static_cast<std::size_t>(spec.precision) < size)
        size = static_cast<std::size_t>(spec.precision);
      detail::write_padded(bodyctx, spec, size, [&](auto &output) {
        std::size_t used = 0;
        if (debug && used < size) {
          detail::family_put(output, Char('"'));
          ++used;
        }
        for (std::size_t i = N; i && used < size; --i, ++used)
          detail::family_put(output, Char(value[i - 1] ? '1' : '0'));
        if (debug && used < size)
          detail::family_put(output, Char('"'));
      });
    });
  }
};

template <typename T, typename Char>
struct formatter<std::reference_wrapper<T>, Char>
    : detail::formatter_adapter<std::remove_cv_t<T>, Char> {
  using ffmt_family_type = std::reference_wrapper<T>;
  static constexpr bool ffmt_family_repeatable =
      detail::family_repeatable<T, Char>;
  template <typename Context>
  FMT_HD constexpr auto format(std::reference_wrapper<T> value,
                               Context &ctx) const -> decltype(ctx.out()) {
    return detail::formatter_adapter<std::remove_cv_t<T>, Char>::format(
        value.get(), ctx);
  }
};

template <typename Char>
struct formatter<std::byte, Char> : formatter<unsigned, Char> {
  using ffmt_family_type = std::byte;
  static constexpr bool ffmt_family_repeatable = true;
  template <typename Context>
  FMT_HD constexpr auto format(std::byte value, Context &ctx) const
      -> decltype(ctx.out()) {
    return formatter<unsigned, Char>::format(static_cast<unsigned>(value), ctx);
  }
};

template <typename T, typename Char>
struct formatter<std::atomic<T>, Char> : formatter<T, Char> {
  template <typename Context>
  FMT_HD auto format(const std::atomic<T> &value, Context &ctx) const
      -> decltype(ctx.out()) {
    return formatter<T, Char>::format(value.load(), ctx);
  }
};

#if defined(__cpp_lib_atomic_flag_test)
template <typename Char>
struct formatter<std::atomic_flag, Char> : formatter<bool, Char> {
  template <typename Context>
  FMT_HD auto format(const std::atomic_flag &value, Context &ctx) const
      -> decltype(ctx.out()) {
    return formatter<bool, Char>::format(value.test(), ctx);
  }
};
#endif

template <typename T, typename Char> struct formatter<std::complex<T>, Char> {
  using ffmt_family_type = std::complex<T>;
  static constexpr bool ffmt_family_repeatable = true;
  detail::basic_format_spec<Char> specs_{};
  FMT_HD constexpr auto parse(basic_format_parse_context<Char> &ctx)
      -> const Char * {
    specs_ = detail::parse_standard_specs(ctx);
    if (!detail::compatible<T>(specs_))
      ctx.on_error(format_error::incompatible_type);
    return ctx.begin();
  }
  template <typename Context>
  FMT_HD constexpr auto format(const std::complex<T> &value, Context &ctx) const
      -> decltype(ctx.out()) {
    auto inner = specs_;
    auto outer = specs_;
    if (outer.zero_pad && !outer.align) {
      outer.fill[0] = Char('0');
      outer.fill_size = 1;
      outer.align = '>';
    }
    inner.width = 0;
    inner.width_arg = -1;
    inner.width_name = {};
    inner.align = 0;
    inner.fill[0] = Char(' ');
    inner.fill_size = 1;
    return detail::family_padded(ctx, outer, [&](auto &bodyctx) {
      auto imag_spec = inner;
      if (value.real() != 0) {
        detail::family_put(bodyctx, Char('('));
        detail::write_value(bodyctx, value.real(), inner);
        imag_spec.sign = '+';
      }
      if constexpr (detail::integer<T>)
        detail::write_integer(bodyctx, value.imag(), imag_spec);
      else
        detail::write_value(bodyctx, value.imag(), imag_spec);
      if constexpr (std::is_floating_point_v<T>) {
        if (!std::isfinite(value.imag()))
          detail::family_put(bodyctx, Char(' '));
      }
      detail::family_put(bodyctx, Char('i'));
      if (value.real() != 0)
        detail::family_put(bodyctx, Char(')'));
    });
  }
};

template <> struct formatter<std::source_location, char> {
  using ffmt_family_type = std::source_location;
  static constexpr bool ffmt_family_repeatable = true;
  FMT_HD constexpr auto parse(basic_format_parse_context<char> &ctx) -> const
      char * {
    return ctx.begin();
  }
  template <typename Context>
  FMT_HD constexpr auto format(const std::source_location &value,
                               Context &ctx) const -> decltype(ctx.out()) {
    detail::family_ascii(ctx, value.file_name());
    detail::family_put(ctx, ':');
    detail::write_value(ctx, value.line(), detail::basic_format_spec<char>{});
    detail::family_put(ctx, ':');
    detail::write_value(ctx, value.column(), detail::basic_format_spec<char>{});
    detail::family_ascii(ctx, ": ");
    detail::family_ascii(ctx, value.function_name());
    return ctx.out();
  }
};
} // namespace ffmt
