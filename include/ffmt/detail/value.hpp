#pragma once
namespace ffmt::detail {
template <typename Context, typename T, typename Char>
FMT_HD void write_value(Context &ctx, const T &value,
                        basic_format_spec<Char> s) {
  using U = std::remove_cvref_t<T>;
  if (!compatible<U>(s)) {
    ctx.on_error(format_error::incompatible_type);
    return;
  }
  if constexpr (char_pointer<U, Char> || string_like<U, Char>) {
    if (s.type == 'p') {
      if constexpr (std::is_pointer_v<U>) {
        auto address = reinterpret_cast<std::uintptr_t>(value);
        s.type = 'x';
        s.alt = true;
        s.zero_pad = false;
        write_integer(ctx, address, s);
      } else
        ctx.on_error(format_error::incompatible_type);
    } else {
      if constexpr (std::is_pointer_v<U>)
        if (value == nullptr) {
          ctx.on_error(format_error::invalid_format);
          return;
        }
      if constexpr (std::is_pointer_v<U> || std::is_array_v<U>)
        write_string(ctx, basic_string_view<Char>{value, string_length(value)},
                     s);
      else
        write_string(ctx, basic_string_view<Char>{value}, s);
    }
  } else if constexpr (std::is_same_v<U, bool>) {
    if (!s.type || s.type == 's') {
      // bool text has string-like alignment but does not accept precision.
      const char *word = value ? "true" : "false";
      std::size_t length = value ? 4 : 5;
      write_padded(
          ctx, s, length, [&](auto &output) { append(output, word, length); },
          '<');
    } else
      write_integer(ctx, value, s);
  } else if constexpr (integer<U>) {
    if (s.type == 'c' || (character<U, Char> && (!s.type || s.type == '?'))) {
      if constexpr (!character<U, Char>) {
        if constexpr (signed_integer<U>)
          if (value < 0) {
            ctx.on_error(format_error::invalid_code_point);
            return;
          }
        if (static_cast<unsigned_t<U>>(value) >
            static_cast<unsigned_t<U>>(
                std::numeric_limits<std::make_unsigned_t<Char>>::max())) {
          ctx.on_error(format_error::invalid_code_point);
          return;
        }
      }
      Char character = static_cast<Char>(value);
      write_string(ctx, basic_string_view<Char>{&character, 1}, s, true);
    } else
      write_integer(ctx, value, s);
  } else if constexpr (floating<U>)
    write_float(ctx, value, s);
  else if constexpr (std::is_pointer_v<U> || std::is_null_pointer_v<U>) {
    std::uintptr_t address = 0;
    if constexpr (std::is_pointer_v<U>)
      address = reinterpret_cast<std::uintptr_t>(value);
    s.type = 'x';
    s.alt = true;
    s.zero_pad = false;
    write_integer(ctx, address, s);
  } else
    ctx.on_error(format_error::unsupported_type);
}

}
