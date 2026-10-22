// Differential regressions against the pinned, independently compiled fmt.
#ifdef NDEBUG
#undef NDEBUG
#endif
#define FMT_TUPLE_JOIN_SPECIFIERS 1
#include <ffmt/args.hpp>
#include <ffmt/chrono.hpp>
#include <ffmt/color.hpp>
#include <ffmt/enum.hpp>
#include <ffmt/ranges.hpp>
#include <ffmt/std.hpp>
#include <fmt/chrono.h>
#include <fmt/color.h>
#include <fmt/ranges.h>
#include <fmt/std.h>
#include <fmt/xchar.h>
#include <array>
#include <cassert>
#include <cstdio>
#include <cstring>
#include <list>
#include <map>
#include <set>
#include <span>
#include <string>
#include <tuple>
#include <vector>

struct legacy_point { int x, y; };
struct unformattable {};
struct char_sequence : std::array<char, 4> {};
struct debug_sequence : std::array<char, 4> {};
template <> struct ffmt::range_format_kind<char_sequence, char>
    : std::integral_constant<ffmt::range_format, ffmt::range_format::string> {};
template <> struct ffmt::range_format_kind<debug_sequence, char>
    : std::integral_constant<ffmt::range_format, ffmt::range_format::debug_string> {};
template <> struct fmt::range_format_kind<char_sequence, char>
    : std::integral_constant<fmt::range_format, fmt::range_format::string> {};
template <> struct fmt::range_format_kind<debug_sequence, char>
    : std::integral_constant<fmt::range_format, fmt::range_format::debug_string> {};
template <> struct ffmt::formatter<legacy_point> {
  static auto format(legacy_point point) {
    return ffmt::formatted<"({}, {})", int, int>{{point.x, point.y}};
  }
};
template <> struct fmt::formatter<legacy_point> {
  constexpr auto parse(fmt::parse_context<>& ctx) { return ctx.begin(); }
  template <typename Context> auto format(legacy_point point, Context& ctx) const {
    return fmt::format_to(ctx.out(), "({}, {})", point.x, point.y);
  }
};
namespace mapped {
enum class label { first };
constexpr auto format_as(label) -> std::string_view { return "a\nb"; }
}
namespace enum_example {
using ffmt::enums::format_as;
enum class status : int { warning = -3, success = 42 };
}
struct custom_spec { int value; };
struct named_value { const char* name; int value; };
template <> struct ffmt::formatter<named_value> {
  constexpr auto parse(ffmt::format_parse_context& ctx) { return ctx.begin(); }
  template <typename Context> auto format(const named_value& item, Context& ctx) const {
    return ffmt::format_to(ctx.out(), "{}={}", item.name, item.value).out;
  }
};
template <> struct ffmt::formatter<custom_spec> {
  bool hex = false;
  constexpr auto parse(ffmt::format_parse_context& ctx) {
    auto it = ctx.begin();
    if (it != ctx.end() && *it == 'x') { hex = true; ++it; }
    return it;
  }
  template <typename Context> auto format(custom_spec value, Context& ctx) const {
    return hex ? ffmt::format_to(ctx.out(), "{:04x}", value.value).out :
                 ffmt::format_to(ctx.out(), "{}", value.value).out;
  }
};
template <> struct fmt::formatter<custom_spec> {
  bool hex = false;
  constexpr auto parse(fmt::parse_context<>& ctx) {
    auto it = ctx.begin();
    if (it != ctx.end() && *it == 'x') { hex = true; ++it; }
    return it;
  }
  template <typename Context> auto format(custom_spec value, Context& ctx) const {
    return hex ? fmt::format_to(ctx.out(), "{:04x}", value.value) :
                 fmt::format_to(ctx.out(), "{}", value.value);
  }
};

namespace {
int count = 0;
template <typename T> const T& oracle_arg(const T& value) { return value; }
auto oracle_arg(std::chrono::year value) { return fmt::year(int(value)); }
auto oracle_arg(std::chrono::month value) { return fmt::month(unsigned(value)); }
auto oracle_arg(std::chrono::day value) { return fmt::day(unsigned(value)); }
auto oracle_arg(std::chrono::weekday value) { return fmt::weekday(value.c_encoding()); }
auto oracle_arg(std::chrono::year_month_day value) {
  return fmt::year_month_day(fmt::year(int(value.year())), fmt::month(unsigned(value.month())), fmt::day(unsigned(value.day())));
}
template <typename Duration>
auto oracle_arg(std::chrono::local_time<Duration> value) { return fmt::local_time<Duration>(value.time_since_epoch()); }
template <typename... T>
void equal_expected(std::string_view pattern, std::string expected, const T&... args) {
  char buffer[16384];
  auto result = ffmt::format_to_n(buffer, sizeof(buffer), ffmt::runtime(pattern), args...);
  auto actual = std::string(buffer, result.written);
  if (result.error != ffmt::format_error::none || actual != expected || result.required != expected.size()) {
    std::fprintf(stderr, "family mismatch format=%.*s error=%d actual=%s expected=%s\n", int(pattern.size()), pattern.data(), int(result.error), actual.c_str(), expected.c_str());
    std::abort();
  }
  ++count;
}
template <typename... T>
void check(std::string_view pattern, const T&... args) {
  std::string expected;
  try { expected = fmt::format(fmt::runtime(pattern), oracle_arg(args)...); }
  catch (const fmt::format_error& error) {
    std::fprintf(stderr, "oracle rejected fixture format=%.*s: %s\n", int(pattern.size()), pattern.data(), error.what());
    std::abort();
  }
  equal_expected(pattern, expected, args...);
}
template <typename... T>
void check_wide(std::wstring_view pattern, const T&... args) {
  wchar_t buffer[2048];
  auto expected = fmt::format(fmt::runtime(pattern), oracle_arg(args)...);
  auto result = ffmt::format_to_n(buffer, std::size(buffer), ffmt::runtime(pattern), args...);
  assert(result.error == ffmt::format_error::none);
  assert(std::wstring(buffer, result.written) == expected);
  ++count;
}
template <typename... T>
void reject(std::string_view pattern, const T&... args) {
  bool rejected = false;
  try { (void)fmt::format(fmt::runtime(pattern), oracle_arg(args)...); }
  catch (const fmt::format_error&) { rejected = true; }
  if (!rejected) {
    std::fprintf(stderr, "oracle accepted negative fixture format=%.*s\n", int(pattern.size()), pattern.data());
    std::abort();
  }
  char buffer[1024];
  assert(ffmt::format_to_n(buffer, sizeof(buffer), ffmt::runtime(pattern), args...).error != ffmt::format_error::none);
  ++count;
}
}

int main() {
  static_assert(!ffmt::is_formattable<unformattable>);
  static_assert(!ffmt::is_formattable<std::variant<unformattable, int>>);
  static_assert(!fmt::is_formattable<std::variant<unformattable, int>>::value);
  std::array<int, 3> numbers{1, -2, 33};
  equal_expected("{}", "ordinary=42", named_value{"ordinary", 42});
  equal_expected("{:+05}", fmt::format("{:+05}", fmt::enums::format_as(enum_example::status::warning)), enum_example::status::warning);
  equal_expected("{:#04x}", fmt::format("{:#04x}", fmt::underlying(enum_example::status::success)), ffmt::underlying(enum_example::status::success));
  check("{}", numbers);
  check("{:n}", numbers);
  check("{:*>30:04}", numbers);
  check("{::>5}", numbers);
  check("{0:*>{1}:03}", numbers, 30);
  check("{}", std::array<std::array<int, 2>, 2>{{{1, 2}, {3, 4}}});
  check("{}", std::array<legacy_point, 2>{{{1, 2}, {3, 4}}});
  check("{::x}", std::array<custom_spec, 2>{{{31}, {255}}});
  check("{}", std::array<mapped::label, 1>{mapped::label::first});
  check("{}", std::array<const char*, 3>{"one", "a\nb", "静夜思"});
  check("{}", std::vector<std::string>{"a\nb", "\"quoted\"", "emoji🦊"});
  check("{:*>30}", std::vector<std::string>{"静夜思", "é"});
  check("{}", std::list<int>{1, 2, 3});
  check("{}", std::span<const int>(numbers));
  check("{}", std::stack<int, std::vector<int>>(std::vector<int>{1, 2, 3}));
  check("{:n}", std::queue<int>(std::deque<int>{1, 2, 3}));
  check("{::03}", std::priority_queue<int>(std::less<int>{}, std::vector<int>{3, 1, 2}));
  check("{}", std::set<int>{1, 2, 3});
  check("{}", std::map<std::string, int>{{"a", 1}, {"b", 2}});
  check("{:n}", std::map<std::string, int>{{"a", 1}});
  check("{}", std::tuple{42, 1.5, "text", '\n'});
  check("{:n}", std::tuple{42, "text", '\n'});
  check("{:*>30}", std::tuple{42, "text"});
  check("{}", std::tuple{});
  check("{}", std::pair{1, 2});
  check("{}", std::array<char, 3>{'a', '\n', '"'});
  check("{:s}", std::array<char, 3>{'a', '\n', '"'});
  check("{:?s}", std::array<char, 3>{'a', '\n', '"'});
  check("{:?s}", std::list<char>{'a', '\n', '"'});
  check("{:?s}", std::list<char>{char(0xe9), char(0x9d), char(0x99), char(0xff)});
  check("{}", char_sequence{{{'a', 'b', 'c', 'd'}}});
  check("{:.2}", char_sequence{{{'a', 'b', 'c', 'd'}}});
  check("{}", debug_sequence{{{'a', '\n', '\\', '"'}}});
  check("{:>12}", debug_sequence{{{'a', '\n', '\\', '"'}}});
  auto joined = ffmt::join(numbers, " | ");
  equal_expected("{:04}", fmt::format("{:04}", fmt::join(numbers, " | ")), joined);
  equal_expected("{}", fmt::format("{}", fmt::join(std::array{1, 2, 33}, ",")), ffmt::join(std::array{1, 2, 33}, ","));
  auto t = std::tuple{1, -2, 3L};
  equal_expected("{:+04}", fmt::format("{:+04}", fmt::join(t, ",")), ffmt::join(t, ","));
  check("{}", std::optional<int>{});
  check("{}", std::optional<int>{42});
  check("{}", std::optional<const char*>{"a\nb"});
  check("{}", std::optional<char>{'\n'});
  check("{:.2f}", std::optional<double>{1.234});
  check("{}", std::optional<std::optional<const char*>>{{"nested"}});
  check("{}", std::optional<legacy_point>{{1, 2}});
  check("{}", std::optional<mapped::label>{mapped::label::first});
  check("{}", std::variant<int, const char*>{"a\nb"});
  check("{}", std::variant<std::monostate, int>{});
  check("{}", std::bitset<10>(77));
  std::bitset<10> proxy_bits(77);
  check("{}", proxy_bits[2]);
  check("{:*>20}", std::bitset<10>(77));
  check("{:*>20.4}", std::bitset<10>(77));
  check("{:>20?}", std::bitset<10>(77));
  check("{:.4?}", std::bitset<10>(77));
  check("{:#04x}", std::byte{42});
  int ref = 42;
  check("{:04}", std::cref(ref));
  std::atomic<int> atomic{42};
  check("{:04}", atomic);
  std::atomic_flag atomic_flag{}; atomic_flag.test_and_set();
  check("{}", atomic_flag);
  auto location = std::source_location::current();
  check("{}", location);
  equal_expected("{}", fmt::format("{}", fmt::ptr(&ref)), ffmt::ptr(&ref));
  std::unique_ptr<int> unique;
  equal_expected("{}", fmt::format("{}", fmt::ptr(unique)), ffmt::ptr(unique));
  std::shared_ptr<int> shared;
  equal_expected("{}", fmt::format("{}", fmt::ptr(shared)), ffmt::ptr(shared));
  check("{}", std::complex<double>{1, 2.2});
  check("{}", std::complex<unsigned>{1, 2});
  check("{:08}", std::complex<double>{1, 2});
  check("{:+08}", std::complex<double>{1, 2});
  check("{:<08}", std::complex<double>{1, 2});
  check("{:+}", std::complex<double>{0, -2.2});
  check("{:*>25.2f}", std::complex<double>{1, -2.2});
  check("{:<{}.{}f}", std::complex<double>{1, -2.2}, 25, 2);
  check("{}", std::complex<double>{1, std::numeric_limits<double>::quiet_NaN()});
  auto style = ffmt::fg(ffmt::rgb(255, 20, 30)) | ffmt::bg(ffmt::terminal_color::bright_blue) | ffmt::emphasis::bold | ffmt::emphasis::underline;
  auto oracle_style = fmt::fg(fmt::rgb(255, 20, 30)) | fmt::bg(fmt::terminal_color::bright_blue) | fmt::emphasis::bold | fmt::emphasis::underline;
  equal_expected("{:>12}", fmt::format("{:>12}", fmt::styled("color", oracle_style)), ffmt::styled("color", style));
  equal_expected("{}", fmt::format("{}", fmt::styled(42, fmt::text_style{})), ffmt::styled(42, ffmt::text_style{}));
  equal_expected("{}", fmt::format("{}", fmt::styled("palette", fmt::fg(fmt::color::rebecca_purple))), ffmt::styled("palette", ffmt::fg(ffmt::color::rebecca_purple)));
  equal_expected("{value:04} {width}", fmt::format("{value:04} {width}", fmt::arg("value", 42), fmt::arg("width", 7)), ffmt::arg("value", 42), ffmt::arg("width", 7));
  equal_expected("{values:*>{width}:03}", fmt::format("{values:*>{width}:03}", fmt::arg("values", numbers), fmt::arg("width", 25)), ffmt::arg("values", numbers), ffmt::arg("width", 25));
  auto complex = std::complex<double>{1, -2.2};
  equal_expected("{value:>{width}.2f}", fmt::format("{value:>{width}.2f}", fmt::arg("value", complex), fmt::arg("width", 25)), ffmt::arg("value", complex), ffmt::arg("width", 25));
  check("{}", std::chrono::seconds(42));
  check("{:>15}", std::chrono::microseconds(-123));
  check("{:%H:%M:%S}", std::chrono::seconds(-12345));
  check("{:%T}", std::chrono::milliseconds(-123456));
  check("{:%Q %q %R %j}", std::chrono::hours(73));
  check("{:%S}", std::chrono::duration<long long, std::ratio<1, 3>>{1});
  check("{}", std::chrono::duration<int, std::ratio<15, 4>>{42});
  check("{:.2}", std::chrono::duration<double, std::milli>{1.2345});
  check("{:%M:%S}", std::chrono::duration<double>{-61.25});
  check("{:.3%T}", std::chrono::duration<double, std::nano>{-99123456789});
  check("{:%S %r}", std::chrono::duration<double>{1.0});
  check("{:%S %r}", std::chrono::duration<double>{1.5});
  check("{:.2}", std::chrono::seconds(42));
  check("{:.3%S}", std::chrono::seconds(42));
  check("{:.2%S}", std::chrono::milliseconds(1234));
  for (double value : {-0.0, 0.0, 1.0, 1.5, -1.5, 12345.125, -12345.125,
      std::numeric_limits<double>::infinity(), -std::numeric_limits<double>::infinity(),
      std::numeric_limits<double>::quiet_NaN()})
    check("{:%Q %H %Q %M %S %q %R %T %r %p}", std::chrono::duration<double>{value});
  check("{:%_T %-R}", std::chrono::milliseconds{1234});
  check("{}", std::chrono::year{2024});
  check("{:>12%B}", std::chrono::month{1});
  check("{}", std::chrono::day{5});
  check("{}", std::chrono::weekday{6});
  auto date = std::chrono::year{2021}/1/1;
  check("{}", date);
  check("{:%a %A %j %G-W%V-%u %U %W}", date);
  check("{:%F %T %z %Z}", std::chrono::sys_seconds{});
  check("{}", std::chrono::sys_time<std::chrono::milliseconds>{std::chrono::milliseconds{-1234}});
  check("{}", std::chrono::sys_time<std::chrono::duration<double>>{std::chrono::duration<double>{-1.5}});
  check("{}", std::chrono::sys_time<std::chrono::duration<double>>{std::chrono::duration<double>{1.0}});
  check("{:%S %T %X %r}", std::chrono::sys_time<std::chrono::duration<double>>{std::chrono::duration<double>{1.125}});
  check("{}", std::chrono::local_time<std::chrono::milliseconds>{std::chrono::milliseconds{-61500}});
  for (int year : {1, 27, 999, 1969, 1970, 1975, 1977, 1999, 2000, 2021, 2024})
    for (unsigned month : {1u, 3u, 12u})
      for (unsigned day : {1u, 28u})
        check("{:%F %a %A %j %G-W%V-%u %U %W}", std::chrono::year(year)/month/day);
  std::tm tm{}; tm.tm_year = 124; tm.tm_mon = 1; tm.tm_mday = 29;
  tm.tm_hour = 23; tm.tm_min = 4; tm.tm_sec = 5; tm.tm_yday = 59; tm.tm_wday = 4;
  check("{:*>40%F %T %a %B %j %G-W%V}", tm);
  check("{:%EY %Oy %Om %Od %_H %-M %S %c %x %X %r}", tm);
  check_wide(L"{}", std::array<int, 3>{1, 2, 3});
  check_wide(L"{}", std::tuple{42, L"a\nb"});
  check_wide(L"{}", std::chrono::microseconds{123});
  check_wide(L"{:%F %T}", std::chrono::sys_seconds{});
  reject("{:s}", numbers);
  reject("{::x}", std::array<const char*, 1>{"text"});
  reject("{:d}", std::tuple{1, 2});
  reject("{::03}", std::map<int, int>{{1, 2}});
  reject("{:n}", debug_sequence{{{'a', 'b', 'c', 'd'}}});
  reject("{:%Y}", std::chrono::seconds(1));
  reject("{:.2%Q}", std::chrono::seconds(1));
  reject("{:%}", tm);
  reject("{:%Ea}", tm);
  reject("{:%Oa}", tm);
  reject("{:%0S}", tm);
  reject("{:literal}", tm);
  reject("{:%z}", std::chrono::local_time<std::chrono::seconds>{});
  reject("{:+}", std::optional<const char*>{"text"});
  reject("{:x}", std::variant<int, const char*>{1});
  reject("{:d}", std::complex<double>{1, 2});
  std::array<int, 200> large{};
  for (std::size_t i = 0; i < large.size(); ++i) large[i] = static_cast<int>(i);
  auto expected = fmt::format("{::04}", large);
  std::array<char, 40> bound; bound.fill('!');
  auto bounded = ffmt::format_to_n(bound.data() + 1, 32, "{::04}", large);
  assert(bounded.error == ffmt::format_error::none && bounded.written == 32 && bounded.required == expected.size());
  assert(std::string(bound.data() + 1, 32) == expected.substr(0, 32));
  assert(bound.front() == '!' && bound[33] == '!');
  ++count;
  std::printf("families: %d differential cases passed\n", count);
}
