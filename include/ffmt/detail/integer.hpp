#pragma once
namespace ffmt::detail {
template <typename Context, typename Char, typename Body>
FMT_HD void numeric_padded(Context &ctx, basic_format_spec<Char> s,
                           std::size_t digits, char sign, const char *prefix,
                           std::size_t prefix_size, bool finite, Body body) {
  s = resolved(ctx, s);
  if (ctx.error() != format_error::none)
    return;
  std::size_t size = digits + prefix_size + (sign ? 1 : 0);
  std::size_t zeros = s.zero_pad && !s.align && finite && s.width > 0 &&
                              static_cast<std::size_t>(s.width) > size
                          ? static_cast<std::size_t>(s.width) - size
                          : 0;
  write_padded(
      ctx, s, size + zeros,
      [&](auto &output) {
        if (sign)
          put(output, Char(sign));
        append(output, prefix, prefix_size);
        repeat(output, Char('0'), zeros);
        body(output);
      },
      '>');
}
template <typename Context, typename T, typename Char>
FMT_HD void write_integer(Context &ctx, T value, basic_format_spec<Char> s) {
  using U = unsigned_t<T>;
  U n = static_cast<U>(value);
  bool negative = false;
  if constexpr (signed_integer<T>)
    if (value < 0) {
      negative = true;
      n = U(0) - n;
    }
  unsigned base = s.type == 'x' || s.type == 'X'   ? 16
                  : s.type == 'o'                  ? 8
                  : s.type == 'b' || s.type == 'B' ? 2
                                                   : 10;
  bool upper = s.type == 'X' || s.type == 'B';
  char digits[sizeof(T) * CHAR_BIT + 1];
  unsigned count = 0;
  do {
    unsigned d = static_cast<unsigned>(n % base);
    digits[count++] = char(d < 10 ? '0' + d : (upper ? 'A' : 'a') + d - 10);
    n /= base;
  } while (n);
  const char *prefix = "";
  unsigned prefix_size = 0;
  if (s.alt) {
    if (base == 16) {
      prefix = upper ? "0X" : "0x";
      prefix_size = 2;
    } else if (base == 2) {
      prefix = upper ? "0B" : "0b";
      prefix_size = 2;
    } else if (base == 8 && (count != 1 || digits[0] != '0')) {
      prefix = "0";
      prefix_size = 1;
    }
  }
  char sign = negative ? '-' : s.sign == '+' ? '+' : s.sign == ' ' ? ' ' : 0;
  numeric_padded(ctx, s, count, sign, prefix, prefix_size, true,
                 [&](auto &output) {
                   for (unsigned i = count; i > 0; --i)
                     put(output, Char(digits[i - 1]));
                 });
}


}
