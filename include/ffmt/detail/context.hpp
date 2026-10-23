#pragma once
namespace ffmt {
template <typename OutputIt, typename Char,
          typename ArgsTuple = detail::empty_args>
class basic_format_context {
  OutputIt out_;
  format_error *error_;
  const ArgsTuple *args_;

public:
  using iterator = OutputIt;
  using char_type = Char;
  FMT_HD constexpr basic_format_context(OutputIt out, format_error &error,
                                        const ArgsTuple *args = nullptr)
      : out_(out), error_(&error), args_(args) {}
  FMT_HD constexpr auto out() const -> OutputIt { return out_; }
  FMT_HD constexpr void advance_to(OutputIt out) { out_ = out; }
  FMT_HD constexpr void on_error(format_error error) {
    if (*error_ == format_error::none)
      *error_ = error;
  }
  FMT_HD constexpr void on_error(const char *) {
    on_error(format_error::invalid_format);
  }
  FMT_HD constexpr auto error() const -> format_error { return *error_; }
  FMT_HD constexpr int arg_int(int index) {
    int result = 0;
    auto resolve = [&](const auto &x) {
      using T = std::remove_cvref_t<decltype(x)>;
      if constexpr (detail::integer<T> && !std::is_same_v<T, bool> &&
                    !detail::character<T, Char>) {
        if constexpr (detail::signed_integer<T>)
          if (x < 0) {
            on_error(format_error::invalid_dynamic_spec);
            return;
          }
        if (static_cast<detail::unsigned_t<T>>(x) >
            static_cast<detail::unsigned_t<T>>(INT_MAX))
          on_error(format_error::invalid_dynamic_spec);
        else
          result = static_cast<int>(x);
      } else
        on_error(format_error::invalid_dynamic_spec);
    };
    bool found =
        args_ && detail::visit_index(*args_, index, [&](const auto &value) {
          using T = std::remove_cvref_t<decltype(value)>;
          if constexpr (detail::has_format_as<T>)
            resolve(detail::mapped_value(value));
          else
            resolve(value);
        });
    if (!found)
      on_error(format_error::argument_out_of_range);
    return result;
  }
  FMT_HD constexpr int arg_int(basic_string_view<Char> name) {
    return arg_int(args_ ? detail::named_index(*args_, name) : -1);
  }
  template <typename It> FMT_HD constexpr auto rebind(It out) {
    return basic_format_context<It, Char, ArgsTuple>{out, *error_, args_};
  }
};

namespace detail {
template <typename T, typename Char> consteval bool formatter_invocable() {
  using U = std::remove_cvref_t<T>;
  using Value = std::remove_reference_t<T>;
  if constexpr (legacy_formatter<U>)
    return requires(Value &value) { formatter<U>::format(value); };
  else
    return requires(formatter<U, Char> &f,
                    basic_format_context<Char *, Char> &ctx,
                    Value &value) { f.format(value, ctx); };
}
template <typename T, typename Char> consteval bool formattable_type() {
  using U = std::decay_t<T>;
  using F = formatter<U, Char>;
  if constexpr (legacy_formatter<U>)
    return formatter_invocable<T, Char>();
  else if constexpr (requires { typename F::ffmt_primary_type; }) {
    if constexpr (std::is_same_v<typename F::ffmt_primary_type, U>) {
      if constexpr (primitive<U, Char>)
        return true;
      else if constexpr (has_format_as<U>) {
        using M = std::remove_cvref_t<decltype(mapped_value(
            std::declval<const U &>()))>;
        return primitive<M, Char>;
      } else
        return false;
    }
  }
  return requires(F &f, basic_format_parse_context<Char> &pc) {
    f.parse(pc);
  } && formatter_invocable<T, Char>();
}
} // namespace detail
template <typename T, typename Char = char>
inline constexpr bool is_formattable = detail::formattable_type<T, Char>();
template <typename T>
concept has_formatter = is_formattable<T>;
template <typename T>
concept sycl_formattable = has_formatter<T>;

namespace detail {
template <typename Context>
FMT_HD constexpr void put(Context &ctx, typename Context::char_type c) {
  auto it = ctx.out();
  *it++ = c;
  ctx.advance_to(it);
}
template <typename Context>
FMT_HD void repeat(Context &ctx, typename Context::char_type c, std::size_t n) {
  auto it = ctx.out();
  if constexpr (requires { it.repeat(c, n); })
    it.repeat(c, n);
  else
    for (std::size_t i = 0; i < n; ++i)
      *it++ = c;
  ctx.advance_to(it);
}
template <typename Context, typename Char>
FMT_HD void append(Context &ctx, const Char *p, std::size_t n) {
  for (std::size_t i = 0; i < n; ++i)
    put(ctx, static_cast<typename Context::char_type>(p[i]));
}
template <typename Context> FMT_HD void ascii(Context &ctx, const char *p) {
  while (*p)
    put(ctx, static_cast<typename Context::char_type>(*p++));
}
template <typename Context, typename Char>
FMT_HD auto resolved(Context &ctx, basic_format_spec<Char> s) {
  if (s.width_arg >= 0)
    s.width = ctx.arg_int(s.width_arg);
  if (!s.width_name.empty())
    s.width = ctx.arg_int(s.width_name);
  if (s.precision_arg >= 0)
    s.precision = ctx.arg_int(s.precision_arg);
  if (!s.precision_name.empty())
    s.precision = ctx.arg_int(s.precision_name);
  return s;
}
template <typename Context, typename Char, typename Body>
FMT_HD void write_padded(Context &ctx, basic_format_spec<Char> specs,
                         std::size_t columns, Body body,
                         char default_align = '<') {
  auto s = resolved(ctx, specs);
  if (ctx.error() != format_error::none)
    return;
  std::size_t padding =
      s.width > 0 && static_cast<std::size_t>(s.width) > columns
          ? static_cast<std::size_t>(s.width) - columns
          : 0;
  char align = s.align ? s.align : default_align;
  std::size_t left = align == '>' ? padding : align == '^' ? padding / 2 : 0;
  auto fill = [&](std::size_t count) {
    if (s.fill_size == 1)
      repeat(ctx, s.fill[0], count);
    else
      for (std::size_t i = 0; i < count; ++i)
        append(ctx, s.fill, s.fill_size);
  };
  fill(left);
  body(ctx);
  fill(padding - left);
}

} // namespace detail

} // namespace ffmt
