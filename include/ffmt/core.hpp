// Shared, allocation-free format parser and output engine.
// This header is included by base.hpp after the numeric conversion helpers.
#pragma once

namespace ffmt {
template <typename Char> using basic_string_view = std::basic_string_view<Char>;
using string_view = basic_string_view<char>;
using wstring_view = basic_string_view<wchar_t>;
namespace detail {
// Host char_traits::length is not device-callable on all standard libraries.
// Volatile loads in the runtime branch prevent loop-idiom lowering to an
// unresolved device strlen symbol (including at optimization level zero).
template <typename Char>
FMT_HD constexpr std::size_t string_length(const Char *text) {
  std::size_t size = 0;
  if (std::is_constant_evaluated()) {
    while (text[size] != Char())
      ++size;
  } else {
    const volatile Char *input = text;
    while (input[size] != Char())
      ++size;
  }
  return size;
}
template <typename Char>
FMT_HD constexpr auto to_string_view(const Char *text)
    -> basic_string_view<Char> {
  return {text, string_length(text)};
}
} // namespace detail

enum class format_error {
  none,
  invalid_format,
  argument_out_of_range,
  incompatible_type,
  invalid_dynamic_spec,
  invalid_code_point,
  unsupported_type,
  transport_error,
  output_overflow
};
struct format_result {
  std::size_t written = 0;
  std::size_t required = 0;
  format_error error = format_error::none;
  FMT_HD constexpr explicit operator bool() const {
    return error == format_error::none;
  }
};
template <typename OutputIt> struct format_to_result : format_result {
  OutputIt out;
  FMT_HD constexpr operator OutputIt() const { return out; }
};
template <typename Char> struct runtime_format_string {
  basic_string_view<Char> str;
};
template <typename Char>
FMT_HD constexpr auto runtime(basic_string_view<Char> str) {
  return runtime_format_string<Char>{str};
}
template <typename Char> FMT_HD constexpr auto runtime(const Char *str) {
  return runtime(basic_string_view<Char>{str, detail::string_length(str)});
}

template <typename T, typename Char = char> struct formatter;
namespace detail {
template <std::size_t N, typename Char = char> struct fixed_string {
  Char data[N]{};
  FMT_HD constexpr fixed_string() {}
  FMT_HD constexpr fixed_string(const Char (&str)[N]) {
    for (std::size_t i = 0; i < N; ++i)
      data[i] = str[i];
  }
  FMT_HD constexpr Char operator[](std::size_t i) const { return data[i]; }
};
template <std::size_t N, typename Char>
fixed_string(const Char (&)[N]) -> fixed_string<N, Char>;
template <std::size_t N, typename Char>
consteval auto flen(const fixed_string<N, Char> &) {
  return N - 1;
}

struct index_state {
  int next = 0;
  bool automatic = false, manual = false;
  int available = -1;
  const bool *dynamic_types = nullptr;
};
struct empty_args {};
template <typename T> inline constexpr bool named_arg = false;
template <typename T> FMT_HD constexpr decltype(auto) unwrap(const T &x) {
  if constexpr (named_arg<T>)
    return (x.value);
  else
    return (x);
}
template <typename T>
using unwrapped_t =
    std::remove_cvref_t<decltype(unwrap(std::declval<const T &>()))>;
template <typename T> inline constexpr bool extended_integer = false;
#if defined(__SIZEOF_INT128__)
template <> inline constexpr bool extended_integer<__int128> = true;
template <> inline constexpr bool extended_integer<unsigned __int128> = true;
#endif
template <typename T> struct floating_traits {
  static constexpr bool value = std::is_floating_point_v<T>;
  static constexpr int precision = std::numeric_limits<T>::digits;
  static constexpr int decimal_digits = std::numeric_limits<T>::digits10;
};
#if defined(__SIZEOF_FLOAT128__) && !defined(__CUDACC__) &&                    \
    !defined(__HIPCC__) && !defined(SYCL_LANGUAGE_VERSION) &&                  \
    !FFMT_COMPILER_ACPP
#define FFMT_HAS_FLOAT128 1
template <> struct floating_traits<__float128> {
  static constexpr bool value = true;
  static constexpr int precision = 113, decimal_digits = 33;
};
#else
#define FFMT_HAS_FLOAT128 0
#endif
template <typename T>
inline constexpr bool floating = floating_traits<T>::value;
template <typename T>
inline constexpr bool integer = std::is_integral_v<T> || extended_integer<T>;
#if defined(__BITINT_MAXWIDTH__)
// Match fmt/std.h's native bit-precise integer surface (up to 64 bits).
template <int N> inline constexpr bool extended_integer<_BitInt(N)> = N <= 64;
template <int N>
inline constexpr bool extended_integer<unsigned _BitInt(N)> = N <= 64;
#endif
template <typename T> struct unsigned_type {
  using type = std::make_unsigned_t<T>;
};
#if defined(__SIZEOF_INT128__)
template <> struct unsigned_type<__int128> {
  using type = unsigned __int128;
};
template <> struct unsigned_type<unsigned __int128> {
  using type = unsigned __int128;
};
#endif
#if defined(__BITINT_MAXWIDTH__)
template <int N> struct unsigned_type<_BitInt(N)> {
  using type = unsigned _BitInt(N);
};
template <int N> struct unsigned_type<unsigned _BitInt(N)> {
  using type = unsigned _BitInt(N);
};
#endif
template <> struct unsigned_type<bool> {
  using type = unsigned;
};
template <typename T> using unsigned_t = typename unsigned_type<T>::type;
template <typename T>
inline constexpr bool signed_integer = [] {
  if constexpr (integer<T>)
    return T(-1) < T(0);
  else
    return false;
}();

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

// Counted views never read a decoder byte beyond the supplied input.
FMT_HD constexpr int decode_utf8(const char *s, std::size_t size,
                                 std::uint32_t &cp) {
  cp = invalid_code_point;
  if (!size)
    return 0;
  auto b = static_cast<unsigned char>(s[0]);
  if (b < 0x80) {
    cp = b;
    return 1;
  }
  int n = b >= 0xc2 && b <= 0xdf   ? 2
          : b >= 0xe0 && b <= 0xef ? 3
          : b >= 0xf0 && b <= 0xf4 ? 4
                                   : 1;
  if (n == 1 || static_cast<std::size_t>(n) > size)
    return 1;
  std::uint32_t v = b & (0x7f >> n);
  for (int j = 1; j < n; ++j) {
    auto t = static_cast<unsigned char>(s[j]);
    if ((t & 0xc0) != 0x80)
      return 1;
    v = (v << 6) | (t & 0x3f);
  }
  if (v < (n == 2   ? 0x80u
           : n == 3 ? 0x800u
                    : 0x10000u) ||
      (v >= 0xd800 && v <= 0xdfff) || v > 0x10ffff)
    return 1;
  cp = v;
  return n;
}
} // namespace detail

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
      : begin_(str.data()),
        end_(str.data() ? str.data() + str.size() : str.data()),
        state_(state ? state : &local_) {}
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
      else if (state_->dynamic_types && !state_->dynamic_types[id])
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
  if constexpr (sizeof(Char) == 1)
    if (units && static_cast<unsigned char>(*p) >= 0x80) {
      std::uint32_t cp;
      units = decode_utf8(p, static_cast<std::size_t>(end - p), cp);
      if (cp == invalid_code_point) {
        pc.on_error(format_error::invalid_format);
        pc.advance_to(p);
        return s;
      }
    }
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

template <typename T, typename Char>
inline constexpr bool character =
    std::is_same_v<T, Char> ||
    (std::is_same_v<T, char> && !std::is_same_v<Char, char>);
template <typename T, typename Char>
inline constexpr bool string_like =
    std::is_convertible_v<const T &, basic_string_view<Char>>;
template <typename T, typename Char>
inline constexpr bool char_pointer =
    std::is_pointer_v<T> &&
    std::is_same_v<std::remove_cv_t<std::remove_pointer_t<T>>, Char>;
template <typename T, typename Char>
FMT_HD constexpr bool compatible(const basic_format_spec<Char> &s) {
  constexpr bool ch = character<T, Char>;
  if constexpr ((std::is_same_v<T, wchar_t> || std::is_same_v<T, char16_t> ||
                 std::is_same_v<T, char32_t>) &&
                !std::is_same_v<T, Char>)
    return false;
  constexpr bool str = string_like<T, Char> || char_pointer<T, Char>;
  constexpr bool flt = floating<T>;
  bool num = (integer<T> && !ch) || flt ||
             (ch && s.type && s.type != 'c' && s.type != '?');
  if (s.sign && !(flt || (signed_integer<T> && !ch)))
    return false;
  if ((s.alt || s.zero_pad) && !num)
    return false;
  if ((s.precision >= 0 || s.precision_arg >= 0 || !s.precision_name.empty()) &&
      !(flt || str))
    return false;
  char t = s.type;
  if constexpr (str)
    return !t || t == 's' || t == '?' || t == 'p';
  if constexpr (std::is_pointer_v<T> || std::is_null_pointer_v<T>)
    return !t || t == 'p';
  if constexpr (std::is_same_v<T, bool>)
    return !t || t == 's' || t == 'd' || t == 'b' || t == 'B' || t == 'o' ||
           t == 'x' || t == 'X';
  if constexpr (integer<T>)
    return !t || t == 'd' || t == 'b' || t == 'B' || t == 'o' || t == 'x' ||
           t == 'X' || t == 'c' || (ch && t == '?');
  if constexpr (flt)
    return !t || t == 'f' || t == 'F' || t == 'e' || t == 'E' || t == 'g' ||
           t == 'G' || t == 'a' || t == 'A';
  return false;
}
template <typename T, typename Char>
inline constexpr bool primitive =
    integer<T> || floating<T> || std::is_pointer_v<T> ||
    std::is_null_pointer_v<T> || string_like<T, Char>;
template <typename T>
concept has_format_as = requires(const T &x) { format_as(x); };
template <typename T> FMT_HD constexpr decltype(auto) mapped_value(const T &x) {
  return format_as(x);
}
template <typename T, bool Mapped = has_format_as<T>> struct mapped_argument {
  using type = T;
};
template <typename T> struct mapped_argument<T, true> {
  using type =
      std::remove_cvref_t<decltype(mapped_value(std::declval<const T &>()))>;
};
template <typename T>
using mapped_argument_t = typename mapped_argument<T>::type;
template <typename T>
concept legacy_formatter = requires(const T &x) { formatter<T>::format(x); };

template <typename Context, typename T, typename Char>
FMT_HD void write_value(Context &, const T &, basic_format_spec<Char>);
} // namespace detail

// The primary formatter covers core types, ADL format_as, and the historical
// static format(T)->formatted protocol. Family headers specialize this class.
template <typename T, typename Char> struct formatter {
  using ffmt_primary_type = T;
  detail::basic_format_spec<Char> specs{};
  bool debug = false;
  FMT_HD constexpr void set_debug_format(bool enabled = true) {
    debug = enabled;
  }
  FMT_HD constexpr auto parse(basic_format_parse_context<Char> &pc)
      -> const Char * {
    specs = detail::parse_standard_specs(pc);
    if constexpr (detail::primitive<T, Char>) {
      if (!detail::compatible<T>(specs))
        pc.on_error(format_error::incompatible_type);
    } else if constexpr (detail::has_format_as<T>) {
      using U = std::remove_cvref_t<decltype(detail::mapped_value(
          std::declval<const T &>()))>;
      if constexpr (detail::primitive<U, Char>)
        if (!detail::compatible<U>(specs))
          pc.on_error(format_error::incompatible_type);
    } else
      pc.on_error(format_error::unsupported_type);
    return pc.begin();
  }
  template <typename Context>
  FMT_HD auto format(const T &value, Context &ctx) const ->
      typename Context::iterator {
    auto s = specs;
    if constexpr (detail::has_format_as<T>) {
      const auto &mapped = detail::mapped_value(value);
      using U = std::remove_cvref_t<decltype(mapped)>;
      if constexpr (detail::character<U, Char> ||
                    detail::string_like<U, Char> ||
                    detail::char_pointer<U, Char>)
        if (debug && !s.type)
          s.type = '?';
      detail::write_value(ctx, mapped, s);
    } else {
      if constexpr (detail::character<T, Char> ||
                    detail::string_like<T, Char> ||
                    detail::char_pointer<T, Char>)
        if (debug && !s.type)
          s.type = '?';
      detail::write_value(ctx, value, s);
    }
    return ctx.out();
  }
};

template <detail::fixed_string Fmt, typename... Args> struct formatted {
  static constexpr auto format_string = Fmt;
  std::tuple<Args...> values;
};
namespace detail {
template <typename T, typename Char> consteval bool formattable_type() {
  using U = std::decay_t<T>;
  using F = formatter<U, Char>;
  if constexpr (legacy_formatter<U>)
    return true;
  else if constexpr (requires { typename F::ffmt_primary_type; }) {
    if constexpr (std::is_same_v<typename F::ffmt_primary_type, U>) {
      if constexpr (primitive<U, Char>)
        return true;
      else if constexpr (has_format_as<U>) {
        using M = std::remove_cvref_t<decltype(mapped_value(
            std::declval<const U &>()))>;
        return primitive<M, Char>;
      } else
        return false;
    }
  }
  return requires(F &f, basic_format_parse_context<Char> &pc) { f.parse(pc); };
}
} // namespace detail
template <typename T, typename Char = char>
inline constexpr bool is_formattable = detail::formattable_type<T, Char>();
template <typename T>
concept has_formatter = is_formattable<T>;
template <typename T>
concept sycl_formattable = has_formatter<T>;

namespace detail {
template <typename Tuple, typename F, std::size_t... I>
FMT_HD constexpr bool visit_index_impl(const Tuple &args, int index, F &&f,
                                       std::index_sequence<I...>) {
  bool found = false;
  (((index == static_cast<int>(I))
        ? (f(unwrap(std::get<I>(args))), found = true, void())
        : void()),
   ...);
  return found;
}
template <typename Tuple, typename F>
FMT_HD constexpr bool visit_index(const Tuple &args, int index, F &&f) {
  if constexpr (std::is_same_v<Tuple, empty_args>)
    return false;
  else
    return visit_index_impl(
        args, index, static_cast<F &&>(f),
        std::make_index_sequence<std::tuple_size_v<Tuple>>{});
}
template <typename Char>
FMT_HD constexpr bool equal_text(basic_string_view<Char> a,
                                 basic_string_view<Char> b) {
  if (a.size() != b.size())
    return false;
  if (std::is_constant_evaluated()) {
    for (std::size_t i = 0; i < a.size(); ++i)
      if (a[i] != b[i])
        return false;
  } else {
    const volatile Char *x = a.data();
    const volatile Char *y = b.data();
    for (std::size_t i = 0; i < a.size(); ++i)
      if (x[i] != y[i])
        return false;
  }
  return true;
}
template <typename Tuple, typename Char, std::size_t... I>
FMT_HD constexpr int named_index_impl(const Tuple &args,
                                      basic_string_view<Char> name,
                                      std::index_sequence<I...>) {
  int result = -1;
  [[maybe_unused]] auto one = [&]<std::size_t N>() {
    const auto &arg = std::get<N>(args);
    if constexpr (named_arg<std::remove_cvref_t<decltype(arg)>>)
      if (equal_text(basic_string_view<Char>{arg.name}, name))
        result = static_cast<int>(N);
  };
  (one.template operator()<I>(), ...);
  return result;
}
template <typename Tuple, typename Char>
FMT_HD constexpr int named_index(const Tuple &args,
                                 basic_string_view<Char> name) {
  if constexpr (std::is_same_v<Tuple, empty_args>)
    return -1;
  else
    return named_index_impl(
        args, name, std::make_index_sequence<std::tuple_size_v<Tuple>>{});
}
} // namespace detail

template <typename OutputIt, typename Char,
          typename ArgsTuple = detail::empty_args>
class basic_format_context {
  OutputIt out_;
  format_error *error_;
  const ArgsTuple *args_;

public:
  using iterator = OutputIt;
  using char_type = Char;
  FMT_HD constexpr basic_format_context(OutputIt out, format_error &error,
                                        const ArgsTuple *args = nullptr)
      : out_(out), error_(&error), args_(args) {}
  FMT_HD constexpr auto out() const -> OutputIt { return out_; }
  FMT_HD constexpr void advance_to(OutputIt out) { out_ = out; }
  FMT_HD constexpr void on_error(format_error error) {
    if (*error_ == format_error::none)
      *error_ = error;
  }
  FMT_HD constexpr void on_error(const char *) {
    on_error(format_error::invalid_format);
  }
  FMT_HD constexpr auto error() const -> format_error { return *error_; }
  FMT_HD constexpr int arg_int(int index) {
    int result = 0;
    auto resolve = [&](const auto &x) {
      using T = std::remove_cvref_t<decltype(x)>;
      if constexpr (detail::integer<T> && !std::is_same_v<T, bool> &&
                    !detail::character<T, Char>) {
        if constexpr (detail::signed_integer<T>)
          if (x < 0) {
            on_error(format_error::invalid_dynamic_spec);
            return;
          }
        if (static_cast<detail::unsigned_t<T>>(x) >
            static_cast<detail::unsigned_t<T>>(INT_MAX))
          on_error(format_error::invalid_dynamic_spec);
        else
          result = static_cast<int>(x);
      } else
        on_error(format_error::invalid_dynamic_spec);
    };
    bool found =
        args_ && detail::visit_index(*args_, index, [&](const auto &value) {
          using T = std::remove_cvref_t<decltype(value)>;
          if constexpr (detail::has_format_as<T>)
            resolve(detail::mapped_value(value));
          else
            resolve(value);
        });
    if (!found)
      on_error(format_error::argument_out_of_range);
    return result;
  }
  FMT_HD constexpr int arg_int(basic_string_view<Char> name) {
    return arg_int(args_ ? detail::named_index(*args_, name) : -1);
  }
  template <typename It> FMT_HD constexpr auto rebind(It out) {
    return basic_format_context<It, Char, ArgsTuple>{out, *error_, args_};
  }
};

namespace detail {
template <typename Context>
FMT_HD constexpr void put(Context &ctx, typename Context::char_type c) {
  auto it = ctx.out();
  *it++ = c;
  ctx.advance_to(it);
}
template <typename Context>
FMT_HD void repeat(Context &ctx, typename Context::char_type c, std::size_t n) {
  auto it = ctx.out();
  if constexpr (requires { it.repeat(c, n); })
    it.repeat(c, n);
  else
    for (std::size_t i = 0; i < n; ++i)
      *it++ = c;
  ctx.advance_to(it);
}
template <typename Context, typename Char>
FMT_HD void append(Context &ctx, const Char *p, std::size_t n) {
  for (std::size_t i = 0; i < n; ++i)
    put(ctx, static_cast<typename Context::char_type>(p[i]));
}
template <typename Context> FMT_HD void ascii(Context &ctx, const char *p) {
  while (*p)
    put(ctx, static_cast<typename Context::char_type>(*p++));
}
template <typename Context, typename Char>
FMT_HD auto resolved(Context &ctx, basic_format_spec<Char> s) {
  if (s.width_arg >= 0)
    s.width = ctx.arg_int(s.width_arg);
  if (!s.width_name.empty())
    s.width = ctx.arg_int(s.width_name);
  if (s.precision_arg >= 0)
    s.precision = ctx.arg_int(s.precision_arg);
  if (!s.precision_name.empty())
    s.precision = ctx.arg_int(s.precision_name);
  return s;
}
template <typename Context, typename Char, typename Body>
FMT_HD void write_padded(Context &ctx, basic_format_spec<Char> specs,
                         std::size_t columns, Body body,
                         char default_align = '<') {
  auto s = resolved(ctx, specs);
  if (ctx.error() != format_error::none)
    return;
  std::size_t padding =
      s.width > 0 && static_cast<std::size_t>(s.width) > columns
          ? static_cast<std::size_t>(s.width) - columns
          : 0;
  char align = s.align ? s.align : default_align;
  std::size_t left = align == '>' ? padding : align == '^' ? padding / 2 : 0;
  auto fill = [&](std::size_t count) {
    if (s.fill_size == 1)
      repeat(ctx, s.fill[0], count);
    else
      for (std::size_t i = 0; i < count; ++i)
        append(ctx, s.fill, s.fill_size);
  };
  fill(left);
  body(ctx);
  fill(padding - left);
}

// An output iterator whose copies share a sink. It permits nested formatter
// calls to count bytes exactly without allocating or losing the outer count.
template <typename Sink> struct sink_iterator {
  using iterator_category = std::output_iterator_tag;
  using value_type = void;
  using difference_type = std::ptrdiff_t;
  using pointer = void;
  using reference = void;
  Sink *sink;
  FMT_HD auto operator*() const -> sink_iterator { return *this; }
  FMT_HD auto operator++() -> sink_iterator & { return *this; }
  FMT_HD auto operator++(int) -> sink_iterator { return *this; }
  FMT_HD auto operator=(typename Sink::char_type c) -> sink_iterator & {
    sink->push(c);
    return *this;
  }
  FMT_HD void repeat(typename Sink::char_type c, std::size_t n) {
    sink->repeat(c, n);
  }
};
template <typename OutputIt, typename Char> struct iterator_sink {
  using char_type = Char;
  OutputIt out;
  std::size_t limit, written = 0, required = 0;
  format_error error = format_error::none;
  FMT_HD void push(Char c) {
    if (required == std::numeric_limits<std::size_t>::max()) {
      error = format_error::output_overflow;
      return;
    }
    ++required;
    if (written < limit) {
      *out++ = c;
      ++written;
    }
  }
  FMT_HD void repeat(Char c, std::size_t count) {
    if (count > std::numeric_limits<std::size_t>::max() - required) {
      error = format_error::output_overflow;
      return;
    }
    required += count;
    auto amount = count < limit - written ? count : limit - written;
    for (std::size_t i = 0; i < amount; ++i)
      *out++ = c;
    written += amount;
  }
};

// Shared parser. A handler validates a selected argument at compile time, or
// writes it through the same formatter parse method at runtime.
template <typename Char, typename Handler>
FMT_HD constexpr format_error walk_format(basic_string_view<Char> text,
                                          Handler &&handle) {
  index_state state;
  if constexpr (requires { handle.initialize(state); })
    handle.initialize(state);
  auto p = text.data();
  auto end = p ? p + text.size() : p;
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

template <typename T, typename Char>
FMT_HD constexpr format_error
validate_one(basic_format_parse_context<Char> &pc) {
  if constexpr (legacy_formatter<T>) {
    if (pc.begin() != pc.end() && *pc.begin() != Char('}'))
      return format_error::invalid_format;
    return format_error::none;
  } else {
    formatter<T, Char> fmt;
    auto it = fmt.parse(pc);
    pc.advance_to(it);
    return pc.error();
  }
}
template <typename Char, typename... Args> struct validation_handler {
  inline static constexpr bool dynamic_types[sizeof...(Args) + 1] = {
      (integer<mapped_argument_t<unwrapped_t<Args>>> &&
       !std::is_same_v<mapped_argument_t<unwrapped_t<Args>>, bool> &&
       !character<mapped_argument_t<unwrapped_t<Args>>, Char>)...,
      false};
  FMT_HD constexpr void initialize(index_state &state) {
    state.available = sizeof...(Args);
    state.dynamic_types = dynamic_types;
  }
  FMT_HD constexpr void literal(Char) {}
  FMT_HD constexpr format_error argument(int index,
                                         basic_string_view<Char> name,
                                         basic_format_parse_context<Char> &pc) {
    if (!name.empty()) { // Named argument names are runtime values, not
                         // template arguments.
      constexpr bool named = (named_arg<std::remove_cvref_t<Args>> || ...);
      if constexpr (named) {
        pc.advance_to(pc.end() - 1);
        return format_error::none;
      }
      return format_error::argument_out_of_range;
    }
    format_error error = format_error::argument_out_of_range;
    int current = 0;
    ((index == current++ ? (error = validate_one<unwrapped_t<Args>>(pc), void())
                         : void()),
     ...);
    return error;
  }
};
namespace consteval_error {
[[noreturn]] FMT_HD inline void invalid_format_spec() { __builtin_trap(); }
} // namespace consteval_error
template <typename Char, typename... Args>
consteval void validate_literal(basic_string_view<Char> s) {
  validation_handler<Char, Args...> handler;
  if (walk_format(s, handler) != format_error::none)
    consteval_error::invalid_format_spec();
}
} // namespace detail

template <typename Char, typename... Args> struct basic_format_string {
  basic_string_view<Char> str;
  template <std::size_t N>
  consteval basic_format_string(const Char (&s)[N]) : str(s, N - 1) {
    detail::validate_literal<Char, Args...>(str);
  }
};
template <typename... Args>
using format_string = basic_format_string<char, std::type_identity_t<Args>...>;
template <typename... Args>
using wformat_string =
    basic_format_string<wchar_t, std::type_identity_t<Args>...>;
namespace detail {
template <typename... Args>
using print_string = basic_format_string<char, Args...>;
}

// The engine below is independent of the destination and shared by buffers,
// counting, host stdout, and accelerator stdout transports.
namespace detail {
template <typename Context, typename T, typename Char>
FMT_HD void write_nested(Context &, const T &, basic_string_view<Char>,
                         bool = false);
template <typename Context, typename T, typename Char>
FMT_HD void format_selected(Context &ctx, const T &value,
                            basic_format_parse_context<Char> &pc) {
  if constexpr (legacy_formatter<T>) {
    if (pc.begin() != pc.end() && *pc.begin() != Char('}')) {
      ctx.on_error(format_error::invalid_format);
      return;
    }
    auto inner = formatter<T>::format(value);
    constexpr auto text = decltype(inner)::format_string;
    std::apply(
        [&](const auto &...vals) {
          // Convert legacy narrow literals for wide destinations without a
          // buffer.
          if constexpr (std::is_same_v<Char, char>) {
            auto result = format_to(
                ctx.out(),
                runtime(basic_string_view<char>{text.data, flen(text)}),
                vals...);
            ctx.advance_to(result.out);
            ctx.on_error(result.error);
          } else {
            constexpr auto wide = [text]() {
              fixed_string<flen(text) + 1, Char> result;
              for (std::size_t i = 0; i <= flen(text); ++i)
                result.data[i] = Char(text.data[i]);
              return result;
            }();
            auto result = format_to(
                ctx.out(),
                runtime(basic_string_view<Char>{wide.data, flen(wide)}),
                vals...);
            ctx.advance_to(result.out);
            ctx.on_error(result.error);
          }
        },
        inner.values);
  } else {
    formatter<T, Char> fmt;
    auto next = fmt.parse(pc);
    pc.advance_to(next);
    if (pc.error() != format_error::none) {
      ctx.on_error(pc.error());
      return;
    }
    ctx.advance_to(fmt.format(value, ctx));
  }
}
// Adapt the original static formatter protocol for nested family formatters.
template <typename T, typename Char, bool Legacy = legacy_formatter<T>>
struct formatter_adapter : formatter<T, Char> {};
template <typename T, typename Char> struct formatter_adapter<T, Char, true> {
  FMT_HD constexpr void set_debug_format(bool = true) {}
  FMT_HD constexpr auto parse(basic_format_parse_context<Char> &pc)
      -> const Char * {
    if (pc.begin() != pc.end() && *pc.begin() != Char('}'))
      pc.on_error(format_error::invalid_format);
    return pc.begin();
  }
  template <typename Context>
  FMT_HD auto format(const T &value, Context &ctx) const ->
      typename Context::iterator {
    basic_format_parse_context<Char> pc{basic_string_view<Char>{}};
    format_selected(ctx, value, pc);
    return ctx.out();
  }
};
template <typename Context, typename Tuple> struct execution_handler {
  using Char = typename Context::char_type;
  Context &ctx;
  const Tuple &args;
  FMT_HD void literal(Char c) {
    if (ctx.error() == format_error::none)
      put(ctx, c);
  }
  FMT_HD format_error argument(int index, basic_string_view<Char> name,
                               basic_format_parse_context<Char> &pc) {
    if (!name.empty())
      index = named_index(args, name);
    bool found = visit_index(args, index, [&](const auto &value) {
      format_selected(ctx, value, pc);
    });
    return found ? ctx.error() : format_error::argument_out_of_range;
  }
};
template <typename Context, typename T, typename Char>
FMT_HD void write_nested(Context &ctx, const T &value,
                         basic_string_view<Char> specs, bool debug) {
  basic_format_parse_context<Char> pc{specs};
  if constexpr (legacy_formatter<T>)
    format_selected(ctx, value, pc);
  else {
    formatter<T, Char> fmt;
    if constexpr (requires { fmt.set_debug_format(debug); })
      fmt.set_debug_format(debug);
    auto p = fmt.parse(pc);
    if (p != pc.end() && *p != Char('}'))
      pc.on_error(format_error::invalid_format);
    if (pc.error() != format_error::none) {
      ctx.on_error(pc.error());
      return;
    }
    ctx.advance_to(fmt.format(value, ctx));
  }
}
template <typename Sink, typename Char, typename... Args>
FMT_HD void run_format(Sink &sink, basic_string_view<Char> text,
                       const Args &...args) {
  auto tuple = std::tuple<const Args &...>{args...};
  basic_format_context<sink_iterator<Sink>, Char, decltype(tuple)> ctx{
      {&sink}, sink.error, &tuple};
  execution_handler<decltype(ctx), decltype(tuple)> handler{ctx, tuple};
  auto error = walk_format(text, handler);
  ctx.on_error(error);
}
} // namespace detail

template <typename OutputIt, typename Char, typename... Args>
FMT_HD auto format_to_n(OutputIt out, std::size_t size,
                        runtime_format_string<Char> pattern,
                        const Args &...args) -> format_to_result<OutputIt> {
  detail::iterator_sink<OutputIt, Char> sink{out, size};
  detail::run_format(sink, pattern.str, args...);
  return {{sink.written, sink.required, sink.error}, sink.out};
}
template <typename OutputIt, typename... Args>
FMT_HD auto format_to_n(OutputIt out, std::size_t size,
                        format_string<Args...> pattern, const Args &...args)
    -> format_to_result<OutputIt> {
  return format_to_n(out, size, runtime(pattern.str), args...);
}
template <typename OutputIt, typename... Args>
FMT_HD auto format_to_n(OutputIt out, std::size_t size,
                        wformat_string<Args...> pattern, const Args &...args)
    -> format_to_result<OutputIt> {
  return format_to_n(out, size, runtime(pattern.str), args...);
}
template <typename OutputIt, typename Char, typename... Args>
FMT_HD auto format_to(OutputIt out, runtime_format_string<Char> pattern,
                      const Args &...args) -> format_to_result<OutputIt> {
  return format_to_n(out, std::numeric_limits<std::size_t>::max(), pattern,
                     args...);
}
template <typename OutputIt, typename... Args>
FMT_HD auto format_to(OutputIt out, format_string<Args...> pattern,
                      const Args &...args) -> format_to_result<OutputIt> {
  return format_to(out, runtime(pattern.str), args...);
}
template <typename OutputIt, typename... Args>
FMT_HD auto format_to(OutputIt out, wformat_string<Args...> pattern,
                      const Args &...args) -> format_to_result<OutputIt> {
  return format_to(out, runtime(pattern.str), args...);
}
template <typename Char, typename... Args>
FMT_HD auto formatted_size(runtime_format_string<Char> pattern,
                           const Args &...args) -> format_result {
  // A null destination is safe at zero bound; no null pointer arithmetic
  // occurs.
  return format_to_n(static_cast<Char *>(nullptr), 0, pattern, args...);
}
template <typename... Args>
FMT_HD auto formatted_size(format_string<Args...> pattern, const Args &...args)
    -> format_result {
  return formatted_size(runtime(pattern.str), args...);
}
template <typename... Args>
FMT_HD auto formatted_size(wformat_string<Args...> pattern, const Args &...args)
    -> format_result {
  return formatted_size(runtime(pattern.str), args...);
}
} // namespace ffmt

#include "detail/numeric.hpp"
#include "detail/text.hpp"
#include "detail/transport.hpp"
