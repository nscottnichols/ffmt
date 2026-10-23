#pragma once
namespace ffmt {
namespace detail {
template <typename Char> FMT_HD constexpr bool is_digit(Char c) {
  return c >= Char('0') && c <= Char('9');
}
template <typename Char> FMT_HD constexpr bool is_name_start(Char c) {
  return (c >= Char('a') && c <= Char('z')) ||
         (c >= Char('A') && c <= Char('Z')) || c == Char('_');
}
template <typename Char> FMT_HD constexpr bool is_name(Char c) {
  return is_name_start(c) || is_digit(c);
}
template <typename Char> FMT_HD constexpr bool is_align(Char c) {
  return c == Char('<') || c == Char('>') || c == Char('^');
}

}
template <typename Char> class basic_format_parse_context {
  const Char *begin_;
  const Char *end_;
  detail::index_state local_{};
  detail::index_state *state_;
  format_error error_ = format_error::none;

public:
  using char_type = Char;
  using iterator = const Char *;
  FMT_HD constexpr explicit basic_format_parse_context(
      basic_string_view<Char> str, detail::index_state *state = nullptr)
      : begin_(str.data()), end_(str.data()), state_(state ? state : &local_) {
    if (str.empty())
      return;
    // GCC ASan rejects a constant-evaluated null comparison of a template
    // parameter object's data. Literal storage is valid by construction;
    // preserve the checked runtime path without comparing that address.
    if (!std::is_constant_evaluated() && !begin_) {
      on_error(format_error::invalid_format);
      return;
    }
    end_ += str.size();
  }
  FMT_HD constexpr iterator begin() const { return begin_; }
  FMT_HD constexpr iterator end() const { return end_; }
  FMT_HD constexpr void advance_to(iterator p) { begin_ = p; }
  FMT_HD constexpr void on_error(format_error error) {
    if (error_ == format_error::none)
      error_ = error;
  }
  FMT_HD constexpr void on_error(const char *) {
    on_error(format_error::invalid_format);
  }
  FMT_HD constexpr format_error error() const { return error_; }
  FMT_HD constexpr int next_arg_id() {
    if (state_->manual)
      on_error(format_error::invalid_format);
    state_->automatic = true;
    int id = state_->next++;
    if (state_->available >= 0 && id >= state_->available)
      on_error(format_error::argument_out_of_range);
    return id;
  }
  FMT_HD constexpr void check_arg_id(int id) {
    if (state_->automatic || id < 0)
      on_error(format_error::invalid_format);
    state_->manual = true;
    if (state_->available >= 0 && id >= state_->available)
      on_error(format_error::argument_out_of_range);
  }
  FMT_HD constexpr void check_arg_id(basic_string_view<Char>) {}
  FMT_HD constexpr void check_dynamic_spec(int id) {
    if (id >= 0 && state_->available >= 0) {
      if (id >= state_->available)
        on_error(format_error::argument_out_of_range);
      else if (state_->has_dynamic_types && !state_->dynamic_types[id])
        on_error(format_error::invalid_dynamic_spec);
    }
  }
};
using format_parse_context = basic_format_parse_context<char>;
using wformat_parse_context = basic_format_parse_context<wchar_t>;

namespace detail {
template <typename Char> struct basic_format_spec {
  Char fill[4] = {Char(' ')};
  unsigned fill_size = 1;
  char align = 0, sign = 0, type = 0;
  bool alt = false, zero_pad = false;
  int width = 0, precision = -1, width_arg = -1, precision_arg = -1;
  basic_string_view<Char> width_name{}, precision_name{};
};
template <typename Char>
FMT_HD constexpr int parse_number(const Char *&p, const Char *end,
                                  basic_format_parse_context<Char> &pc) {
  int result = 0;
  while (p != end && is_digit(*p)) {
    int digit = int(*p++ - Char('0'));
    if (result > (INT_MAX - digit) / 10) {
      pc.on_error(format_error::invalid_format);
      return 0;
    }
    result = result * 10 + digit;
  }
  return result;
}
template <typename Char>
FMT_HD constexpr void parse_dynamic(const Char *&p, const Char *end,
                                    basic_format_parse_context<Char> &pc,
                                    int &id, basic_string_view<Char> &name) {
  ++p;
  if (p == end) {
    pc.on_error(format_error::invalid_format);
    return;
  }
  if (*p == Char('}'))
    id = pc.next_arg_id();
  else if (is_digit(*p)) {
    bool leading = *p == Char('0');
    const Char *first = p;
    id = parse_number(p, end, pc);
    if (leading && p - first > 1)
      pc.on_error(format_error::invalid_format);
    pc.check_arg_id(id);
  } else if (is_name_start(*p)) {
    auto first = p++;
    while (p != end && is_name(*p))
      ++p;
    name = {first, static_cast<std::size_t>(p - first)};
  } else
    pc.on_error(format_error::invalid_format);
  if (p == end || *p != Char('}'))
    pc.on_error(format_error::invalid_format);
  else
    ++p;
  pc.check_dynamic_spec(id);
}
template <typename Char>
FMT_HD constexpr auto parse_standard_specs(basic_format_parse_context<Char> &pc)
    -> basic_format_spec<Char> {
  basic_format_spec<Char> s;
  auto p = pc.begin();
  auto end = pc.end();
  unsigned units = p != end ? 1 : 0;
  if (units && static_cast<std::size_t>(end - p) > units &&
      is_align(p[units])) {
    if (*p == Char('{') || *p == Char('}'))
      pc.on_error(format_error::invalid_format);
    s.fill_size = units;
    for (unsigned i = 0; i < units; ++i)
      s.fill[i] = p[i];
    s.align = char(p[units]);
    p += units + 1;
  } else if (p != end && is_align(*p))
    s.align = char(*p++);
  if (p != end && (*p == Char('+') || *p == Char('-') || *p == Char(' ')))
    s.sign = char(*p++);
  if (p != end && *p == Char('#')) {
    s.alt = true;
    ++p;
  }
  if (p != end && *p == Char('0')) {
    s.zero_pad = true;
    ++p;
  }
  if (p != end && *p == Char('{'))
    parse_dynamic(p, end, pc, s.width_arg, s.width_name);
  else
    s.width = parse_number(p, end, pc);
  if (p != end && *p == Char('.')) {
    ++p;
    if (p != end && *p == Char('{'))
      parse_dynamic(p, end, pc, s.precision_arg, s.precision_name);
    else if (p != end && is_digit(*p))
      s.precision = parse_number(p, end, pc);
    else
      pc.on_error(format_error::invalid_format);
  }
  if (p != end && *p != Char('}')) {
    s.type = char(*p++);
  }
  if (p != end && *p != Char('}'))
    pc.on_error(format_error::invalid_format);
  pc.advance_to(p);
  return s;
}

// Shared parser. A handler validates a selected argument at compile time, or
// writes it through the same formatter parse method at runtime.
template <typename Char, typename Handler>
FMT_HD constexpr format_error walk_format(basic_string_view<Char> text,
                                          Handler &&handle) {
  index_state state;
  if constexpr (requires { handle.initialize(state); })
    handle.initialize(state);
  auto p = text.data();
  if (text.empty())
    return format_error::none;
  // Avoid testing a template parameter object's address during literal
  // validation, while rejecting invalid runtime counted views before use.
  if (!std::is_constant_evaluated() && !p)
    return format_error::invalid_format;
  auto end = p + text.size();
  while (p != end) {
    if (*p != Char('{') && *p != Char('}')) {
      handle.literal(*p++);
      continue;
    }
    if (p + 1 != end && p[1] == *p) {
      handle.literal(*p);
      p += 2;
      continue;
    }
    if (*p == Char('}'))
      return format_error::invalid_format;
    ++p;
    int index = -1;
    basic_string_view<Char> name{};
    if (p == end)
      return format_error::invalid_format;
    basic_format_parse_context<Char> idctx{
        {p, static_cast<std::size_t>(end - p)}, &state};
    if (is_digit(*p)) {
      bool leading = *p == Char('0');
      const Char *start = p;
      index = parse_number(p, end, idctx);
      if (leading && p - start > 1)
        return format_error::invalid_format;
      idctx.check_arg_id(index);
    } else if (is_name_start(*p)) {
      auto start = p++;
      while (p != end && is_name(*p))
        ++p;
      name = {start, static_cast<std::size_t>(p - start)};
    } else if (*p == Char(':') || *p == Char('}'))
      index = idctx.next_arg_id();
    else
      return format_error::invalid_format;
    if (idctx.error() != format_error::none)
      return idctx.error();
    bool has_specs = p != end && *p == Char(':');
    if (has_specs)
      ++p;
    auto specs = p;
    int nesting = 0;
    if (has_specs)
      while (p != end) {
        if (*p == Char('{'))
          ++nesting;
        else if (*p == Char('}')) {
          if (!nesting)
            break;
          --nesting;
        }
        ++p;
      }
    if (p == end || *p != Char('}'))
      return format_error::invalid_format;
    basic_format_parse_context<Char> pc{
        {specs, static_cast<std::size_t>(p - specs + 1)}, &state};
    auto error = handle.argument(index, name, pc);
    if (error != format_error::none)
      return error;
    if (pc.error() != format_error::none)
      return pc.error();
    if (pc.begin() != p)
      return format_error::invalid_format;
    ++p;
  }
  return format_error::none;
}

}

}
