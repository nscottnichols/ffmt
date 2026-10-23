// Ranges, tuples and joins; independent of named, chrono, std and color
// adapters.
#define FMT_TUPLE_JOIN_SPECIFIERS 1
#include "family_test_support.hpp"
#include <array>
#include <ffmt/ranges.hpp>
#include <fmt/ranges.h>
#include <list>
#include <map>
#include <set>
#include <span>
#include <tuple>
#include <vector>
struct legacy_point {
  int x, y;
};
struct char_sequence : std::array<char, 4> {};
struct debug_sequence : std::array<char, 4> {};
template <>
struct ffmt::range_format_kind<char_sequence, char>
    : std::integral_constant<ffmt::range_format, ffmt::range_format::string> {};
template <>
struct ffmt::range_format_kind<debug_sequence, char>
    : std::integral_constant<ffmt::range_format,
                             ffmt::range_format::debug_string> {};
template <>
struct fmt::range_format_kind<char_sequence, char>
    : std::integral_constant<fmt::range_format, fmt::range_format::string> {};
template <>
struct fmt::range_format_kind<debug_sequence, char>
    : std::integral_constant<fmt::range_format,
                             fmt::range_format::debug_string> {};
template <> struct ffmt::formatter<legacy_point> {
  static auto format(legacy_point point) {
    return ffmt::formatted<"({}, {})", int, int>{{point.x, point.y}};
  }
};
template <> struct fmt::formatter<legacy_point> {
  constexpr auto parse(fmt::parse_context<> &ctx) { return ctx.begin(); }
  template <typename Context>
  auto format(legacy_point point, Context &ctx) const {
    return fmt::format_to(ctx.out(), "({}, {})", point.x, point.y);
  }
};
namespace mapped {
enum class label { first };
constexpr auto format_as(label) -> std::string_view { return "a\nb"; }
} // namespace mapped
struct custom_spec {
  int value;
};
template <> struct ffmt::formatter<custom_spec> {
  bool hex = false;
  constexpr auto parse(ffmt::format_parse_context &ctx) {
    auto it = ctx.begin();
    if (it != ctx.end() && *it == 'x') {
      hex = true;
      ++it;
    }
    return it;
  }
  template <typename Context>
  auto format(custom_spec value, Context &ctx) const {
    return hex ? ffmt::format_to(ctx.out(), "{:04x}", value.value).out
               : ffmt::format_to(ctx.out(), "{}", value.value).out;
  }
};
template <> struct fmt::formatter<custom_spec> {
  bool hex = false;
  constexpr auto parse(fmt::parse_context<> &ctx) {
    auto it = ctx.begin();
    if (it != ctx.end() && *it == 'x') {
      hex = true;
      ++it;
    }
    return it;
  }
  template <typename Context>
  auto format(custom_spec value, Context &ctx) const {
    return hex ? fmt::format_to(ctx.out(), "{:04x}", value.value)
               : fmt::format_to(ctx.out(), "{}", value.value);
  }
};

namespace {
ffmt_test::differential_suite suite("ranges");
void sequences_and_nested_specs() {
  std::array<int, 3> numbers{1, -2, 33};
  suite.check("{}", numbers);
  suite.check("{:n}", numbers);
  suite.check("{:*>30:04}", numbers);
  suite.check("{::>5}", numbers);
  suite.check("{0:*>{1}:03}", numbers, 30);
  suite.check("{}", std::array<std::array<int, 2>, 2>{{{1, 2}, {3, 4}}});
  suite.check("{}", std::array<legacy_point, 2>{{{1, 2}, {3, 4}}});
  suite.check("{::x}", std::array<custom_spec, 2>{{{31}, {255}}});
  suite.check("{}", std::array<mapped::label, 1>{mapped::label::first});
  suite.check("{}", std::array<const char *, 3>{"one", "a\nb", "静夜思"});
  suite.check("{}", std::vector<std::string>{"a\nb", "\"quoted\"", "emoji🦊"});
  suite.check("{:*>30}", std::vector<std::string>{"静夜思", "é"});
  suite.check("{}", std::list<int>{1, 2, 3});
  suite.check("{}", std::span<const int>(numbers));
  suite.check("{}",
              std::stack<int, std::vector<int>>(std::vector<int>{1, 2, 3}));
  suite.check("{:n}", std::queue<int>(std::deque<int>{1, 2, 3}));
  suite.check("{::03}", std::priority_queue<int>(std::less<int>{},
                                                 std::vector<int>{3, 1, 2}));
}

void associative_ranges_and_tuples() {
  suite.check("{}", std::set<int>{1, 2, 3});
  suite.check("{}", std::map<std::string, int>{{"a", 1}, {"b", 2}});
  suite.check("{:n}", std::map<std::string, int>{{"a", 1}});
  suite.check("{}", std::tuple{42, 1.5, "text", '\n'});
  suite.check("{:n}", std::tuple{42, "text", '\n'});
  suite.check("{:*>30}", std::tuple{42, "text"});
  suite.check("{}", std::tuple{});
  suite.check("{}", std::pair{1, 2});
  suite.check("{}", std::array<char, 3>{'a', '\n', '"'});
  suite.check("{:s}", std::array<char, 3>{'a', '\n', '"'});
  suite.check("{:?s}", std::array<char, 3>{'a', '\n', '"'});
  suite.check("{:?s}", std::list<char>{'a', '\n', '"'});
  suite.check("{:?s}",
              std::list<char>{char(0xe9), char(0x9d), char(0x99), char(0xff)});
}

void string_range_customization() {
  suite.check("{}", char_sequence{{{'a', 'b', 'c', 'd'}}});
  suite.check("{:.2}", char_sequence{{{'a', 'b', 'c', 'd'}}});
  suite.check("{}", debug_sequence{{{'a', '\n', '\\', '"'}}});
  suite.check("{:>12}", debug_sequence{{{'a', '\n', '\\', '"'}}});
}

void join_views() {
  std::array<int, 3> numbers{1, -2, 33};
  auto joined = ffmt::join(numbers, " | ");
  suite.equal_expected("{:04}", fmt::format("{:04}", fmt::join(numbers, " | ")),
                       joined);
  suite.equal_expected("{}",
                       fmt::format("{}", fmt::join(std::array{1, 2, 33}, ",")),
                       ffmt::join(std::array{1, 2, 33}, ","));
  auto t = std::tuple{1, -2, 3L};
  suite.equal_expected("{:+04}", fmt::format("{:+04}", fmt::join(t, ",")),
                       ffmt::join(t, ","));
}

void wide_ranges_and_tuples() {
  suite.check_wide(L"{}", std::array<int, 3>{1, 2, 3});
  suite.check_wide(L"{}", std::tuple{42, L"a\nb"});
}

void invalid_specs() {
  std::array<int, 3> numbers{1, -2, 33};
  suite.reject("{:s}", numbers);
  suite.reject("{::x}", std::array<const char *, 1>{"text"});
  suite.reject("{:d}", std::tuple{1, 2});
  suite.reject("{::03}", std::map<int, int>{{1, 2}});
  suite.reject("{:n}", debug_sequence{{{'a', 'b', 'c', 'd'}}});
}

void large_bounded_prefix() {
  std::array<int, 200> large{};
  for (std::size_t i = 0; i < large.size(); ++i)
    large[i] = static_cast<int>(i);
  auto expected = fmt::format("{::04}", large);
  std::array<char, 40> bound;
  bound.fill('!');
  auto bounded = ffmt::format_to_n(bound.data() + 1, 32, "{::04}", large);
  assert(bounded.error == ffmt::format_error::none && bounded.written == 32 &&
         bounded.required == expected.size());
  assert(std::string(bound.data() + 1, 32) == expected.substr(0, 32));
  assert(bound.front() == '!' && bound[33] == '!');
  suite.record();
}

} // namespace
int main() {
  sequences_and_nested_specs();
  associative_ranges_and_tuples();
  string_range_customization();
  join_views();
  wide_ranges_and_tuples();
  invalid_specs();
  large_bounded_prefix();
  suite.report();
}
