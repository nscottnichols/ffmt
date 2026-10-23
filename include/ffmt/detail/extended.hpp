// Native extended representations use the same bounded decimal algorithm;
// neither values nor their formatting precision are narrowed to binary64.
#pragma once
namespace ffmt::detail {
template <typename Context, typename T, typename Char>
FMT_HD void write_extended_float(Context &ctx, T arg,
                                 basic_format_spec<Char> s) {
  auto parts = split_binary(arg);
  if (!parts.supported) {
    ctx.on_error(format_error::unsupported_type);
    return;
  }
  s = resolved(ctx, s);
  if (ctx.error() != format_error::none)
    return;
  bool upper = s.type == 'F' || s.type == 'E' || s.type == 'G' || s.type == 'A';
  char sign = parts.negative  ? '-'
              : s.sign == '+' ? '+'
              : s.sign == ' ' ? ' '
                              : 0;
  if (!parts.finite) {
    const char *word =
        parts.nan ? (upper ? "NAN" : "nan") : (upper ? "INF" : "inf");
    numeric_padded(ctx, s, 3, sign, "", 0, false,
                   [&](auto &out) { append(out, word, 3); });
    return;
  }
  if (s.type == 'a' || s.type == 'A') {
    auto rounded = parts;
    unsigned total = (parts.precision + 3) / 4, full_fraction = total - 1;
    unsigned lead = parts.nibble(full_fraction);
    int exponent = parts.zero() ? 0 : parts.exponent + parts.precision - 1;
    if (lead > 1) {
      unsigned n = lead;
      while (n >> 1) {
        --exponent;
        n >>= 1;
      }
    }
    int precision = s.precision;
    if (precision >= 0 && unsigned(precision) < full_fraction) {
      unsigned keep_bit = (full_fraction - unsigned(precision)) * 4;
      bool up = parts.nibble(full_fraction - unsigned(precision) - 1) >= 8;
      for (unsigned i = 0; i < 4; ++i) {
        unsigned start = i * 32;
        if (start >= keep_bit)
          break;
        unsigned n = keep_bit - start;
        if (n >= 32)
          rounded.words[i] = 0;
        else
          rounded.words[i] &= ~((std::uint32_t(1) << n) - 1);
      }
      if (up) {
        unsigned word = keep_bit / 32, shift = keep_bit % 32;
        std::uint64_t sum =
            std::uint64_t(rounded.words[word]) + (std::uint64_t(1) << shift);
        rounded.words[word] = std::uint32_t(sum);
        std::uint32_t carry = std::uint32_t(sum >> 32);
        for (++word; word < 4 && carry; ++word) {
          sum = std::uint64_t(rounded.words[word]) + carry;
          rounded.words[word] = std::uint32_t(sum);
          carry = std::uint32_t(sum >> 32);
        }
      }
      if (parts.precision == 64 && (rounded.words[2] & 1)) {
        rounded.shift_right(4);
        exponent += 4;
      }
    }
    unsigned fraction = precision >= 0 && unsigned(precision) < full_fraction
                            ? unsigned(precision)
                            : full_fraction;
    while (fraction && rounded.nibble(full_fraction - fraction) == 0)
      --fraction;
    std::size_t zeros =
        precision > int(fraction) ? std::size_t(precision) - fraction : 0;
    unsigned abs_exp = exponent < 0 ? unsigned(-exponent) : unsigned(exponent),
             exp_digits = 1;
    for (unsigned n = abs_exp; n >= 10; n /= 10)
      ++exp_digits;
    bool point = fraction || zeros || s.alt;
    std::size_t size = 1 + (point ? 1 : 0) + fraction + zeros + 2 + exp_digits;
    numeric_padded(
        ctx, s, size, sign, upper ? "0X" : "0x", 2, true, [&](auto &out) {
          auto hex = [&](unsigned digit) {
            put(out, Char(digit < 10 ? '0' + digit
                                     : (upper ? 'A' : 'a') + digit - 10));
          };
          hex(rounded.nibble(full_fraction));
          if (point)
            put(out, Char('.'));
          for (unsigned i = 0; i < fraction; ++i)
            hex(rounded.nibble(full_fraction - 1 - i));
          repeat(out, Char('0'), zeros);
          put(out, Char(upper ? 'P' : 'p'));
          put(out, Char(exponent < 0 ? '-' : '+'));
          char digits[8];
          unsigned count = 0, n = abs_exp;
          do {
            digits[count++] = char('0' + n % 10);
            n /= 10;
          } while (n);
          for (unsigned i = count; i > 0; --i)
            put(out, Char(digits[i - 1]));
        });
    return;
  }
  if (!s.type && s.precision < 0) {
    char text[64];
    int size = portable_shortest(text, arg);
    if (s.alt) {
      int pos = 0;
      while (pos < size && text[pos] != '.' && text[pos] != 'e')
        ++pos;
      if (pos == size || text[pos] == 'e') {
        for (int i = size; i > pos; --i)
          text[i] = text[i - 1];
        text[pos] = '.';
        ++size;
      }
    }
    numeric_padded(ctx, s, std::size_t(size), sign, "", 0, true,
                   [&](auto &out) { append(out, text, std::size_t(size)); });
    return;
  }
  precise_decimal<16520> decimal(parts);
  write_decimal_number(ctx, decimal, s, sign, upper);
}
} // namespace ffmt::detail
