// Numeric formatting. The bounded decimal expansion is a property of IEEE
// binary32/binary64, not an output capacity; large requested precision streams
// its remaining zeros directly to the destination.
#pragma once
#include "floating.hpp"
#include "integer.hpp"

#ifndef FFMT_PORTABLE_SHORTEST
// Fast-math may fold Dragonbox's bitwise zero test into a floating comparison,
// which treats subnormals as zero when the runtime enables DAZ. The interval
// converter keeps classification and rounding in integer representation data.
#if defined(__SPIR__) || defined(__SPIRV__) || defined(__FAST_MATH__)
#define FFMT_PORTABLE_SHORTEST 1
#else
#define FFMT_PORTABLE_SHORTEST 0
#endif
#endif
namespace ffmt::detail {
struct decimal_digits {
  // A binary64 exact expansion has at most 1075 significant decimal places.
  char data[1100];
  int size = 0, exponent = 0;
  FMT_HD explicit decimal_digits(double value) {
    exact_decimal source(value);
    exponent = source.int_digits - 1;
    if (!source.rest_nonzero()) {
      data[size++] = '0';
      exponent = 0;
      return;
    }
    int digit = source.next();
    while (!digit) {
      --exponent;
      digit = source.next();
    }
    data[size++] = char('0' + digit);
    while (source.rest_nonzero())
      data[size++] = char('0' + source.next());
  }
  FMT_HD void round(long long keep) {
    if (keep >= size)
      return;
    if (keep < 0) {
      data[0] = '0';
      size = 1;
      exponent = 0;
      return;
    }
    int retained = static_cast<int>(keep);
    bool sticky = false;
    for (int i = retained + 1; i < size; ++i)
      sticky = sticky || data[i] != '0';
    int last = retained ? data[retained - 1] - '0' : 0;
    bool up = data[retained] > '5' ||
              (data[retained] == '5' && (sticky || (last & 1)));
    if (!retained) {
      if (up) {
        data[0] = '1';
        size = 1;
        ++exponent;
      } else {
        data[0] = '0';
        size = 1;
        exponent = 0;
      }
      return;
    }
    size = retained;
    if (!up)
      return;
    int i = size - 1;
    while (i >= 0 && data[i] == '9')
      data[i--] = '0';
    if (i >= 0)
      ++data[i];
    else {
      data[0] = '1';
      size = 1;
      ++exponent;
    }
  }
  FMT_HD void trim() {
    while (size > 1 && data[size - 1] == '0')
      --size;
  }
  FMT_HD char at(long long index) const {
    return index >= 0 && index < size ? data[index] : '0';
  }
};
FMT_HD inline double widen_float(float f) {
  auto bits = __builtin_bit_cast(std::uint32_t, f);
  if ((bits & 0x7f800000u) || (bits & 0x7fffffu) == 0)
    return static_cast<double>(f);
  double d = static_cast<double>(bits & 0x7fffffu) * 0x1p-149;
  return bits >> 31 ? -d : d;
}
FMT_HD inline double widen_float(double d) { return d; }

template <typename Context, typename Char>
FMT_HD void exponent_text(Context &ctx, int exponent, bool upper) {
  put(ctx, Char(upper ? 'E' : 'e'));
  put(ctx, Char(exponent < 0 ? '-' : '+'));
  unsigned n = exponent < 0 ? static_cast<unsigned>(-exponent)
                            : static_cast<unsigned>(exponent);
  char digits[8];
  unsigned size = 0;
  do {
    digits[size++] = char('0' + n % 10);
    n /= 10;
  } while (n);
  if (size < 2)
    put(ctx, Char('0'));
  for (unsigned i = size; i > 0; --i)
    put(ctx, Char(digits[i - 1]));
}
FMT_HD constexpr std::size_t exponent_size(int e) {
  unsigned n = e < 0 ? unsigned(-e) : unsigned(e);
  return 2 + (n >= 1000 ? 4 : n >= 100 ? 3 : 2);
}

template <typename Context, typename Char, typename Decimal>
FMT_HD void write_decimal_number(Context &ctx, Decimal &dec,
                                 basic_format_spec<Char> s, char sign,
                                 bool upper) {
  char type = s.type ? s.type : 'g';
  int precision = s.precision < 0 ? 6 : s.precision;
  bool scientific = type == 'e' || type == 'E';
  bool fixed = type == 'f' || type == 'F';
  if (fixed)
    dec.round(static_cast<long long>(dec.exponent) + precision + 1);
  else {
    if (!scientific && precision == 0)
      precision = 1;
    dec.round(static_cast<long long>(precision) + (scientific ? 1 : 0));
  }
  long long fraction_digits;
  if (fixed)
    fraction_digits = precision;
  else if (scientific)
    fraction_digits = precision;
  else {
    scientific = dec.exponent < -4 || dec.exponent >= precision;
    fraction_digits =
        scientific ? static_cast<long long>(precision) - 1
                   : static_cast<long long>(precision) - 1 - dec.exponent;
    if (!s.alt) {
      dec.trim();
      int significant_fraction =
          scientific ? dec.size - 1 : dec.size - 1 - dec.exponent;
      fraction_digits = significant_fraction > 0 ? significant_fraction : 0;
    }
  }
  std::size_t count = scientific ? 1
                      : dec.exponent >= 0
                          ? static_cast<std::size_t>(dec.exponent) + 1
                          : 1;
  if (fraction_digits > 0 || s.alt)
    count += 1 + static_cast<std::size_t>(fraction_digits);
  if (scientific)
    count += exponent_size(dec.exponent);
  numeric_padded(ctx, s, count, sign, "", 0, true, [&](auto &output) {
    if (scientific)
      put(output, Char(dec.data[0]));
    else if (dec.exponent < 0)
      put(output, Char('0'));
    else
      for (int i = 0; i <= dec.exponent; ++i)
        put(output, Char(dec.at(i)));
    if (fraction_digits > 0 || s.alt)
      put(output, Char('.'));
    long long first = scientific ? 1 : static_cast<long long>(dec.exponent) + 1;
    long long i = 0;
    if (first < 0) {
      std::size_t leading = static_cast<std::size_t>(-first);
      if (leading > static_cast<std::size_t>(fraction_digits))
        leading = static_cast<std::size_t>(fraction_digits);
      repeat(output, Char('0'), leading);
      first += static_cast<long long>(leading);
      i = static_cast<long long>(leading);
    }
    for (; i < fraction_digits && first < dec.size; ++i, ++first)
      put(output, Char(dec.at(first)));
    if (i < fraction_digits)
      repeat(output, Char('0'), static_cast<std::size_t>(fraction_digits - i));
    if (scientific)
      exponent_text<decltype(output), Char>(output, dec.exponent, upper);
  });
}

template <typename Context, typename T, typename Char>
FMT_HD void write_extended_float(Context &, T, basic_format_spec<Char>);

template <typename Context, typename T, typename Char>
FMT_HD void write_float(Context &ctx, T arg, basic_format_spec<Char> s) {
  if constexpr (!std::is_same_v<T, float> && !std::is_same_v<T, double>) {
    // Devices differ in their long-double representation and availability.
    // Do not narrow an extended representation and report a false success.
    if constexpr (sizeof(T) == sizeof(double) &&
                  std::numeric_limits<T>::digits == 53)
      write_float(ctx, static_cast<double>(arg), s);
    else
      ctx.on_error(format_error::unsupported_type);
  } else {
    s = resolved(ctx, s);
    if (ctx.error() != format_error::none)
      return;
    using Bits = std::conditional_t<std::is_same_v<T, float>, std::uint32_t,
                                    std::uint64_t>;
    constexpr int fraction = std::numeric_limits<T>::digits - 1;
    constexpr Bits sign_mask = Bits(1) << (sizeof(T) * CHAR_BIT - 1);
    constexpr Bits fraction_mask = (Bits(1) << fraction) - 1;
    constexpr Bits exponent_mask = ~(sign_mask | fraction_mask);
    Bits bits = __builtin_bit_cast(Bits, arg);
    bool negative = bool(bits & sign_mask);
    T positive = __builtin_bit_cast(T, static_cast<Bits>(bits & ~sign_mask));
    bool finite = (bits & exponent_mask) != exponent_mask;
    bool upper =
        s.type == 'F' || s.type == 'E' || s.type == 'G' || s.type == 'A';
    char sign = negative ? '-' : s.sign == '+' ? '+' : s.sign == ' ' ? ' ' : 0;
    if (!finite) {
      bool nan = bool(bits & fraction_mask);
      const char *word =
          nan ? (upper ? "NAN" : "nan") : (upper ? "INF" : "inf");
      numeric_padded(ctx, s, 3, sign, "", 0, false,
                     [&](auto &output) { append(output, word, 3); });
      return;
    }
    if (s.type == 'a' || s.type == 'A') {
      auto raw = __builtin_bit_cast(std::uint64_t, widen_float(positive));
      int biased = static_cast<int>((raw >> 52) & 0x7ff),
          exponent = biased                       ? biased - 1023
                     : (raw & 0xfffffffffffffull) ? -1022
                                                  : 0;
      std::uint64_t mantissa = raw & 0xfffffffffffffull;
      int lead = biased ? 1 : 0, digits = 13;
      int precision = s.precision;
      if (precision >= 0 && precision < 13) {
        int drop = (13 - precision) * 4;
        bool up = ((mantissa >> (drop - 4)) & 15) >= 8;
        mantissa >>= drop;
        if (up) {
          ++mantissa;
          if (mantissa >> (precision * 4)) {
            mantissa = 0;
            ++lead;
          }
        }
        digits = precision;
      } else if (precision < 0)
        while (digits && (mantissa & 15) == 0) {
          mantissa >>= 4;
          --digits;
        }
      std::size_t zeros =
          precision > 13 ? static_cast<std::size_t>(precision - 13) : 0;
      unsigned abs_exp = exponent < 0 ? static_cast<unsigned>(-exponent)
                                      : static_cast<unsigned>(exponent);
      unsigned exp_digits = 1;
      for (unsigned v = abs_exp; v >= 10; v /= 10)
        ++exp_digits;
      std::size_t count = 1 + ((digits || zeros || s.alt) ? 1 : 0) + digits +
                          zeros + 2 + exp_digits;
      numeric_padded(
          ctx, s, count, sign, upper ? "0X" : "0x", 2, true, [&](auto &output) {
            put(output,
                Char(lead < 10 ? '0' + lead : (upper ? 'A' : 'a') + lead - 10));
            if (digits || zeros || s.alt)
              put(output, Char('.'));
            for (int i = digits - 1; i >= 0; --i) {
              unsigned d = static_cast<unsigned>((mantissa >> (i * 4)) & 15);
              put(output,
                  Char(d < 10 ? '0' + d : (upper ? 'A' : 'a') + d - 10));
            }
            repeat(output, Char('0'), zeros);
            put(output, Char(upper ? 'P' : 'p'));
            put(output, Char(exponent < 0 ? '-' : '+'));
            char expbuf[8];
            unsigned count = 0, v = abs_exp;
            do {
              expbuf[count++] = char('0' + v % 10);
              v /= 10;
            } while (v);
            for (unsigned i = count; i > 0; --i)
              put(output, Char(expbuf[i - 1]));
          });
      return;
    }
    if (!s.type && s.precision < 0) {
      char text[40];
#if FFMT_PORTABLE_SHORTEST
      int length = portable_shortest(text, positive);
#else
      int length = dragonbox::format_shortest(text, positive);
#endif
      if (s.alt) {
        int pos = 0;
        while (pos < length && text[pos] != '.' && text[pos] != 'e')
          ++pos;
        if (pos == length || text[pos] == 'e') {
          for (int i = length; i > pos; --i)
            text[i] = text[i - 1];
          text[pos] = '.';
          ++length;
        }
      }
      numeric_padded(ctx, s, static_cast<std::size_t>(length), sign, "", 0,
                     true, [&](auto &output) {
                       append(output, text, static_cast<std::size_t>(length));
                     });
      return;
    }
    decimal_digits dec(widen_float(positive));
    write_decimal_number(ctx, dec, s, sign, upper);
  }
}
} // namespace ffmt::detail
