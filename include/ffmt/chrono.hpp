// Locale-independent chrono/calendar formatting, without clocks, strftime,
// timezone databases, or allocations. Grammar follows {fmt}/chrono.h;
// civil-date arithmetic is adapted from its Howard Hinnant algorithms. See
// LICENSES/fmt.txt for attribution and licensing.
#pragma once
#include <chrono>
#include <cmath>
#include <ctime>
#include <ffmt/detail/family.hpp>
#include <limits>

namespace ffmt::detail {

FMT_HD constexpr std::int64_t
family_days_from_civil(std::int64_t year, unsigned month, unsigned day) {
  year -= month <= 2;
  auto era = (year >= 0 ? year : year - 399) / 400;
  auto yoe = static_cast<unsigned>(year - era * 400);
  auto doy = (153 * (month > 2 ? month - 3 : month + 9) + 2) / 5 + day - 1;
  auto doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
  return era * 146097 + static_cast<std::int64_t>(doe) - 719468;
}
struct family_calendar {
  std::int64_t year = 1900;
  int month = 1, day = 0, hour = 0, minute = 0, second = 0, weekday = 0,
      yearday = 0;
  std::int64_t utc_offset = 0;
  const char *zone = "";
  std::uint64_t fraction = 0;
  int fraction_digits = 0;
};
FMT_HD constexpr family_calendar family_civil_from_days(std::int64_t days) {
  family_calendar result;
  auto z = days + 719468;
  auto era = (z >= 0 ? z : z - 146096) / 146097;
  auto doe = static_cast<unsigned>(z - era * 146097);
  auto yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
  auto year = static_cast<std::int64_t>(yoe) + era * 400;
  auto doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
  auto mp = (5 * doy + 2) / 153;
  result.day = static_cast<int>(doy - (153 * mp + 2) / 5 + 1);
  result.month = static_cast<int>(mp < 10 ? mp + 3 : mp - 9);
  result.year = year + (result.month <= 2);
  result.weekday = static_cast<int>((days % 7 + 11) % 7);
  result.yearday =
      static_cast<int>(days - family_days_from_civil(result.year, 1, 1));
  return result;
}

template <typename Context, typename Integer>
FMT_HD constexpr void family_chrono_number(Context &ctx, Integer value,
                                           int width = 0, char pad = '0') {
  basic_format_spec<typename Context::char_type> spec;
  spec.width = width;
  spec.align = '>';
  spec.zero_pad = pad == '0';
  spec.fill[0] = static_cast<typename Context::char_type>(pad);
  if (pad == '-') {
    spec.width = 0;
    spec.zero_pad = false;
  }
  write_value(ctx, value, spec);
}

template <typename Char> struct family_chrono_spec {
  basic_format_spec<Char> outer;
  basic_string_view<Char> pattern;
  FMT_HD constexpr auto parse(basic_format_parse_context<Char> &ctx,
                              bool duration, bool floating = false)
      -> const Char * {
    auto it = family_outer_specs(ctx, outer);
    if (duration && it != ctx.end() && *it == Char('.')) {
      ++it;
      if (it != ctx.end() && *it == Char('{')) {
        parse_dynamic(it, ctx.end(), ctx, outer.precision_arg,
                      outer.precision_name);
      } else {
        int precision = 0;
        if (it == ctx.end() || *it < Char('0') || *it > Char('9'))
          ctx.on_error(format_error::invalid_format);
        while (it != ctx.end() && *it >= Char('0') && *it <= Char('9')) {
          int digit = int(*it++ - Char('0'));
          if (precision > (INT_MAX - digit) / 10) {
            ctx.on_error(format_error::invalid_format);
            return it;
          }
          precision = precision * 10 + digit;
        }
        outer.precision = precision;
      }
    }
    if (it != ctx.end() && *it == Char('L'))
      ctx.on_error(format_error::unsupported_type);
    auto begin = it;
    if (it != ctx.end() && *it != Char('}') && *it != Char('%'))
      ctx.on_error(format_error::invalid_format);
    for (; it != ctx.end() && *it != Char('}'); ++it) {
      if (*it != Char('%'))
        continue;
      ++it;
      if (it != ctx.end() && (*it == Char('_') || *it == Char('-')))
        ++it;
      char modifier = 0;
      if (it != ctx.end() && (*it == Char('E') || *it == Char('O')))
        modifier = static_cast<char>(*it++);
      if (it == ctx.end() || *it == Char('}')) {
        ctx.on_error(format_error::invalid_format);
        break;
      }
      const char *valid = duration ? "%ntHIMSrpRjTQq"
                                   : "%ntYCyGgVbBhmaAuwd eHIMSrpRDFTxXcUWjzZ";
      bool found = false;
      for (auto p = valid; *p; ++p)
        if (*it == Char(*p))
          found = true;
      if (modifier) {
        auto allowed = modifier == 'E' ? "YyCcxXz" : "ymUWVdew uHIMS z";
        bool alternative = false;
        for (auto p = allowed; *p; ++p)
          if (*it == Char(*p))
            alternative = true;
        found = found && alternative;
      }
      if (!found || *it == Char(' '))
        ctx.on_error(format_error::invalid_format);
      if (duration && !floating && *it == Char('Q') &&
          (outer.precision >= 0 || outer.precision_arg >= 0 ||
           !outer.precision_name.empty()))
        ctx.on_error(format_error::invalid_format);
    }
    pattern = {begin, static_cast<std::size_t>(it - begin)};
    ctx.advance_to(it);
    return it;
  }
};

FMT_HD constexpr int family_iso_weeks(std::int64_t year) {
  auto jan1 = (family_days_from_civil(year, 1, 1) % 7 + 11) % 7;
  bool leap = year % 4 == 0 && (year % 100 != 0 || year % 400 == 0);
  return 52 + (jan1 == 4 || (jan1 == 3 && leap));
}

template <typename Context>
FMT_HD constexpr void
family_calendar_atom(Context &ctx, const family_calendar &c, char token,
                     char pad = '0', bool alt = false, bool fraction = true) {
  constexpr const char *short_days[] = {"Sun", "Mon", "Tue", "Wed",
                                        "Thu", "Fri", "Sat"};
  constexpr const char *long_days[] = {"Sunday",    "Monday",   "Tuesday",
                                       "Wednesday", "Thursday", "Friday",
                                       "Saturday"};
  constexpr const char *short_months[] = {"Jan", "Feb", "Mar", "Apr",
                                          "May", "Jun", "Jul", "Aug",
                                          "Sep", "Oct", "Nov", "Dec"};
  constexpr const char *long_months[] = {
      "January", "February", "March",     "April",   "May",      "June",
      "July",    "August",   "September", "October", "November", "December"};
  using Char = typename Context::char_type;
  auto put = [&](char ch) { family_put(ctx, Char(ch)); };
  auto number = [&](auto n, int width = 2, char fill = 0) {
    family_chrono_number(ctx, n, width, fill ? fill : pad);
  };
  switch (token) {
  case '%':
    put('%');
    break;
  case 'n':
    put('\n');
    break;
  case 't':
    put('\t');
    break;
  case 'Y':
    number(c.year, 4);
    break;
  case 'C': {
    auto century = c.year / 100;
    if (c.year >= -99 && c.year < 0)
      family_ascii(ctx, "-0");
    else
      number(century, century >= 0 && century < 100 ? 2 : 0, '0');
    break;
  }
  case 'y':
    number(c.year % 100 < 0 ? -(c.year % 100) : c.year % 100, 2, '0');
    break;
  case 'm':
    number(c.month);
    break;
  case 'd':
    number(c.day);
    break;
  case 'e':
    number(c.day, 2, ' ');
    break;
  case 'H':
    number(c.hour);
    break;
  case 'I':
    number(c.hour % 12 ? c.hour % 12 : 12);
    break;
  case 'M':
    number(c.minute);
    break;
  case 'S':
    number(c.second);
    if (fraction && c.fraction_digits) {
      put('.');
      number(c.fraction, c.fraction_digits, '0');
    }
    break;
  case 'p':
    family_ascii(ctx, c.hour < 12 ? "AM" : "PM");
    break;
  case 'a':
  case 'A':
    if (c.weekday < 0 || c.weekday > 6)
      ctx.on_error(format_error::invalid_format);
    else
      family_ascii(ctx,
                   token == 'a' ? short_days[c.weekday] : long_days[c.weekday]);
    break;
  case 'b':
  case 'h':
  case 'B':
    if (c.month < 1 || c.month > 12)
      ctx.on_error(format_error::invalid_format);
    else
      family_ascii(ctx, token == 'B' ? long_months[c.month - 1]
                                     : short_months[c.month - 1]);
    break;
  case 'w':
    number(c.weekday, 1);
    break;
  case 'u':
    number(c.weekday ? c.weekday : 7, 1);
    break;
  case 'j':
    number(c.yearday + 1, 3);
    break;
  case 'U':
    number((c.yearday + 7 - c.weekday) / 7);
    break;
  case 'W':
    number((c.yearday + 7 - (c.weekday + 6) % 7) / 7);
    break;
  case 'G':
  case 'g':
  case 'V': {
    auto iso_year = c.year;
    int week = (c.yearday + 11 - (c.weekday ? c.weekday : 7)) / 7;
    if (week < 1) {
      --iso_year;
      week = family_iso_weeks(iso_year);
    } else if (week > family_iso_weeks(iso_year)) {
      ++iso_year;
      week = 1;
    }
    if (token == 'V')
      number(week);
    else if (token == 'g')
      number(iso_year % 100 < 0 ? -(iso_year % 100) : iso_year % 100, 2, '0');
    else
      number(iso_year, 4, '0');
    break;
  }
  case 'z': {
    auto offset = c.utc_offset;
    put(offset < 0 ? '-' : '+');
    auto magnitude = offset < 0
                         ? std::uint64_t(0) - static_cast<std::uint64_t>(offset)
                         : static_cast<std::uint64_t>(offset);
    number(magnitude / 3600);
    if (alt)
      put(':');
    number((magnitude / 60) % 60);
    break;
  }
  case 'Z':
    if (c.zone)
      family_ascii(ctx, c.zone);
    break;
  default:
    ctx.on_error(format_error::invalid_format);
    break;
  }
}

// Composite C-locale directives expand once into primitive directives. Keeping
// this call graph acyclic is required by SPIR-V and avoids recursive GPU
// frames.
template <typename Context>
FMT_HD constexpr void
family_calendar_token(Context &ctx, const family_calendar &c, char token,
                      char pad = '0', bool alt = false) {
  const char *expansion = nullptr;
  switch (token) {
  case 'F':
    expansion = "%Y-%m-%d";
    break;
  case 'D':
  case 'x':
    expansion = "%m/%d/%y";
    break;
  case 'R':
    expansion = "%H:%M";
    break;
  case 'T':
  case 'X':
    expansion = "%H:%M:%S";
    break;
  case 'r':
    expansion = "%I:%M:%S %p";
    break;
  case 'c':
    expansion = "%a %b %e %H:%M:%S %Y";
    break;
  default:
    family_calendar_atom(ctx, c, token, pad, alt);
    return;
  }
  for (auto it = expansion; *it; ++it) {
    if (*it != '%') {
      family_put(ctx, static_cast<typename Context::char_type>(*it));
      continue;
    }
    char component = *++it;
    family_calendar_atom(ctx, c, component,
                         token == 'c' && component == 'Y' ? ' ' : '0', false,
                         token != 'r');
  }
}

template <typename Context, typename Char, typename Token>
FMT_HD constexpr void family_chrono_pattern(Context &ctx,
                                            basic_string_view<Char> pattern,
                                            Token token) {
  for (std::size_t i = 0; i < pattern.size(); ++i) {
    if (pattern[i] != Char('%')) {
      family_put(ctx, pattern[i]);
      continue;
    }
    if (++i == pattern.size()) {
      ctx.on_error(format_error::invalid_format);
      return;
    }
    char pad = '0';
    bool alt = false;
    if (pattern[i] == Char('-') || pattern[i] == Char('_')) {
      pad = pattern[i] == Char('_') ? ' ' : static_cast<char>(pattern[i]);
      ++i;
    }
    if (i != pattern.size() &&
        (pattern[i] == Char('E') || pattern[i] == Char('O'))) {
      alt = true;
      ++i;
    }
    if (i == pattern.size()) {
      ctx.on_error(format_error::invalid_format);
      return;
    }
    token(static_cast<char>(pattern[i]), pad, alt);
  }
}

// Exact portable multiply/divide without a wider intermediate. Both quotient
// overflow and remainder are explicit, including on devices without __int128.
FMT_HD constexpr bool family_muldiv(std::uint64_t a, std::uint64_t b,
                                    std::uint64_t d, std::uint64_t &quotient,
                                    std::uint64_t &remainder) {
  quotient = remainder = 0;
  auto aq = a / d, ar = a % d;
  constexpr auto max = (std::numeric_limits<std::uint64_t>::max)();
  for (int bit = 63; bit >= 0; --bit) {
    bool carry = remainder >= d - remainder;
    remainder = carry ? remainder - (d - remainder) : remainder + remainder;
    if (quotient > (max - unsigned(carry)) / 2)
      return false;
    quotient = quotient * 2 + unsigned(carry);
    if (!(b & (std::uint64_t(1) << bit)))
      continue;
    carry = remainder >= d - ar;
    remainder = carry ? remainder - (d - ar) : remainder + ar;
    if (aq > max - quotient || unsigned(carry) > max - quotient - aq)
      return false;
    quotient += aq + unsigned(carry);
  }
  return true;
}
template <typename Period> FMT_HD constexpr int family_fraction_digits() {
  auto d = Period::den;
  int count = 0;
  while (d > 1 && d % 10 == 0) {
    d /= 10;
    ++count;
  }
  return d == 1 ? count : 6;
}
FMT_HD constexpr std::uint64_t family_power10(int exponent) {
  std::uint64_t value = 1;
  while (exponent-- > 0)
    value *= 10;
  return value;
}
template <typename Period, typename Context>
FMT_HD constexpr void family_duration_unit(Context &ctx) {
  if constexpr (Period::den == 1) {
    if constexpr (Period::num == 1)
      family_ascii(ctx, "s");
    else if constexpr (Period::num == 60)
      family_ascii(ctx, "min");
    else if constexpr (Period::num == 3600)
      family_ascii(ctx, "h");
    else if constexpr (Period::num == 86400)
      family_ascii(ctx, "d");
    else if constexpr (Period::num == 10)
      family_ascii(ctx, "das");
    else if constexpr (Period::num == 100)
      family_ascii(ctx, "hs");
    else if constexpr (Period::num == 1000)
      family_ascii(ctx, "ks");
    else if constexpr (Period::num == 1000000)
      family_ascii(ctx, "Ms");
    else if constexpr (Period::num == 1000000000)
      family_ascii(ctx, "Gs");
    else if constexpr (Period::num == 1000000000000LL)
      family_ascii(ctx, "Ts");
    else if constexpr (Period::num == 1000000000000000LL)
      family_ascii(ctx, "Ps");
    else if constexpr (Period::num == 1000000000000000000LL)
      family_ascii(ctx, "Es");
    else {
      family_ascii(ctx, "[");
      family_chrono_number(ctx, Period::num);
      family_ascii(ctx, "]s");
    }
  } else if constexpr (Period::num == 1) {
    if constexpr (Period::den == 10)
      family_ascii(ctx, "ds");
    else if constexpr (Period::den == 100)
      family_ascii(ctx, "cs");
    else if constexpr (Period::den == 1000)
      family_ascii(ctx, "ms");
    else if constexpr (Period::den == 1000000) {
      if constexpr (sizeof(typename Context::char_type) == 1)
        family_ascii(ctx, "\xc2\xb5s");
      else {
        family_put(ctx, static_cast<typename Context::char_type>(0xb5));
        family_ascii(ctx, "s");
      }
    } else if constexpr (Period::den == 1000000000)
      family_ascii(ctx, "ns");
    else if constexpr (Period::den == 1000000000000LL)
      family_ascii(ctx, "ps");
    else if constexpr (Period::den == 1000000000000000LL)
      family_ascii(ctx, "fs");
    else if constexpr (Period::den == 1000000000000000000LL)
      family_ascii(ctx, "as");
    else {
      family_ascii(ctx, "[1/");
      family_chrono_number(ctx, Period::den);
      family_ascii(ctx, "]s");
    }
  } else {
    family_ascii(ctx, "[");
    family_chrono_number(ctx, Period::num);
    family_ascii(ctx, "/");
    family_chrono_number(ctx, Period::den);
    family_ascii(ctx, "]s");
  }
}

template <typename Rep, typename Period> struct family_duration_parts {
  Rep count;
  std::uint64_t seconds = 0, remainder = 0;
  Rep fp_seconds{};
  bool negative = false, valid = true, finite = true;
  FMT_HD explicit constexpr family_duration_parts(Rep value) : count(value) {
    if constexpr (std::is_integral_v<Rep> && sizeof(Rep) <= 8) {
      negative = std::is_signed_v<Rep> && value < 0;
      auto magnitude =
          negative ? std::uint64_t(0) - static_cast<std::uint64_t>(value)
                   : static_cast<std::uint64_t>(value);
      valid = family_muldiv(magnitude, Period::num, Period::den, seconds,
                            remainder);
    } else if constexpr (std::is_floating_point_v<Rep>) {
      negative = value < Rep(0);
      fp_seconds = (negative ? -value : value) * static_cast<Rep>(Period::num) /
                   static_cast<Rep>(Period::den);
      finite = std::isfinite(fp_seconds);
    } else
      valid = false;
  }
  FMT_HD constexpr std::uint64_t component(std::uint64_t modulus,
                                           std::uint64_t divisor = 1) const {
    if constexpr (std::is_integral_v<Rep>)
      return (seconds / divisor) % modulus;
    else
      return static_cast<std::uint64_t>(
          std::fmod(std::floor(fp_seconds / static_cast<Rep>(divisor)),
                    static_cast<Rep>(modulus)));
  }
};
} // namespace ffmt::detail

namespace ffmt {
template <typename Char> struct formatter<std::tm, Char> {
  using ffmt_family_type = std::tm;
  static constexpr bool ffmt_family_repeatable = true;
  detail::family_chrono_spec<Char> specs_;
  FMT_HD constexpr auto parse(basic_format_parse_context<Char> &ctx)
      -> const Char * {
    return specs_.parse(ctx, false);
  }
  template <typename Context>
  FMT_HD constexpr auto format(const std::tm &value, Context &ctx) const
      -> decltype(ctx.out()) {
    detail::family_calendar calendar;
    calendar.year = std::int64_t(value.tm_year) + 1900;
    calendar.month = value.tm_mon + 1;
    calendar.day = value.tm_mday;
    calendar.hour = value.tm_hour;
    calendar.minute = value.tm_min;
    calendar.second = value.tm_sec;
    calendar.weekday = value.tm_wday;
    calendar.yearday = value.tm_yday;
    if constexpr (requires { value.tm_gmtoff; })
      calendar.utc_offset = value.tm_gmtoff;
    if constexpr (requires { value.tm_zone; })
      calendar.zone = value.tm_zone;
    return format_calendar(calendar, ctx);
  }
  template <typename Context>
  FMT_HD constexpr auto format_calendar(const detail::family_calendar &value,
                                        Context &ctx) const
      -> decltype(ctx.out()) {
    return detail::family_padded(ctx, specs_.outer, [&](auto &bodyctx) {
      if (specs_.pattern.empty()) {
        detail::family_calendar_token(bodyctx, value, 'F');
        detail::family_ascii(bodyctx, " ");
        detail::family_calendar_token(bodyctx, value, 'T');
      } else
        detail::family_chrono_pattern(
            bodyctx, specs_.pattern, [&](char token, char pad, bool alt) {
              detail::family_calendar_token(bodyctx, value, token, pad, alt);
            });
    });
  }
};

template <typename Rep, typename Period, typename Char>
struct formatter<std::chrono::duration<Rep, Period>, Char> {
  using ffmt_family_type = std::chrono::duration<Rep, Period>;
  static constexpr bool ffmt_family_repeatable = true;
  detail::family_chrono_spec<Char> specs_;
  FMT_HD constexpr auto parse(basic_format_parse_context<Char> &ctx)
      -> const Char * {
    return specs_.parse(ctx, true, std::is_floating_point_v<Rep>);
  }
  template <typename Context>
  FMT_HD constexpr auto format(std::chrono::duration<Rep, Period> value,
                               Context &ctx) const -> decltype(ctx.out()) {
    auto resolved_spec = detail::resolved(ctx, specs_.outer);
    if (ctx.error() != format_error::none)
      return ctx.out();
    auto number_spec = detail::basic_format_spec<Char>{};
    if constexpr (std::is_floating_point_v<Rep>) {
      number_spec.precision = resolved_spec.precision;
      number_spec.type = number_spec.precision >= 0 ? 'f' : 'g';
    }
    return detail::family_padded(ctx, specs_.outer, [&](auto &bodyctx) {
      auto count_value = [&] {
        detail::write_value(bodyctx, value.count(), number_spec);
      };
      if (specs_.pattern.empty()) {
        count_value();
        detail::family_duration_unit<Period>(bodyctx);
        return;
      }
      detail::family_duration_parts<Rep, Period> parts(value.count());
      bool signed_time = false;
      auto sign = [&] {
        if (!signed_time && parts.negative)
          detail::family_put(bodyctx, Char('-'));
        signed_time = true;
      };
      auto time_number = [&](std::uint64_t v, char pad) {
        sign();
        detail::family_chrono_number(bodyctx, v, 2, pad);
      };
      auto seconds = [&](char pad) {
        sign();
        if constexpr (std::is_floating_point_v<Rep>) {
          auto p = resolved_spec.precision;
          if (p < 0) {
            p = detail::family_fraction_digits<Period>();
            if (p < 6 && std::round(value.count()) != value.count())
              p = 6;
          }
          auto s = detail::basic_format_spec<Char>{};
          s.type = 'f';
          s.precision = p;
          bool enormous_precision = p > INT_MAX - 3;
          s.width =
              (pad == '-' || enormous_precision ? 0 : 2 + (p ? p + 1 : 0));
          s.zero_pad = pad == '0';
          s.fill[0] = Char(pad == '-' ? ' ' : pad);
          s.align = '>';
          auto second = std::fmod(parts.fp_seconds, Rep(60));
          if (enormous_precision && pad != '-' && second < Rep(10))
            detail::family_put(bodyctx, Char(pad));
          detail::write_value(bodyctx, second, s);
        } else {
          detail::family_chrono_number(bodyctx, parts.seconds % 60, 2, pad);
          constexpr int digits = detail::family_fraction_digits<Period>();
          int precision =
              resolved_spec.precision >= 0 ? resolved_spec.precision : digits;
          if (precision) {
            std::uint64_t fraction = 0, remainder = 0;
            if (!detail::family_muldiv(parts.remainder,
                                       detail::family_power10(digits),
                                       Period::den, fraction, remainder)) {
              bodyctx.on_error(format_error::unsupported_type);
              return;
            }
            detail::family_put(bodyctx, Char('.'));
            if (precision <= digits)
              detail::family_chrono_number(
                  bodyctx,
                  fraction / detail::family_power10(digits - precision),
                  precision);
            else {
              if constexpr (digits)
                detail::family_chrono_number(bodyctx, fraction, digits);
              detail::repeat(bodyctx, Char('0'),
                             static_cast<std::size_t>(precision - digits));
            }
          }
        }
      };
      auto atom_writer = [&](char token, char pad) -> void {
        auto special = [&] {
          if constexpr (std::is_floating_point_v<Rep>)
            detail::family_ascii(bodyctx,
                                 std::isnan(value.count()) ? "nan" : "inf");
          else
            bodyctx.on_error(format_error::unsupported_type);
        };
        if (token == 'Q') {
          if (!parts.finite) {
            special();
            return;
          }
          sign();
          if constexpr (std::is_integral_v<Rep>) {
            auto magnitude =
                static_cast<detail::unsigned_t<Rep>>(value.count());
            if (parts.negative)
              magnitude = detail::unsigned_t<Rep>(0) - magnitude;
            detail::write_value(bodyctx, magnitude, number_spec);
          } else
            detail::write_value(bodyctx,
                                parts.negative ? -value.count() : value.count(),
                                number_spec);
          return;
        }
        if (token == 'q') {
          detail::family_duration_unit<Period>(bodyctx);
          return;
        }
        if (token == '%') {
          detail::family_ascii(bodyctx, "%");
          return;
        }
        if (token == 'n') {
          detail::family_ascii(bodyctx, "\n");
          return;
        }
        if (token == 't') {
          detail::family_ascii(bodyctx, "\t");
          return;
        }
        if (!parts.valid) {
          bodyctx.on_error(format_error::unsupported_type);
          return;
        }
        if (!parts.finite) {
          special();
          return;
        }
        switch (token) {
        case 'H':
          time_number(parts.component(24, 3600), pad);
          break;
        case 'I': {
          auto hour = parts.component(12, 3600);
          time_number(hour ? hour : 12, pad);
          break;
        }
        case 'M':
          time_number(parts.component(60, 60), pad);
          break;
        case 'S':
          seconds(pad);
          break;
        case 'p':
          detail::family_ascii(bodyctx,
                               parts.component(24, 3600) < 12 ? "AM" : "PM");
          break;
        case 'j': {
          sign();
          if constexpr (std::is_integral_v<Rep>) {
            if (parts.seconds / 86400 > INT_MAX)
              bodyctx.on_error(format_error::invalid_format);
            else
              detail::family_chrono_number(bodyctx, parts.seconds / 86400);
          } else {
            auto days = std::floor(parts.fp_seconds / Rep(86400));
            if (days > Rep(INT_MAX))
              bodyctx.on_error(format_error::invalid_format);
            else
              detail::family_chrono_number(bodyctx,
                                           static_cast<unsigned>(days));
          }
          break;
        }
        default:
          bodyctx.on_error(format_error::invalid_format);
          break;
        }
      };
      detail::family_chrono_pattern(
          bodyctx, specs_.pattern, [&](char token, char pad, bool alt) {
            (void)alt;
            if (!parts.valid ||
                (token != 'R' && token != 'T' && token != 'r')) {
              atom_writer(token, pad);
              return;
            }
            if (token == 'r') {
              if (!parts.finite) {
                atom_writer(token, pad);
                return;
              }
              atom_writer('I', '0');
              detail::family_ascii(bodyctx, ":");
              atom_writer('M', '0');
              detail::family_ascii(bodyctx, ":");
              detail::family_chrono_number(bodyctx, parts.component(60), 2);
              detail::family_ascii(bodyctx, " ");
              atom_writer('p', '0');
              return;
            }
            atom_writer('H', '0');
            detail::family_ascii(bodyctx, ":");
            atom_writer('M', '0');
            if (token == 'T') {
              detail::family_ascii(bodyctx, ":");
              atom_writer('S', '0');
            }
          });
    });
  }
};

namespace detail {
template <typename T>
concept family_calendar_type = std::is_same_v<T, std::chrono::year> ||
                               std::is_same_v<T, std::chrono::month> ||
                               std::is_same_v<T, std::chrono::day> ||
                               std::is_same_v<T, std::chrono::weekday> ||
                               std::is_same_v<T, std::chrono::year_month_day>;
}
template <typename T, typename Char>
  requires detail::family_calendar_type<T>
struct formatter<T, Char> {
  using ffmt_family_type = T;
  static constexpr bool ffmt_family_repeatable = true;
  formatter<std::tm, Char> underlying_;
  FMT_HD constexpr auto parse(basic_format_parse_context<Char> &ctx)
      -> const Char * {
    return underlying_.parse(ctx);
  }
  template <typename Context>
  FMT_HD constexpr auto format(T value, Context &ctx) const
      -> decltype(ctx.out()) {
    detail::family_calendar calendar;
    char default_token = 'F';
    if constexpr (std::is_same_v<T, std::chrono::year>) {
      calendar.year = int(value);
      default_token = 'Y';
    } else if constexpr (std::is_same_v<T, std::chrono::month>) {
      calendar.month = unsigned(value);
      default_token = 'b';
    } else if constexpr (std::is_same_v<T, std::chrono::day>) {
      calendar.day = unsigned(value);
      default_token = 'd';
    } else if constexpr (std::is_same_v<T, std::chrono::weekday>) {
      calendar.weekday = value.c_encoding();
      default_token = 'a';
    } else {
      auto days = detail::family_days_from_civil(
          int(value.year()), unsigned(value.month()), unsigned(value.day()));
      calendar = detail::family_civil_from_days(days);
      // Preserve invalid user-supplied components; formatting must not
      // normalize the supplied date before it has been validated by the caller.
      calendar.year = int(value.year());
      calendar.month = unsigned(value.month());
      calendar.day = unsigned(value.day());
    }
    if (!underlying_.specs_.pattern.empty())
      return underlying_.format_calendar(calendar, ctx);
    return detail::family_padded(
        ctx, underlying_.specs_.outer, [&](auto &bodyctx) {
          detail::family_calendar_token(bodyctx, calendar, default_token);
        });
  }
};

template <typename Clock, typename Duration, typename Char>
  requires(std::is_same_v<Clock, std::chrono::system_clock> ||
           std::is_same_v<Clock, std::chrono::local_t>)
struct formatter<std::chrono::time_point<Clock, Duration>, Char> {
  using ffmt_family_type = std::chrono::time_point<Clock, Duration>;
  static constexpr bool ffmt_family_repeatable = true;
  formatter<std::tm, Char> underlying_;
  FMT_HD constexpr auto parse(basic_format_parse_context<Char> &ctx)
      -> const Char * {
    auto end = underlying_.parse(ctx);
    if constexpr (std::is_same_v<Clock, std::chrono::local_t>) {
      auto pattern = underlying_.specs_.pattern;
      for (std::size_t i = 0; i < pattern.size(); ++i) {
        if (pattern[i] != Char('%'))
          continue;
        if (++i == pattern.size())
          break;
        if (pattern[i] == Char('_') || pattern[i] == Char('-') ||
            pattern[i] == Char('0'))
          ++i;
        if (i == pattern.size())
          break;
        if (pattern[i] == Char('E') || pattern[i] == Char('O'))
          ++i;
        if (i < pattern.size() &&
            (pattern[i] == Char('z') || pattern[i] == Char('Z')))
          ctx.on_error(format_error::invalid_format);
      }
    }
    return end;
  }
  template <typename Context>
  FMT_HD constexpr auto
  format(const std::chrono::time_point<Clock, Duration> &value,
         Context &ctx) const -> decltype(ctx.out()) {
    using Rep = typename Duration::rep;
    using Period = typename Duration::period;
    detail::family_duration_parts<Rep, Period> parts(
        value.time_since_epoch().count());
    if (!parts.valid || !parts.finite) {
      ctx.on_error(format_error::unsupported_type);
      return ctx.out();
    }
    std::int64_t seconds = 0;
    std::uint64_t fraction_value = 0;
    int fraction_digits = detail::family_fraction_digits<Period>();
    if constexpr (std::is_integral_v<Rep>) {
      if (parts.seconds > static_cast<std::uint64_t>(
                              (std::numeric_limits<std::int64_t>::max)()) -
                              1) {
        ctx.on_error(format_error::unsupported_type);
        return ctx.out();
      }
      seconds = static_cast<std::int64_t>(parts.seconds);
      auto fraction = parts.remainder;
      if (parts.negative) {
        seconds = -seconds;
        if (fraction) {
          --seconds;
          fraction = Period::den - fraction;
        }
      }
      std::uint64_t remainder = 0;
      if (!detail::family_muldiv(fraction,
                                 detail::family_power10(fraction_digits),
                                 Period::den, fraction_value, remainder)) {
        ctx.on_error(format_error::unsupported_type);
        return ctx.out();
      }
    } else if constexpr (std::is_floating_point_v<Rep>) {
      if (parts.fp_seconds >=
          static_cast<Rep>((std::numeric_limits<std::int64_t>::max)())) {
        ctx.on_error(format_error::unsupported_type);
        return ctx.out();
      }
      seconds = static_cast<std::int64_t>(parts.fp_seconds);
      if (parts.negative)
        seconds = -seconds;
      Rep remainder =
          value.time_since_epoch().count() - static_cast<Rep>(seconds) *
                                                 static_cast<Rep>(Period::den) /
                                                 static_cast<Rep>(Period::num);
      if (remainder < Rep(0)) {
        --seconds;
        remainder +=
            static_cast<Rep>(Period::den) / static_cast<Rep>(Period::num);
      }
      if (fraction_digits < 6 && std::round(remainder) != remainder)
        fraction_digits = 6;
      if (fraction_digits) {
        // At most 18 decimal places plus "0.". Keep the native floating type
        // through conversion, including rounding, then retain the fraction.
        Char digits[32];
        auto fraction_ctx = ctx.rebind(digits);
        detail::basic_format_spec<Char> spec;
        spec.type = 'f';
        spec.precision = fraction_digits;
        detail::write_float(fraction_ctx,
                            remainder * static_cast<Rep>(Period::num) /
                                static_cast<Rep>(Period::den),
                            spec);
        if (fraction_ctx.error() != format_error::none)
          return ctx.out();
        for (auto it = digits + 2; it != fraction_ctx.out(); ++it)
          fraction_value =
              fraction_value * 10 + static_cast<unsigned>(*it - Char('0'));
      }
    } else {
      ctx.on_error(format_error::unsupported_type);
      return ctx.out();
    }
    auto days = seconds / 86400;
    auto day_seconds = seconds % 86400;
    if (day_seconds < 0) {
      --days;
      day_seconds += 86400;
    }
    auto calendar = detail::family_civil_from_days(days);
    calendar.hour = static_cast<int>(day_seconds / 3600);
    calendar.minute = static_cast<int>((day_seconds / 60) % 60);
    calendar.second = static_cast<int>(day_seconds % 60);
    calendar.fraction_digits = fraction_digits;
    calendar.fraction = fraction_value;
    if constexpr (std::is_same_v<Clock, std::chrono::system_clock>)
      calendar.zone = "UTC";
    return underlying_.format_calendar(calendar, ctx);
  }
};
} // namespace ffmt
