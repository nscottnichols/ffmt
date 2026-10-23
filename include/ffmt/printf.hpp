// Checked, typed printf-style formatting. No varargs, allocation, or %n.
// Conversion, length, and precision semantics follow the independent
// fmt::sprintf oracle. See LICENSES/fmt.txt for {fmt}'s license and
// attribution.
#pragma once
#include <ffmt/detail/family.hpp>

namespace ffmt::detail {
enum class family_printf_length { none, hh, h, l, ll, j, z, t, capital_l };

template <typename Context, typename T>
FMT_HD void
family_printf_integer(Context &ctx, T value,
                      basic_format_spec<typename Context::char_type> spec) {
  using Char = typename Context::char_type;
  using U = unsigned_t<T>;
  bool negative = false;
  U magnitude = static_cast<U>(value);
  if constexpr (signed_integer<T>)
    if (value < 0) {
      negative = true;
      magnitude = U(0) - magnitude;
    }
  unsigned base = spec.type == 'o'                       ? 8
                  : spec.type == 'x' || spec.type == 'X' ? 16
                                                         : 10;
  char digits[sizeof(T) * CHAR_BIT + 1];
  unsigned count = 0;
  do {
    unsigned digit = static_cast<unsigned>(magnitude % base);
    digits[count++] = char(
        digit < 10 ? '0' + digit : (spec.type == 'X' ? 'A' : 'a') + digit - 10);
    magnitude /= base;
  } while (magnitude);
  bool zero = count == 1 && digits[0] == '0';
  const char *prefix = "";
  unsigned prefix_size = 0;
  if (spec.alt && !zero) {
    if (base == 16) {
      prefix = spec.type == 'X' ? "0X" : "0x";
      prefix_size = 2;
    } else if (base == 8) {
      prefix = "0";
      prefix_size = 1;
    }
  }
  if (zero && spec.precision == 0) {
    count = 0;
    if (spec.alt && base == 8) {
      digits[count++] = '0';
    }
  }
  auto precision_count = count + (base == 8 ? prefix_size : 0);
  std::size_t zeros =
      spec.precision > 0 &&
              static_cast<unsigned>(spec.precision) > precision_count
          ? static_cast<std::size_t>(spec.precision) - precision_count
          : 0;
  if (spec.precision >= 0)
    spec.zero_pad = false;
  char sign = negative           ? '-'
              : spec.sign == '+' ? '+'
              : spec.sign == ' ' ? ' '
                                 : 0;
  numeric_padded(ctx, spec, count + zeros, sign, prefix, prefix_size, true,
                 [&](auto &bodyctx) {
                   repeat(bodyctx, Char('0'), zeros);
                   for (unsigned i = count; i; --i)
                     family_put(bodyctx, Char(digits[i - 1]));
                 });
}

template <typename Target, typename Source, typename Context>
FMT_HD void
family_printf_convert(Context &ctx, Source value,
                      basic_format_spec<typename Context::char_type> spec,
                      bool signed_conversion) {
  if constexpr (sizeof(Target) <= sizeof(int)) {
    if (signed_conversion)
      family_printf_integer(ctx, static_cast<int>(static_cast<Target>(value)),
                            spec);
    else
      family_printf_integer(
          ctx, static_cast<unsigned>(static_cast<unsigned_t<Target>>(value)),
          spec);
  } else {
    if (signed_conversion)
      family_printf_integer(ctx, static_cast<long long>(value), spec);
    else
      family_printf_integer(ctx, static_cast<unsigned_t<Source>>(value), spec);
  }
}

template <typename Context, typename T>
FMT_HD void
family_printf_value(Context &ctx, const T &value,
                    basic_format_spec<typename Context::char_type> spec,
                    family_printf_length length) {
  using Char = typename Context::char_type;
  using U = std::remove_cvref_t<T>;
  if constexpr (has_format_as<U>) {
    family_printf_value(ctx, mapped_value(value), spec, length);
  } else if constexpr (integer<U>) {
    if constexpr (sizeof(U) > sizeof(std::uint64_t)) {
      ctx.on_error(format_error::unsupported_type);
      return;
    }
    if (spec.type == 'c') {
      spec.sign = 0;
      spec.alt = spec.zero_pad = false;
      spec.precision = -1;
      Char c = static_cast<Char>(value);
      write_string(ctx, basic_string_view<Char>{&c, 1}, spec, true);
      return;
    }
    bool signed_conversion = spec.type == 'd' || spec.type == 'i';
    if (!(signed_conversion || spec.type == 'u' || spec.type == 'o' ||
          spec.type == 'x' || spec.type == 'X')) {
      ctx.on_error(format_error::incompatible_type);
      return;
    }
    if (!signed_conversion)
      spec.sign = 0;
    if (spec.type == 'i' || spec.type == 'u')
      spec.type = 'd';
    if (spec.zero_pad && spec.align != '<' && spec.precision < 0)
      spec.align = 0;
    else
      spec.zero_pad = false;
    // fmt's printf argument store applies integer promotions before conversion.
    using Promoted = std::conditional_t<
        (sizeof(U) < sizeof(int)) || std::is_same_v<U, bool>,
        std::conditional_t<std::is_unsigned_v<U> && !std::is_same_v<U, bool>,
                           unsigned, int>,
        U>;
    auto promoted = static_cast<Promoted>(value);
    switch (length) {
    case family_printf_length::hh:
      family_printf_convert<signed char>(ctx, promoted, spec,
                                         signed_conversion);
      break;
    case family_printf_length::h:
      family_printf_convert<short>(ctx, promoted, spec, signed_conversion);
      break;
    case family_printf_length::l:
      family_printf_convert<long>(ctx, promoted, spec, signed_conversion);
      break;
    case family_printf_length::ll:
      family_printf_convert<long long>(ctx, promoted, spec, signed_conversion);
      break;
    case family_printf_length::j:
      family_printf_convert<std::intmax_t>(ctx, promoted, spec,
                                           signed_conversion);
      break;
    case family_printf_length::z:
      family_printf_convert<std::size_t>(ctx, promoted, spec,
                                         signed_conversion);
      break;
    case family_printf_length::t:
      family_printf_convert<std::ptrdiff_t>(ctx, promoted, spec,
                                            signed_conversion);
      break;
    default:
      family_printf_convert<Promoted>(ctx, promoted, spec, signed_conversion);
      break;
    }
  } else if constexpr (std::is_floating_point_v<U>) {
    if (!(spec.type == 'f' || spec.type == 'F' || spec.type == 'e' ||
          spec.type == 'E' || spec.type == 'g' || spec.type == 'G' ||
          spec.type == 'a' || spec.type == 'A')) {
      ctx.on_error(format_error::incompatible_type);
      return;
    }
    if (spec.zero_pad && spec.align != '<')
      spec.align = 0;
    else
      spec.zero_pad = false;
    // C varargs promote float to double; fmt::sprintf follows that convention.
    if constexpr (std::is_same_v<U, float>)
      write_float(ctx, widen_float(value), spec);
    else
      write_float(ctx, value, spec);
  } else if constexpr (string_like<U, Char> || char_pointer<U, Char>) {
    constexpr bool cstring = std::is_pointer_v<U> || std::is_array_v<U>;
    if (spec.type == 'p') {
      if constexpr (cstring) {
        auto pointer = static_cast<const Char *>(value);
        if (pointer) {
          spec.zero_pad = false;
          spec.precision = -1;
          write_value(ctx, static_cast<const void *>(pointer), spec);
        } else {
          spec.type = 0;
          spec.sign = 0;
          spec.alt = spec.zero_pad = false;
          spec.precision = -1;
          write_padded(
              ctx, spec, 5, [&](auto &b) { family_ascii(b, "(nil)"); }, '>');
        }
      } else
        ctx.on_error(format_error::incompatible_type);
      return;
    }
    if (spec.type != 's') {
      ctx.on_error(format_error::incompatible_type);
      return;
    }
    spec.sign = 0;
    spec.alt = spec.zero_pad = false;
    if constexpr (cstring) {
      auto pointer = static_cast<const Char *>(value);
      if (!pointer) {
        spec.precision = -1;
        write_padded(
            ctx, spec, 6, [&](auto &b) { family_ascii(b, "(null)"); }, '>');
      } else if (spec.precision >= 0) {
        // Bound the scan itself: %.Ns may format an unterminated byte array.
        std::size_t size = 0;
        while (size < static_cast<std::size_t>(spec.precision) &&
               pointer[size] != Char())
          ++size;
        write_string(ctx, basic_string_view<Char>(pointer, size), spec);
      } else
        write_string(ctx,
                     basic_string_view<Char>(pointer, string_length(pointer)),
                     spec);
    } else
      write_string(ctx, basic_string_view<Char>(value), spec);
  } else if constexpr (std::is_pointer_v<U> || std::is_null_pointer_v<U>) {
    if (spec.type != 'p') {
      ctx.on_error(format_error::incompatible_type);
      return;
    }
    spec.sign = 0;
    spec.alt = spec.zero_pad = false;
    spec.precision = -1;
    if (value)
      write_value(ctx, value, spec);
    else {
      spec.type = 0;
      write_padded(
          ctx, spec, 5, [&](auto &b) { family_ascii(b, "(nil)"); }, '>');
    }
  } else
    ctx.on_error(format_error::unsupported_type);
}

template <typename Sink, typename Char, typename... Args>
FMT_HD void family_run_printf(Sink &sink, basic_string_view<Char> pattern,
                              const Args &...args) {
  auto values = std::tuple<const Args &...>{args...};
  basic_format_context<sink_iterator<Sink>, Char, decltype(values)> ctx{
      {&sink}, sink.error, &values};
  auto it = pattern.data(), end = it ? it + pattern.size() : it;
  index_state indices;
  basic_format_parse_context<Char> parsectx{pattern, &indices};
  auto number = [&] { return parse_number(it, end, parsectx); };
  auto index = [&](int positional) {
    if (positional >= 0) {
      if (!positional)
        ctx.on_error(format_error::argument_out_of_range);
      parsectx.check_arg_id(positional - 1);
      return positional - 1;
    }
    return parsectx.next_arg_id();
  };
  auto positional = [&] {
    if (it != end && is_digit(*it)) {
      auto result = number();
      if (it == end || *it != Char('$'))
        ctx.on_error(format_error::invalid_format);
      else
        ++it;
      return result;
    }
    return -1;
  };
  auto dynamic = [&](basic_format_spec<Char> &spec, bool precision) {
    ++it;
    int argid = index(positional());
    bool found = visit_index(values, argid, [&](const auto &arg) {
      using T = std::remove_cvref_t<decltype(arg)>;
      if constexpr (integer<T>) {
        bool negative = false;
        auto magnitude = static_cast<unsigned_t<T>>(arg);
        if constexpr (signed_integer<T>)
          if (arg < 0) {
            negative = true;
            magnitude = unsigned_t<T>(0) - magnitude;
          }
        if (magnitude > static_cast<unsigned_t<T>>(INT_MAX)) {
          // Negative precision must still fit int, including INT_MIN.
          if (!(precision && negative &&
                magnitude == static_cast<unsigned_t<T>>(INT_MAX) + 1u))
            ctx.on_error(format_error::invalid_dynamic_spec);
        }
        if (precision)
          spec.precision = negative ? -1 : static_cast<int>(magnitude);
        else {
          if (negative)
            spec.align = '<';
          spec.width = static_cast<int>(magnitude);
        }
      } else
        ctx.on_error(format_error::invalid_dynamic_spec);
    });
    if (!found)
      ctx.on_error(format_error::argument_out_of_range);
  };
  while (it != end && ctx.error() == format_error::none &&
         parsectx.error() == format_error::none) {
    if (*it != Char('%')) {
      family_put(ctx, *it++);
      continue;
    }
    ++it;
    if (it != end && *it == Char('%')) {
      family_put(ctx, *it++);
      continue;
    }
    if (it == end) {
      ctx.on_error(format_error::invalid_format);
      break;
    }
    int argpos = -1;
    if (is_digit(*it)) {
      auto start = it;
      int n = number();
      if (it != end && *it == Char('$')) {
        argpos = n;
        ++it;
      } else
        it = start;
    }
    basic_format_spec<Char> spec;
    spec.align = '>';
    bool more = true;
    while (it != end && more) {
      switch (static_cast<char>(*it)) {
      case '-':
        spec.align = '<';
        break;
      case '+':
        spec.sign = '+';
        break;
      case ' ':
        if (spec.sign != '+')
          spec.sign = ' ';
        break;
      case '0':
        spec.zero_pad = true;
        break;
      case '#':
        spec.alt = true;
        break;
      default:
        more = false;
        continue;
      }
      ++it;
    }
    if (it != end && *it == Char('*'))
      dynamic(spec, false);
    else if (it != end && is_digit(*it))
      spec.width = number();
    if (it != end && *it == Char('.')) {
      ++it;
      if (it != end && *it == Char('*'))
        dynamic(spec, true);
      else
        spec.precision = number();
    }
    auto length = family_printf_length::none;
    if (it != end)
      switch (static_cast<char>(*it)) {
      case 'h':
        ++it;
        length = family_printf_length::h;
        if (it != end && *it == Char('h')) {
          ++it;
          length = family_printf_length::hh;
        }
        break;
      case 'l':
        ++it;
        length = family_printf_length::l;
        if (it != end && *it == Char('l')) {
          ++it;
          length = family_printf_length::ll;
        }
        break;
      case 'j':
        ++it;
        length = family_printf_length::j;
        break;
      case 'z':
        ++it;
        length = family_printf_length::z;
        break;
      case 't':
        ++it;
        length = family_printf_length::t;
        break;
      case 'L':
        ++it;
        length = family_printf_length::capital_l;
        break;
      default:
        break;
      }
    if (it == end) {
      ctx.on_error(format_error::invalid_format);
      break;
    }
    spec.type = static_cast<char>(*it++);
    if (spec.type == 'n') {
      ctx.on_error(format_error::invalid_format);
      break;
    }
    int argid = index(argpos);
    if (ctx.error() != format_error::none ||
        parsectx.error() != format_error::none)
      break;
    bool found = visit_index(values, argid, [&](const auto &value) {
      family_printf_value(ctx, value, spec, length);
    });
    if (!found)
      ctx.on_error(format_error::argument_out_of_range);
  }
  ctx.on_error(parsectx.error());
}
} // namespace ffmt::detail

namespace ffmt {
template <typename OutputIt, typename Char, typename... Args>
FMT_HD auto printf_to_n(OutputIt out, std::size_t size,
                        runtime_format_string<Char> pattern,
                        const Args &...args) -> format_to_result<OutputIt> {
  detail::iterator_sink<OutputIt, Char> sink{out, size};
  detail::family_run_printf(sink, pattern.str, args...);
  return {{sink.written, sink.required, sink.error}, sink.out};
}
template <typename OutputIt, typename Char, typename... Args>
FMT_HD auto printf_to_n(OutputIt out, std::size_t size,
                        basic_string_view<Char> pattern, const Args &...args)
    -> format_to_result<OutputIt> {
  return printf_to_n(out, size, runtime(pattern), args...);
}
template <typename OutputIt, typename Char, typename... Args>
FMT_HD auto printf_to_n(OutputIt out, std::size_t size, const Char *pattern,
                        const Args &...args) -> format_to_result<OutputIt> {
  return printf_to_n(out, size, runtime(pattern), args...);
}
template <typename OutputIt, typename Char, typename... Args>
FMT_HD auto printf_to(OutputIt out, runtime_format_string<Char> pattern,
                      const Args &...args) -> format_to_result<OutputIt> {
  return printf_to_n(out, (std::numeric_limits<std::size_t>::max)(), pattern,
                     args...);
}
template <typename OutputIt, typename Char, typename... Args>
FMT_HD auto printf_to(OutputIt out, const Char *pattern, const Args &...args)
    -> format_to_result<OutputIt> {
  return printf_to(out, runtime(pattern), args...);
}
template <typename OutputIt, typename Char, typename... Args>
FMT_HD auto printf_to(OutputIt out, basic_string_view<Char> pattern,
                      const Args &...args) -> format_to_result<OutputIt> {
  return printf_to(out, runtime(pattern), args...);
}
template <typename Char, typename... Args>
FMT_HD auto try_printf(runtime_format_string<Char> pattern, const Args &...args)
    -> format_result {
  detail::stdout_sink<Char> sink;
  detail::family_run_printf(sink, pattern.str, args...);
  sink.finish();
  return {sink.written, sink.required, sink.error};
}
template <typename Char, typename... Args>
FMT_HD auto try_printf(const Char *pattern, const Args &...args)
    -> format_result {
  return try_printf(runtime(pattern), args...);
}
template <typename Char, typename... Args>
FMT_HD auto try_printf(basic_string_view<Char> pattern, const Args &...args)
    -> format_result {
  return try_printf(runtime(pattern), args...);
}
} // namespace ffmt
