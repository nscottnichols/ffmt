// Focused memory-formatting regressions. fmt is a separate host oracle.
#include <ffmt/base.hpp>
#include <fmt/format.h>
#include <fmt/xchar.h>
#include <array>
#include <cassert>
#include <cstring>
#include <iostream>
#include <limits>
#include <string>

struct point {
  int x, y;
};
template <> struct ffmt::formatter<point> {
  static constexpr auto format(point p) {
    return ffmt::formatted<ffmt::detail::fixed_string{"({1}, {0})"}, int, int>{
        {p.x, p.y}};
  }
};
struct parsed {
  int value;
};
template <typename Char>
struct ffmt::formatter<parsed, Char> : ffmt::formatter<int, Char> {
  template <typename Context>
  FMT_HD auto format(const parsed &p, Context &ctx) const {
    return ffmt::formatter<int, Char>::format(p.value, ctx);
  }
};
struct mapped {
  unsigned value;
};
constexpr auto format_as(mapped v) { return v.value; }

static std::size_t checks = 0;
template <typename... Args>
void compare(ffmt::string_view pattern, const Args &...args) {
  auto expected = fmt::format(
      fmt::runtime(fmt::string_view{pattern.data(), pattern.size()}), args...);
  std::string actual(expected.size() + 8, '~');
  auto result = ffmt::format_to_n(actual.data(), expected.size(),
                                  ffmt::runtime(pattern), args...);
  if (result.error != ffmt::format_error::none ||
      result.required != expected.size() || result.written != expected.size() ||
      actual.substr(0, result.written) != expected) {
    std::cerr << "mismatch " << std::string(pattern) << " expected=" << expected
              << " actual=" << actual.substr(0, result.written)
              << " error=" << int(result.error)
              << " required=" << result.required << '\n';
    std::abort();
  }
  assert(actual.substr(expected.size()) == std::string(8, '~'));
  for (std::size_t bound :
       {std::size_t(0), std::size_t(1), expected.size() / 2}) {
    actual.assign(expected.size() + 8, '~');
    auto bounded = ffmt::format_to_n(actual.data(), bound,
                                     ffmt::runtime(pattern), args...);
    assert(bounded.error == ffmt::format_error::none &&
           bounded.required == expected.size() && bounded.written == bound);
    assert(actual.substr(0, bound) == expected.substr(0, bound));
    assert(actual[bound] == '~');
  }
  ++checks;
}
struct unsupported_type {};
static_assert(!ffmt::has_formatter<unsupported_type>);
static_assert(ffmt::has_formatter<int>);
static_assert(ffmt::has_formatter<point>);
static_assert(ffmt::has_formatter<parsed>);
static_assert(ffmt::has_formatter<mapped>);
int main() {
  compare("a{}b", char(0));
  compare("{0} {0} {1}", 42, "hello");
  compare("[{0:>{1}}]", 42, 400);
  compare("{:.180f}", 0.125);
  compare("{:.2000f}", 0.125);
  compare("{:.2000e}", 0.125);
  compare("{:#.2000g}", 0.125);
  compare("{:.0f} {:.0f} {:.0f} {:.0f}", 0.5, 1.5, 2.5, 9.5);
  compare("{} {}", std::numeric_limits<double>::min(),
          std::numeric_limits<double>::denorm_min());
  compare("[{:.3}] [{:─^9}]", "静夜思", "é");
  compare("{:?}", "a\n\t\"\\\x7f");
  compare("{:?}", '\n');
  compare("{:.3?}", "a\nb");
  compare("{:.2?}", "界");
  compare("{:.3?}", "界");
  compare("{:.4?}", "界");
  compare("{:.2?}", "😀");
  compare("{}", ffmt::string_view{"x\0y", 3});
  []<std::size_t... I>(std::index_sequence<I...>) {
    compare("{0:>{139}}", int(I)...);
  }(std::make_index_sequence<140>{});
  std::string long_pattern(513, 'x');
  long_pattern += "{0}";
  for (int i = 0; i < 32; ++i)
    long_pattern += "{0}";
  compare(long_pattern, 123);
  std::uint64_t state = 0x4d595df4d0f33173ull;
  for (unsigned i = 0; i < 4096; ++i) {
    state ^= state << 13;
    state ^= state >> 7;
    state ^= state << 17;
    auto d = __builtin_bit_cast(double, state);
    auto f = __builtin_bit_cast(float, static_cast<std::uint32_t>(state));
    compare("{}", d);
    compare("{}", f);
    for (const char *spec :
         {"{:.0f}", "{:.6f}", "{:.17g}", "{:.5e}", "{:#020.5a}"}) {
      compare(spec, d);
      compare(spec, f);
    }
    compare("{:#020x} {:#070b} {:o}", state, state, state);
    compare("{}", static_cast<std::int64_t>(state));
  }
  compare("{:>{}}", 42, mapped{7});
  char out[32];
  auto mapped_width = ffmt::format_to(out, "{:>{}}", 42, mapped{7});
  assert(mapped_width && std::string(out, mapped_width.written) == "     42");
  auto legacy = ffmt::format_to(out, "{}", point{2, 7});
  assert(std::string(out, legacy.written) == "(7, 2)");
  auto custom = ffmt::format_to(out, "{:04x}", parsed{42});
  assert(std::string(out, custom.written) == "002a");
  auto map = ffmt::format_to(out, "{:#x}", mapped{42});
  assert(std::string(out, map.written) == "0x2a");
  auto embedded = ffmt::format_to(out, "a\0{}", 7);
  assert(embedded.written == 3 && out[0] == 'a' && out[1] == 0 &&
         out[2] == '7');
  auto large = ffmt::format_to_n(out, 2, "{:100000}", 7);
  assert(large.error == ffmt::format_error::none && large.required == 100000 &&
         large.written == 2);
  auto huge = ffmt::format_to_n(out, 2, "{:#.2147483647g}", 0.125);
  assert(huge.error == ffmt::format_error::none &&
         huge.required == 2147483649ull && huge.written == 2);
  auto nullstring = ffmt::format_to_n(out, sizeof out, ffmt::runtime("{}"),
                                      static_cast<const char *>(nullptr));
  assert(nullstring.error == ffmt::format_error::invalid_format);
  for (const char *bad : {"{", "}", "{} {}", "{0} {}", "{0:!}", "{99}",
                          "{999999999999999999999}"}) {
    auto error = ffmt::format_to_n(out, sizeof out, ffmt::runtime(bad), 42);
    assert(error.error != ffmt::format_error::none);
  }
  auto dynamic_negative =
      ffmt::format_to_n(out, sizeof out, ffmt::runtime("{:{}}"), 42, -1);
  assert(dynamic_negative.error == ffmt::format_error::invalid_dynamic_spec);
  auto dynamic_float =
      ffmt::format_to_n(out, sizeof out, ffmt::runtime("{:{}}"), 42, 1.5);
  assert(dynamic_float.error == ffmt::format_error::invalid_dynamic_spec);
  wchar_t wide[128];
  auto wr =
      ffmt::format_to(wide, L"[{:>8}] {:.3f} {}", L"静夜", 1.25, point{2, 7});
  auto we = fmt::format(L"[{:>8}] {:.3f} ({}, {})", L"静夜", 1.25, 7, 2);
  assert(wr.error == ffmt::format_error::none &&
         std::wstring(wide, wr.written) == we);
  auto wc = ffmt::format_to(wide, L"{} {:d}", 'a', 'a');
  assert(wc && std::wstring(wide, wc.written) == L"a 97");
  std::cout << "engine checks: " << checks << '\n';
}
