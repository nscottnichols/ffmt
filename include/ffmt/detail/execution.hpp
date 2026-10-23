#pragma once
namespace ffmt {
// The engine below is independent of the destination and shared by buffers,
// counting, host stdout, and accelerator stdout transports.
namespace detail {
template <typename Context, typename T, typename Char>
FMT_HD void write_nested(Context &, T &&, basic_string_view<Char>,
                         bool = false);
template <typename Context, typename T, typename Char>
FMT_HD void format_selected(Context &ctx, T &&value,
                            basic_format_parse_context<Char> &pc) {
  using U = std::remove_cvref_t<T>;
  if constexpr (!formatter_invocable<T, Char>()) {
    ctx.on_error(format_error::incompatible_type);
  } else {
    formatter<U, Char> fmt;
    auto next = fmt.parse(pc);
    pc.advance_to(next);
    if (pc.error() != format_error::none) {
      ctx.on_error(pc.error());
      return;
    }
    ctx.advance_to(fmt.format(value, ctx));
  }
}
template <typename Context, typename Tuple> struct execution_handler {
  using Char = typename Context::char_type;
  Context &ctx;
  const Tuple &args;
  FMT_HD void literal(Char c) {
    if (ctx.error() == format_error::none)
      put(ctx, c);
  }
  FMT_HD format_error argument(int index, basic_string_view<Char> name,
                               basic_format_parse_context<Char> &pc) {
    if (!name.empty())
      index = named_index(args, name);
    bool found = visit_index(
        args, index, [&](auto &&value) { format_selected(ctx, value, pc); });
    return found ? ctx.error() : format_error::argument_out_of_range;
  }
};
template <typename Context, typename T, typename Char>
FMT_HD void write_nested(Context &ctx, T &&value, basic_string_view<Char> specs,
                         bool debug) {
  basic_format_parse_context<Char> pc{specs};
  using U = std::remove_cvref_t<T>;
  if constexpr (!formatter_invocable<T, Char>()) {
    ctx.on_error(format_error::incompatible_type);
  } else {
    formatter<U, Char> fmt;
    if constexpr (requires { fmt.set_debug_format(debug); })
      fmt.set_debug_format(debug);
    auto p = fmt.parse(pc);
    if (p != pc.end() && *p != Char('}'))
      pc.on_error(format_error::invalid_format);
    if (pc.error() != format_error::none) {
      ctx.on_error(pc.error());
      return;
    }
    ctx.advance_to(fmt.format(value, ctx));
  }
}
template <typename Sink, typename Char, typename... Args>
FMT_HD void run_format(Sink &sink, basic_string_view<Char> text,
                       Args &&...args) {
  auto tuple = std::tuple<Args &...>{args...};
  basic_format_context<sink_iterator<Sink>, Char, decltype(tuple)> ctx{
      {&sink}, sink.error, &tuple};
  execution_handler<decltype(ctx), decltype(tuple)> handler{ctx, tuple};
  auto error = walk_format(text, handler);
  ctx.on_error(error);
}
} // namespace detail


}
