#pragma once
#include "unicode.hpp"
namespace ffmt::detail {
template <typename Char> struct text_piece {
  Char data[12];
  unsigned size = 0;
  std::size_t columns = 0;
  FMT_HD void push(Char c) { data[size++] = c; }
};
template <typename Char>
FMT_HD auto escaped_piece(std::uint32_t cp, const Char *raw, unsigned units,
                          bool character) -> text_piece<Char> {
  text_piece<Char> result;
  bool escape = (cp < 0x20 || cp == 0x7f || cp == '"' || cp == '\\' ||
                 !unicode_printable(cp));
  if (character)
    escape = (escape && cp != '"') || cp == '\'';
  if (!escape) {
    for (unsigned i = 0; i < units; ++i)
      result.push(raw[i]);
    result.columns = sizeof(Char) == 1 ? display_width_of(cp) : units;
    return result;
  }
  result.push(Char('\\'));
  switch (cp) {
  case '\n':
    result.push(Char('n'));
    break;
  case '\r':
    result.push(Char('r'));
    break;
  case '\t':
    result.push(Char('t'));
    break;
  case '"':
  case '\'':
  case '\\':
    result.push(Char(cp));
    break;
  default: {
    if (cp == invalid_code_point)
      cp = static_cast<unsigned char>(*raw);
    unsigned digits = cp < 0x100 ? 2 : cp < 0x10000 ? 4 : 8;
    result.push(Char(digits == 2 ? 'x' : digits == 4 ? 'u' : 'U'));
    for (unsigned i = digits; i > 0; --i) {
      unsigned d = (cp >> ((i - 1) * 4)) & 15;
      result.push(Char(d < 10 ? '0' + d : 'a' + d - 10));
    }
    break;
  }
  }
  result.columns = result.size;
  return result;
}
template <typename Context, typename Char>
FMT_HD void write_string(Context &ctx, basic_string_view<Char> value,
                         basic_format_spec<Char> s, bool character = false) {
  s = resolved(ctx, s);
  if (ctx.error() != format_error::none)
    return;
  bool debug = s.type == '?';
  std::size_t columns = 0;
  std::size_t limit = s.precision < 0 ? std::numeric_limits<std::size_t>::max()
                                      : static_cast<std::size_t>(s.precision);
  auto body = [&](auto &output, bool measuring) {
    std::size_t used = 0;
    auto emit_piece = [&](const Char *data, std::size_t n, std::size_t width) {
      if (width > limit - used) {
        // fmt permits a precision boundary inside an ASCII escape sequence.
        if (debug && width == n) {
          n = limit - used;
          width = n;
        } else
          return false;
      }
      if (!measuring)
        append(output, data, n);
      used += width;
      return used < limit;
    };
    Char quote = Char(character ? '\'' : '"');
    if (debug && !emit_piece(&quote, 1, 1)) {
      if (measuring)
        columns = used;
      return;
    }
    bool truncated = false;
    for (std::size_t i = 0; i < value.size();) {
      std::uint32_t cp = static_cast<std::make_unsigned_t<Char>>(value[i]);
      unsigned n = 1;
      if constexpr (sizeof(Char) == 1) {
#if FFMT_UNICODE
        n = decode_utf8(value.data() + i, value.size() - i, cp);
#endif
      }
      if (debug) {
        auto piece = escaped_piece(cp, value.data() + i, n, character);
        if (!emit_piece(piece.data, piece.size, piece.columns)) {
          truncated = true;
          break;
        }
      } else {
        std::size_t width = sizeof(Char) == 1 ? display_width_of(cp) : 1;
        if (!emit_piece(value.data() + i, n, width))
          break;
      }
      i += n;
    }
    if (debug && !truncated && used < limit)
      emit_piece(&quote, 1, 1);
    if (measuring)
      columns = used;
  };
  // Wide fmt precision is in code units, and its debug presentation ignores
  // string precision while measuring the complete escaped representation.
  if constexpr (sizeof(Char) != 1)
    if (debug)
      limit = std::numeric_limits<std::size_t>::max();
  body(ctx, true);
  write_padded(
      ctx, s, columns, [&](auto &output) { body(output, false); }, '<');
}

} // namespace ffmt::detail
