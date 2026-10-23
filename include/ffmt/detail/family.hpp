// Shared allocation-free support for optional formatting families.
//
// Portions of the family format semantics and civil-date algorithms are adapted
// from {fmt}. Its copyright and permission notice follows:
//
// Copyright (c) 2012 - present, Victor Zverovich and {fmt} contributors
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.
#pragma once
#include <climits>
#include <ffmt/base.hpp>
#include <iterator>

namespace ffmt::detail {

template <typename Context>
FMT_HD constexpr void family_put(Context &ctx, typename Context::char_type c) {
  auto out = ctx.out();
  *out++ = c;
  ctx.advance_to(out);
}

template <typename Context>
FMT_HD constexpr void family_ascii(Context &ctx, const char *text) {
  while (*text)
    family_put(ctx, static_cast<typename Context::char_type>(*text++));
}

template <typename Context, typename Char>
FMT_HD constexpr void family_text(Context &ctx, basic_string_view<Char> text) {
  for (auto c : text)
    family_put(ctx, c);
}

// Counts display columns while consuming a stream of code units. No output or
// temporary formatted string is needed to align a nested range or tuple.
template <typename Char> struct family_counter {
  std::size_t columns = 0;
  unsigned remaining = 0;
  unsigned seen = 0, total = 0;
  std::uint32_t point = 0;
  FMT_HD constexpr void push(Char c) {
    if constexpr (sizeof(Char) == 1) {
#if !FFMT_UNICODE
      (void)c;
      ++columns;
      return;
#endif
      auto b = static_cast<unsigned char>(c);
      if (remaining && (b & 0xc0) == 0x80) {
        point = (point << 6) | (b & 0x3f);
        ++seen;
        if (!--remaining) {
          bool valid = point >= (total == 2   ? 0x80u
                                 : total == 3 ? 0x800u
                                              : 0x10000u) &&
                       point <= 0x10ffff &&
                       !(point >= 0xd800 && point <= 0xdfff);
          columns += valid ? display_width_of(point) : seen;
          seen = 0;
        }
      } else {
        // Invalid UTF-8 bytes each occupy one column, as they do in fmt.
        if (remaining) {
          columns += seen;
          remaining = 0;
          seen = 0;
        }
        if (b < 0x80)
          ++columns;
        else if (b >= 0xc2 && b <= 0xf4) {
          remaining = b < 0xe0 ? 1 : b < 0xf0 ? 2 : 3;
          total = remaining + 1;
          seen = 1;
          point = b & (remaining == 1 ? 0x1f : remaining == 2 ? 0x0f : 0x07);
        } else
          ++columns;
      }
    } else {
      (void)c;
      ++columns; // Wide fmt padding measures code units.
    }
  }
  FMT_HD constexpr std::size_t size() const { return columns + seen; }
  FMT_HD constexpr void repeat(Char c, std::size_t n) {
    if (!n)
      return;
    if constexpr (sizeof(Char) != 1) {
      (void)c;
      columns += n;
    } else {
      auto byte = static_cast<unsigned char>(c);
      if (byte >= 0xc2 && byte <= 0xf4) {
        columns += seen + n - 1;
        remaining = seen = 0;
        push(c);
      } else {
        // A pending sequence consumes at most three repeated continuation
        // bytes.
        while (remaining && n) {
          push(c);
          --n;
        }
        columns += n;
      }
    }
  }
};

template <typename Char> struct family_count_iterator {
  using iterator_category = std::output_iterator_tag;
  using value_type = void;
  using difference_type = std::ptrdiff_t;
  using pointer = void;
  using reference = void;
  family_counter<Char> *counter;
  FMT_HD constexpr auto operator*() const -> family_count_iterator {
    return *this;
  }
  FMT_HD constexpr auto operator++() -> family_count_iterator & {
    return *this;
  }
  FMT_HD constexpr auto operator++(int) -> family_count_iterator {
    return *this;
  }
  FMT_HD constexpr auto operator=(Char c) -> family_count_iterator & {
    counter->push(c);
    return *this;
  }
  FMT_HD constexpr void repeat(Char c, std::size_t n) { counter->repeat(c, n); }
};

template <typename OutputIt, typename Char> struct family_forward_count_state {
  OutputIt out;
  family_counter<Char> counter;
};
template <typename OutputIt, typename Char>
struct family_forward_count_iterator {
  using iterator_category = std::output_iterator_tag;
  using value_type = void;
  using difference_type = std::ptrdiff_t;
  using pointer = void;
  using reference = void;
  family_forward_count_state<OutputIt, Char> *state;
  FMT_HD constexpr auto operator*() const -> family_forward_count_iterator {
    return *this;
  }
  FMT_HD constexpr auto operator++() -> family_forward_count_iterator & {
    return *this;
  }
  FMT_HD constexpr auto operator++(int) -> family_forward_count_iterator {
    return *this;
  }
  FMT_HD constexpr auto operator=(Char c) -> family_forward_count_iterator & {
    *state->out++ = c;
    state->counter.push(c);
    return *this;
  }
  FMT_HD constexpr void repeat(Char c, std::size_t n) {
    if constexpr (requires { state->out.repeat(c, n); })
      state->out.repeat(c, n);
    else
      for (std::size_t i = 0; i < n; ++i)
        *state->out++ = c;
    state->counter.repeat(c, n);
  }
  FMT_HD constexpr auto ffmt_base_out() -> OutputIt & { return state->out; }
};

template <typename OutputIt> consteval bool family_can_reorder() {
  if constexpr (requires(OutputIt &it) { it.ffmt_padding_mark(); })
    return true;
  else if constexpr (requires(OutputIt &it) { it.ffmt_base_out(); })
    return family_can_reorder<std::remove_cvref_t<
        decltype(std::declval<OutputIt &>().ffmt_base_out())>>();
  else
    return false;
}
template <typename OutputIt>
FMT_HD constexpr decltype(auto) family_padding_output(OutputIt &out) {
  if constexpr (requires { out.ffmt_padding_mark(); })
    return (out);
  else
    return family_padding_output(out.ffmt_base_out());
}

// Unknown custom formatters can mutate referenced values even through a const
// format method. Only our own matching formatter markers establish replay.
template <typename T, typename Char> consteval bool family_repeatable_type() {
  using U = std::remove_cvref_t<T>;
  using F = formatter<U, Char>;
  if constexpr (std::is_void_v<U>)
    return true;
  else if constexpr (requires {
                       typename F::ffmt_family_type;
                       F::ffmt_family_repeatable;
                     })
    return std::is_same_v<typename F::ffmt_family_type, U> &&
           F::ffmt_family_repeatable;
  else if constexpr (requires { typename F::ffmt_primary_type; })
    return std::is_same_v<typename F::ffmt_primary_type, U> &&
           primitive<U, Char> && !has_format_as<U>;
  else
    return false;
}
template <typename T, typename Char>
inline constexpr bool family_repeatable = family_repeatable_type<T, Char>();

template <typename Char> FMT_HD constexpr bool family_align(Char c) {
  return c == Char('<') || c == Char('>') || c == Char('^');
}

// Family outer specs accept only fill/alignment and width. A leading ':' is
// the nested element separator, never a fill character.
template <typename Char>
FMT_HD constexpr auto family_outer_specs(basic_format_parse_context<Char> &ctx,
                                         basic_format_spec<Char> &spec)
    -> const Char * {
  auto it = ctx.begin();
  auto end = ctx.end();
  if (it == end || *it == Char('}') || *it == Char(':'))
    return it;
  unsigned fill_units = 1;
  if constexpr (sizeof(Char) == 1) {
    auto b = static_cast<unsigned char>(*it);
    if (b >= 0xc2)
      fill_units = b < 0xe0 ? 2 : b < 0xf0 ? 3 : 4;
  }
  if (static_cast<std::size_t>(end - it) > fill_units &&
      family_align(it[fill_units])) {
    spec.fill_size = fill_units;
    for (unsigned i = 0; i < fill_units; ++i)
      spec.fill[i] = it[i];
    spec.align = static_cast<char>(it[fill_units]);
    it += fill_units + 1;
  } else if (family_align(*it))
    spec.align = static_cast<char>(*it++);
  if (it != end && *it >= Char('0') && *it <= Char('9')) {
    int width = 0;
    while (it != end && *it >= Char('0') && *it <= Char('9')) {
      int digit = int(*it++ - Char('0'));
      if (width > (INT_MAX - digit) / 10) {
        ctx.on_error(format_error::invalid_format);
        return it;
      }
      width = width * 10 + digit;
    }
    spec.width = width;
  } else if (it != end && *it == Char('{'))
    parse_dynamic(it, end, ctx, spec.width_arg, spec.width_name);
  ctx.advance_to(it);
  return it;
}

template <typename Context, typename Char, typename Body>
FMT_HD constexpr auto family_padded(Context &ctx, basic_format_spec<Char> spec,
                                    Body body, bool replayable = true)
    -> decltype(ctx.out()) {
  spec = resolved(ctx, spec);
  if (ctx.error() != format_error::none)
    return ctx.out();
  spec.width_arg = spec.precision_arg = -1;
  spec.width_name = spec.precision_name = {};
  if (!spec.width) {
    body(ctx);
    return ctx.out();
  }
  auto out = ctx.out();
  using OutputIt = decltype(out);
  // Render while measuring columns. This path never invokes a formatter twice.
  auto render_and_measure = [&] {
    family_forward_count_state<OutputIt, Char> state{ctx.out(), {}};
    auto counting =
        ctx.rebind(family_forward_count_iterator<OutputIt, Char>{&state});
    body(counting);
    ctx.advance_to(state.out);
    return state.counter.size();
  };
  // Left padding needs only the completed body width, so every iterator can
  // stream the body once and append its trailing padding.
  if (!spec.align || spec.align == '<') {
    auto columns = render_and_measure();
    write_padded(ctx, spec, columns, [](auto &) {});
    return ctx.out();
  }
  // A bounded writable buffer can insert leading padding after rendering. Its
  // accounting retains the full required size even when the stored prefix ends.
  if constexpr (family_can_reorder<OutputIt>()) {
    auto &output = family_padding_output(out);
    auto mark = output.ffmt_padding_mark();
    auto columns = render_and_measure();
    auto body_end = output.ffmt_padding_mark();
    write_padded(ctx, spec, columns, [](auto &) {});
    std::size_t padding = static_cast<std::size_t>(spec.width) > columns
                              ? static_cast<std::size_t>(spec.width) - columns
                              : 0;
    output.ffmt_apply_padding(mark, body_end.required - mark.required,
                              spec.align == '^' ? padding / 2 : padding,
                              spec.fill, spec.fill_size);
    return ctx.out();
  } else {
    // Other iterators cannot insert bytes before the body. Measure, then replay
    // only when the formatter and input explicitly permit a second traversal.
    if (!replayable) {
      ctx.on_error(format_error::unsupported_type);
      return ctx.out();
    }
    family_counter<Char> count;
    auto counter_ctx = ctx.rebind(family_count_iterator<Char>{&count});
    body(counter_ctx);
    write_padded(ctx, spec, count.size(), body);
    return ctx.out();
  }
}

template <typename Formatter>
FMT_HD constexpr void family_debug(Formatter &fmt, bool enabled = true) {
  if constexpr (requires { fmt.set_debug_format(enabled); })
    fmt.set_debug_format(enabled);
}

template <typename T, typename Char, typename Context>
FMT_HD constexpr void family_default(Context &ctx, const T &value,
                                     bool debug = true) {
  formatter_adapter<std::remove_cvref_t<T>, Char> fmt;
  family_debug(fmt, debug);
  basic_format_parse_context<Char> pc{basic_string_view<Char>{}};
  fmt.parse(pc);
  if (pc.error() != format_error{})
    ctx.on_error(pc.error());
  ctx.advance_to(fmt.format(value, ctx));
}

} // namespace ffmt::detail
