// Allocation-free ranges, tuple-like values, and joins. Formatting grammar and
// debug defaults follow {fmt}; see LICENSES/fmt.txt for its license.
#pragma once
#include <array>
#include <ffmt/detail/family.hpp>
#include <iterator>
#include <queue>
#include <stack>
#include <tuple>
#include <type_traits>

namespace ffmt {
enum class range_format { disabled, map, set, sequence, string, debug_string };

namespace detail {
using std::begin;
using std::end;
template <typename R>
FMT_HD constexpr auto family_begin(R &r) -> decltype(begin(r)) {
  return begin(r);
}
template <typename R>
FMT_HD constexpr auto family_end(R &r) -> decltype(end(r)) {
  return end(r);
}
template <typename R>
concept family_range = requires(R &r) {
  family_begin(r);
  family_end(r);
  *family_begin(r);
};
template <typename T> struct family_string : std::false_type {};
template <typename C, typename Tr, typename A>
struct family_string<std::basic_string<C, Tr, A>> : std::true_type {};
template <typename C, typename Tr>
struct family_string<std::basic_string_view<C, Tr>> : std::true_type {};
template <typename T>
inline constexpr bool family_character_array =
    std::is_array_v<T> && (std::is_same_v<std::remove_extent_t<T>, char> ||
                           std::is_same_v<std::remove_extent_t<T>, wchar_t>);
template <typename R>
using family_range_value =
    std::remove_cvref_t<decltype(*family_begin(std::declval<R &>()))>;
template <typename R>
inline constexpr bool family_forward_range =
    std::forward_iterator<decltype(family_begin(std::declval<R &>()))>;
template <typename R, typename Char> consteval bool family_range_invocable() {
  if constexpr (family_range<R>)
    return formatter_invocable<decltype(*family_begin(std::declval<R &>())),
                               Char>();
  else
    return false;
}

template <typename T>
concept family_tuple = requires { std::tuple_size<T>::value; };
using std::get;
template <std::size_t I, typename T>
FMT_HD constexpr decltype(auto) family_get(const T &t) {
  return get<I>(t);
}

// Own the tiny default delimiters so their addresses always belong to the
// formatter's execution space. User-supplied delimiters retain view semantics.
template <typename Char, std::size_t N> struct family_delimiter {
  Char units[N];
  basic_string_view<Char> custom;
  bool external = false;
  template <typename... C>
  FMT_HD constexpr family_delimiter(C... value) : units{Char(value)...} {}
  FMT_HD constexpr auto view() const -> basic_string_view<Char> {
    return external ? custom : basic_string_view<Char>{units, N};
  }
  FMT_HD constexpr void set(basic_string_view<Char> value) {
    custom = value;
    external = true;
  }
  template <typename... C> FMT_HD constexpr void set_chars(C... value) {
    Char replacement[N] = {Char(value)...};
    for (std::size_t i = 0; i < N; ++i)
      units[i] = replacement[i];
    external = false;
  }
};

// Decode at most one UTF-8 code point at a time. This also supports character
// ranges without contiguous storage, preserving invalid bytes for debug
// escapes.
template <typename Context, typename R>
FMT_HD void family_char_range(Context &ctx, R &range, bool debug,
                              bool quote_plain = true, int precision = -1) {
  using Char = typename Context::char_type;
  std::size_t used = 0;
  std::size_t limit = precision < 0 ? (std::numeric_limits<std::size_t>::max)()
                                    : static_cast<std::size_t>(precision);
  if constexpr (sizeof(Char) != 1)
    if (debug)
      limit = (std::numeric_limits<std::size_t>::max)();
  auto emit = [&](const Char *data, std::size_t size, std::size_t columns) {
    if (columns > limit - used) {
      if (debug && columns == size) {
        size = limit - used;
        columns = size;
      } else
        return false;
    }
    family_text(ctx, basic_string_view<Char>{data, size});
    used += columns;
    return used < limit;
  };
  Char quote = Char('"');
  if (debug || quote_plain) {
    if (!emit(&quote, 1, 1))
      return;
  }
  if (used == limit)
    return;
  auto it = family_begin(range);
  auto end = family_end(range);
  Char pending[4];
  unsigned count = 0;
  bool complete = true;
  while (count || it != end) {
    if (!count)
      pending[count++] = Char(*it++);
    unsigned needed = 1;
    std::uint32_t point = static_cast<std::make_unsigned_t<Char>>(pending[0]);
    if constexpr (sizeof(Char) == 1) {
#if FFMT_UNICODE
      auto first = static_cast<unsigned char>(pending[0]);
      needed = first >= 0xc2 && first <= 0xdf   ? 2
               : first >= 0xe0 && first <= 0xef ? 3
               : first >= 0xf0 && first <= 0xf4 ? 4
                                                : 1;
      while (count < needed && it != end)
        pending[count++] = Char(*it++);
      needed = decode_utf8(pending, count, point);
#endif
    }
    if (debug) {
      auto piece = escaped_piece(point, pending, needed, false);
      if (!emit(piece.data, piece.size, piece.columns)) {
        complete = false;
        break;
      }
    } else if (!emit(pending, needed,
                     sizeof(Char) == 1 ? display_width_of(point) : 1)) {
      complete = false;
      break;
    }
    for (unsigned i = needed; i < count; ++i)
      pending[i - needed] = pending[i];
    count -= needed;
  }
  if (complete && (debug || quote_plain) && used < limit)
    emit(&quote, 1, 1);
}
} // namespace detail

namespace detail {
template <typename R, typename = void>
struct family_has_mapped : std::false_type {};
template <typename R>
struct family_has_mapped<R, std::void_t<typename R::mapped_type>>
    : std::true_type {};
template <typename R, typename = void>
struct family_has_key : std::false_type {};
template <typename R>
struct family_has_key<R, std::void_t<typename R::key_type>> : std::true_type {};
template <typename R>
inline constexpr range_format family_range_kind =
    !family_range<R> || family_string<R>::value || family_character_array<R>
        ? range_format::disabled
    : family_has_mapped<R>::value ? range_format::map
    : family_has_key<R>::value    ? range_format::set
                                  : range_format::sequence;
} // namespace detail
template <typename R, typename Char>
struct range_format_kind
    : std::integral_constant<range_format, detail::family_range_kind<R>> {};

template <typename Tuple, typename Char>
  requires(detail::family_tuple<Tuple> && !detail::family_range<Tuple> &&
           !detail::has_format_as<Tuple>)
struct formatter<Tuple, Char> {
private:
  template <std::size_t... I>
  static consteval bool repeatable(std::index_sequence<I...>) {
    return (detail::family_repeatable<
                std::remove_cvref_t<decltype(detail::family_get<I>(
                    std::declval<Tuple>()))>,
                Char> &&
            ...);
  }
  template <std::size_t... I>
  static auto types(std::index_sequence<I...>)
      -> std::tuple<detail::formatter_adapter<
          std::remove_cvref_t<
              decltype(detail::family_get<I>(std::declval<Tuple>()))>,
          Char>...>;
  decltype(types(
      std::make_index_sequence<std::tuple_size_v<Tuple>>{})) elements_;
  detail::basic_format_spec<Char> specs_{};
  detail::family_delimiter<Char, 2> separator_{',', ' '};
  detail::family_delimiter<Char, 1> opening_{'('};
  detail::family_delimiter<Char, 1> closing_{')'};

  template <std::size_t... I>
  FMT_HD constexpr void parse_elements(basic_format_parse_context<Char> &ctx,
                                       std::index_sequence<I...>) {
    (
        [&] {
          auto &f = std::get<I>(elements_);
          detail::family_debug(f);
          f.parse(ctx);
        }(),
        ...);
  }
  template <typename Context, std::size_t... I>
  FMT_HD constexpr void body(Context &ctx, const Tuple &value,
                             std::index_sequence<I...>) const {
    detail::family_text(ctx, opening_.view());
    (
        [&] {
          if constexpr (I)
            detail::family_text(ctx, separator_.view());
          ctx.advance_to(
              std::get<I>(elements_).format(detail::family_get<I>(value), ctx));
        }(),
        ...);
    detail::family_text(ctx, closing_.view());
  }

public:
  using ffmt_family_type = Tuple;
  static constexpr bool ffmt_family_repeatable =
      repeatable(std::make_index_sequence<std::tuple_size_v<Tuple>>{});
  FMT_HD constexpr void set_separator(basic_string_view<Char> value) {
    separator_.set(value);
  }
  FMT_HD constexpr void set_separator_chars(Char first, Char second) {
    separator_.set_chars(first, second);
  }
  FMT_HD constexpr void set_brackets(basic_string_view<Char> open,
                                     basic_string_view<Char> close) {
    opening_.set(open);
    closing_.set(close);
  }
  FMT_HD constexpr auto parse(basic_format_parse_context<Char> &ctx)
      -> const Char * {
    auto it = detail::family_outer_specs(ctx, specs_);
    if (it != ctx.end() && *it == Char('n')) {
      ++it;
      set_brackets({}, {});
      set_separator({});
    }
    if (it != ctx.end() && *it != Char('}'))
      ctx.on_error(format_error::invalid_format);
    ctx.advance_to(it);
    parse_elements(ctx, std::make_index_sequence<std::tuple_size_v<Tuple>>{});
    return it;
  }
  template <typename Context>
  FMT_HD constexpr auto format(const Tuple &value, Context &ctx) const
      -> decltype(ctx.out()) {
    return detail::family_padded(
        ctx, specs_,
        [&](auto &bodyctx) {
          body(bodyctx, value,
               std::make_index_sequence<std::tuple_size_v<Tuple>>{});
        },
        ffmt_family_repeatable);
  }
};

template <typename T, typename Char = char> struct range_formatter {
private:
  detail::formatter_adapter<T, Char> element_;
  detail::basic_format_spec<Char> specs_{};
  detail::family_delimiter<Char, 2> separator_{',', ' '};
  detail::family_delimiter<Char, 1> opening_{'['};
  detail::family_delimiter<Char, 1> closing_{']'};
  bool string_ = false, debug_string_ = false, quote_plain_string_ = true;

  template <typename R, typename Context>
  FMT_HD constexpr void body(R &range, Context &ctx) const {
    if (string_) {
      if constexpr (std::is_same_v<T, Char>)
        detail::family_char_range(ctx, range, debug_string_,
                                  quote_plain_string_);
      else
        ctx.on_error(format_error::unsupported_type);
      return;
    }
    detail::family_text(ctx, opening_.view());
    bool first = true;
    auto end = detail::family_end(range);
    for (auto it = detail::family_begin(range); it != end; ++it) {
      if (!first)
        detail::family_text(ctx, separator_.view());
      first = false;
      ctx.advance_to(element_.format(*it, ctx));
    }
    detail::family_text(ctx, closing_.view());
  }

public:
  FMT_HD constexpr auto underlying() -> detail::formatter_adapter<T, Char> & {
    return element_;
  }
  FMT_HD constexpr void set_string_format(bool debug = false) {
    string_ = true;
    debug_string_ = debug;
    quote_plain_string_ = false;
  }
  FMT_HD constexpr void set_separator(basic_string_view<Char> value) {
    separator_.set(value);
  }
  FMT_HD constexpr void set_brackets(basic_string_view<Char> open,
                                     basic_string_view<Char> close) {
    opening_.set(open);
    closing_.set(close);
  }
  FMT_HD constexpr void set_bracket_chars(Char open, Char close) {
    opening_.set_chars(open);
    closing_.set_chars(close);
  }
  FMT_HD constexpr auto parse(basic_format_parse_context<Char> &ctx)
      -> const Char * {
    auto it = detail::family_outer_specs(ctx, specs_);
    detail::family_debug(element_);
    if (it != ctx.end() && *it == Char('n')) {
      ++it;
      set_brackets({}, {});
    }
    if (it != ctx.end() && (*it == Char('s') || *it == Char('?'))) {
      string_ = true;
      if (*it == Char('?')) {
        debug_string_ = true;
        ++it;
      }
      if (it == ctx.end() || *it != Char('s') || !std::is_same_v<T, Char>)
        ctx.on_error(format_error::invalid_format);
      else
        ++it;
      ctx.advance_to(it);
      return it;
    }
    if (it != ctx.end() && *it != Char('}')) {
      if (*it != Char(':')) {
        ctx.on_error(format_error::invalid_format);
        return it;
      }
      ++it;
      detail::family_debug(element_, false);
    }
    ctx.advance_to(it);
    return element_.parse(ctx);
  }
  template <typename R, typename Context>
  FMT_HD constexpr auto format(R &range, Context &ctx) const
      -> decltype(ctx.out()) {
    return detail::family_padded(
        ctx, specs_, [&](auto &bodyctx) { body(range, bodyctx); },
        detail::family_forward_range<R> && detail::family_repeatable<T, Char>);
  }
};

template <typename R, typename Char>
  requires(range_format_kind<R, Char>::value != range_format::disabled &&
           range_format_kind<R, Char>::value != range_format::string &&
           range_format_kind<R, Char>::value != range_format::debug_string &&
           !detail::has_format_as<R>)
struct formatter<R, Char> {
private:
  using T = detail::family_range_value<R>;
  range_formatter<T, Char> range_;

public:
  using ffmt_family_type = R;
  static constexpr bool ffmt_family_repeatable =
      detail::family_forward_range<R> && detail::family_repeatable<T, Char>;
  FMT_HD constexpr formatter() {
    constexpr auto kind = range_format_kind<R, Char>::value;
    if constexpr (kind == range_format::map || kind == range_format::set)
      range_.set_bracket_chars(Char('{'), Char('}'));
    if constexpr (kind == range_format::map) {
      range_.underlying().set_brackets({}, {});
      range_.underlying().set_separator_chars(Char(':'), Char(' '));
    }
  }
  FMT_HD constexpr auto parse(basic_format_parse_context<Char> &ctx)
      -> const Char * {
    if constexpr (range_format_kind<R, Char>::value == range_format::map) {
      // The map grammar has no nonempty nested element specification.
      for (auto it = ctx.begin(); it != ctx.end() && *it != Char('}'); ++it) {
        if (*it == Char(':') && it + 1 != ctx.end() && it[1] != Char('}')) {
          ctx.on_error(format_error::invalid_format);
          return it;
        }
      }
    }
    return range_.parse(ctx);
  }
  template <typename Context>
    requires(detail::family_range_invocable<R, Char>())
  FMT_HD constexpr auto format(R &value, Context &ctx) const
      -> decltype(ctx.out()) {
    return range_.format(value, ctx);
  }
  template <typename Context>
    requires(detail::family_range_invocable<const R, Char>())
  FMT_HD constexpr auto format(const R &value, Context &ctx) const
      -> decltype(ctx.out()) {
    return range_.format(value, ctx);
  }
};

namespace detail {
template <typename Adaptor, typename Char>
struct family_adaptor_formatter
    : formatter_adapter<typename Adaptor::container_type, Char> {
  using Container = typename Adaptor::container_type;
  using ffmt_family_type = Adaptor;
  static constexpr bool ffmt_family_repeatable =
      family_repeatable<Container, Char>;
  template <typename Context>
  FMT_HD auto format(const Adaptor &value, Context &ctx) const
      -> decltype(ctx.out()) {
    // The protected member is accessed through a derived-class member pointer;
    // no layout assumptions, copy, iteration of pop(), or allocation is needed.
    struct access : Adaptor {
      FMT_HD static const Container &get(const Adaptor &item) {
        return item.*(&access::c);
      }
    };
    return formatter_adapter<Container, Char>::format(access::get(value), ctx);
  }
};
} // namespace detail
template <typename T, typename Container, typename Char>
struct formatter<std::stack<T, Container>, Char>
    : detail::family_adaptor_formatter<std::stack<T, Container>, Char> {};
template <typename T, typename Container, typename Char>
struct formatter<std::queue<T, Container>, Char>
    : detail::family_adaptor_formatter<std::queue<T, Container>, Char> {};
template <typename T, typename Container, typename Compare, typename Char>
struct formatter<std::priority_queue<T, Container, Compare>, Char>
    : detail::family_adaptor_formatter<
          std::priority_queue<T, Container, Compare>, Char> {};

template <typename R, typename Char>
  requires((range_format_kind<R, Char>::value == range_format::string ||
            range_format_kind<R, Char>::value == range_format::debug_string) &&
           !detail::has_format_as<R>)
struct formatter<R, Char> {
  using ffmt_family_type = R;
  static constexpr bool ffmt_family_repeatable =
      detail::family_forward_range<R>;
  detail::basic_format_spec<Char> specs_;
  FMT_HD constexpr auto parse(basic_format_parse_context<Char> &ctx)
      -> const Char * {
    specs_ = detail::parse_standard_specs(ctx);
    if (!detail::compatible<basic_string_view<Char>>(specs_) ||
        (specs_.type && specs_.type != 's' && specs_.type != '?'))
      ctx.on_error(format_error::incompatible_type);
    if constexpr (range_format_kind<R, Char>::value ==
                  range_format::debug_string)
      specs_.type = '?';
    return ctx.begin();
  }

private:
  template <typename Range, typename Context>
  FMT_HD auto format_range(Range &value, Context &ctx) const
      -> decltype(ctx.out()) {
    if constexpr (std::is_same_v<detail::family_range_value<Range>, Char>) {
      auto spec = detail::resolved(ctx, specs_);
      if (ctx.error() != format_error::none)
        return ctx.out();
      return detail::family_padded(
          ctx, spec,
          [&](auto &output) {
            detail::family_char_range(output, value, spec.type == '?', false,
                                      spec.precision);
          },
          detail::family_forward_range<Range>);
    } else {
      ctx.on_error(format_error::unsupported_type);
      return ctx.out();
    }
  }

public:
  template <typename Context>
  FMT_HD auto format(R &value, Context &ctx) const -> decltype(ctx.out()) {
    return format_range(value, ctx);
  }
  template <typename Context>
    requires detail::family_range<const R>
  FMT_HD auto format(const R &value, Context &ctx) const
      -> decltype(ctx.out()) {
    return format_range(value, ctx);
  }
};

template <typename It, typename Sentinel = It, typename Char = char>
struct join_view {
  It begin;
  Sentinel end;
  basic_string_view<Char> sep;
};

template <typename It, typename Sentinel, typename Char>
FMT_HD constexpr auto join(It first, Sentinel last, basic_string_view<Char> sep)
    -> join_view<It, Sentinel, Char> {
  return {std::move(first), std::move(last), sep};
}
template <typename It, typename Sentinel, typename Char>
FMT_HD constexpr auto join(It first, Sentinel last, const Char *sep)
    -> join_view<It, Sentinel, Char> {
  return {std::move(first), std::move(last), {sep, detail::string_length(sep)}};
}
// The range and separator must outlive formatting. A temporary range is valid
// when the join is consumed in the same full expression, as in print(...).
template <typename R, typename Char>
  requires detail::family_range<std::remove_reference_t<R>>
FMT_HD constexpr auto join(R &&range, const Char *sep) {
  return join(detail::family_begin(range), detail::family_end(range), sep);
}
template <typename R, typename Char>
  requires detail::family_range<std::remove_reference_t<R>>
FMT_HD constexpr auto join(R &&range, basic_string_view<Char> sep) {
  return join(detail::family_begin(range), detail::family_end(range), sep);
}

template <typename It, typename Sentinel, typename Char>
struct formatter<join_view<It, Sentinel, Char>, Char> {
  using view = join_view<It, Sentinel, Char>;
  using ffmt_family_type = view;
  static constexpr bool ffmt_family_repeatable =
      std::forward_iterator<It> &&
      detail::family_repeatable<
          std::remove_cvref_t<decltype(*std::declval<It>())>, Char>;
  detail::formatter_adapter<std::remove_cvref_t<decltype(*std::declval<It>())>,
                            Char>
      element_;
  FMT_HD constexpr auto parse(basic_format_parse_context<Char> &ctx)
      -> const Char * {
    return element_.parse(ctx);
  }

private:
  template <typename View, typename Context>
  FMT_HD constexpr auto format_impl(View &value, Context &ctx) const
      -> decltype(ctx.out()) {
    using Cursor =
        std::conditional_t<std::is_copy_constructible_v<It>, It, It &>;
    Cursor it = value.begin;
    bool first = true;
    for (; it != value.end; ++it) {
      if (!first)
        detail::family_text(ctx, value.sep);
      first = false;
      ctx.advance_to(element_.format(*it, ctx));
    }
    return ctx.out();
  }

public:
  template <typename Context>
  FMT_HD constexpr auto format(view &value, Context &ctx) const
      -> decltype(ctx.out()) {
    return format_impl(value, ctx);
  }
  template <typename Context>
    requires std::is_copy_constructible_v<It>
  FMT_HD constexpr auto format(const view &value, Context &ctx) const
      -> decltype(ctx.out()) {
    return format_impl(value, ctx);
  }
};

template <typename Tuple, typename Char = char> struct tuple_join_view {
  const Tuple &tuple;
  basic_string_view<Char> sep;
};
template <typename Tuple, typename Char>
  requires(detail::family_tuple<Tuple> && !detail::family_range<Tuple>)
FMT_HD constexpr auto join(const Tuple &tuple, const Char *sep)
    -> tuple_join_view<Tuple, Char> {
  return {tuple, {sep, detail::string_length(sep)}};
}
template <typename Tuple, typename Char>
struct formatter<tuple_join_view<Tuple, Char>, Char> {
  using ffmt_family_type = tuple_join_view<Tuple, Char>;
  static constexpr bool ffmt_family_repeatable =
      detail::family_repeatable<Tuple, Char>;

private:
  template <std::size_t... I>
  static auto types(std::index_sequence<I...>)
      -> std::tuple<detail::formatter_adapter<
          std::remove_cvref_t<
              decltype(detail::family_get<I>(std::declval<Tuple>()))>,
          Char>...>;
  decltype(types(
      std::make_index_sequence<std::tuple_size_v<Tuple>>{})) elements_;
  template <std::size_t... I>
  FMT_HD constexpr auto parse_impl(basic_format_parse_context<Char> &ctx,
                                   std::index_sequence<I...>) -> const Char * {
    auto first = ctx.begin(), last = first;
    (
        [&] {
          ctx.advance_to(first);
          last = std::get<I>(elements_).parse(ctx);
        }(),
        ...);
    ctx.advance_to(last);
    return last;
  }
  template <typename Context, std::size_t... I>
  FMT_HD constexpr void body(const tuple_join_view<Tuple, Char> &value,
                             Context &ctx, std::index_sequence<I...>) const {
    (
        [&] {
          if constexpr (I)
            detail::family_text(ctx, value.sep);
          ctx.advance_to(std::get<I>(elements_).format(
              detail::family_get<I>(value.tuple), ctx));
        }(),
        ...);
  }

public:
  FMT_HD constexpr auto parse(basic_format_parse_context<Char> &ctx)
      -> const Char * {
    return parse_impl(ctx,
                      std::make_index_sequence<std::tuple_size_v<Tuple>>{});
  }
  template <typename Context>
  FMT_HD constexpr auto format(const tuple_join_view<Tuple, Char> &value,
                               Context &ctx) const -> decltype(ctx.out()) {
    body(value, ctx, std::make_index_sequence<std::tuple_size_v<Tuple>>{});
    return ctx.out();
  }
};
} // namespace ffmt
