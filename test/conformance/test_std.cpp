// Standard-type adapters; independent of ranges, chrono, styling and names.
#include "family_test_support.hpp"
#include <ffmt/std.hpp>
#include <fmt/std.h>
struct unformattable {};
struct legacy_point {
  int x, y;
};
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

namespace {
ffmt_test::differential_suite suite("std");
void optional_and_variant() {
  suite.check("{}", std::optional<int>{});
  suite.check("{}", std::optional<int>{42});
  suite.check("{}", std::optional<const char *>{"a\nb"});
  suite.check("{}", std::optional<char>{'\n'});
  suite.check("{:.2f}", std::optional<double>{1.234});
  suite.check("{}", std::optional<std::optional<const char *>>{{"nested"}});
  suite.check("{}", std::optional<legacy_point>{{1, 2}});
  suite.check("{}", std::optional<mapped::label>{mapped::label::first});
  suite.check("{}", std::variant<int, const char *>{"a\nb"});
  suite.check("{}", std::variant<std::monostate, int>{});
}

void bitsets_and_scalar_wrappers() {
  suite.check("{}", std::bitset<10>(77));
  std::bitset<10> proxy_bits(77);
  suite.check("{}", proxy_bits[2]);
  suite.check("{:*>20}", std::bitset<10>(77));
  suite.check("{:*>20.4}", std::bitset<10>(77));
  suite.check("{:>20?}", std::bitset<10>(77));
  suite.check("{:.4?}", std::bitset<10>(77));
  suite.check("{:#04x}", std::byte{42});
  int ref = 42;
  suite.check("{:04}", std::cref(ref));
  std::atomic<int> atomic{42};
  suite.check("{:04}", atomic);
  std::atomic_flag atomic_flag{};
  atomic_flag.test_and_set();
  suite.check("{}", atomic_flag);
}

void source_and_pointer_views() {
  int ref = 42;
  auto location = std::source_location::current();
  suite.check("{}", location);
  suite.equal_expected("{}", fmt::format("{}", fmt::ptr(&ref)),
                       ffmt::ptr(&ref));
  std::unique_ptr<int> unique;
  suite.equal_expected("{}", fmt::format("{}", fmt::ptr(unique)),
                       ffmt::ptr(unique));
  std::shared_ptr<int> shared;
  suite.equal_expected("{}", fmt::format("{}", fmt::ptr(shared)),
                       ffmt::ptr(shared));
}

void complex_numbers() {
  suite.check("{}", std::complex<double>{1, 2.2});
  suite.check("{}", std::complex<unsigned>{1, 2});
  suite.check("{:08}", std::complex<double>{1, 2});
  suite.check("{:+08}", std::complex<double>{1, 2});
  suite.check("{:<08}", std::complex<double>{1, 2});
  suite.check("{:+}", std::complex<double>{0, -2.2});
  suite.check("{:*>25.2f}", std::complex<double>{1, -2.2});
  suite.check("{:<{}.{}f}", std::complex<double>{1, -2.2}, 25, 2);
  suite.check(
      "{}", std::complex<double>{1, std::numeric_limits<double>::quiet_NaN()});
}

void invalid_specs() {
  suite.reject("{:+}", std::optional<const char *>{"text"});
  suite.reject("{:x}", std::variant<int, const char *>{1});
  suite.reject("{:d}", std::complex<double>{1, 2});
}

} // namespace
int main() {
  static_assert(!ffmt::is_formattable<unformattable>);
  static_assert(!ffmt::is_formattable<std::variant<unformattable, int>>);
  static_assert(!fmt::is_formattable<std::variant<unformattable, int>>::value);
  optional_and_variant();
  bitsets_and_scalar_wrappers();
  source_and_pointer_views();
  complex_numbers();
  invalid_specs();
  suite.report();
}
