#ifdef NDEBUG
#undef NDEBUG
#endif
#include <array>
#include <cassert>
#include <cstdio>
#include <ffmt/std23.hpp>
#include <fmt/std.h>
#include <string>

#if defined(__cpp_lib_expected) && __cpp_lib_expected >= 202202L
struct unformattable {};
static_assert(!ffmt::is_formattable<std::expected<unformattable, int>>);
static_assert(!ffmt::is_formattable<std::expected<void, unformattable>>);
template <typename T> void check(const T &value) {
  auto expected = fmt::format("{}", value);
  std::array<char, 256> buffer{};
  auto result = ffmt::format_to_n(buffer.data(), buffer.size(), "{}", value);
  assert(result.error == ffmt::format_error::none);
  assert(std::string(buffer.data(), result.written) == expected);
}
int main() {
  check(std::expected<void, int>{});
  check(std::expected<int, int>{42});
  check(std::expected<const char *, int>{"a\nb"});
  check(std::expected<char, int>{'\n'});
  check(std::expected<int, const char *>{std::unexpected("a\nb")});
  check(std::unexpected<char>{'\n'});
  std::puts("std23: 6 expected/unexpected differential cases passed");
}
#else
int main() {
  std::puts("UNSUPPORTED: the selected standard library has no C++23 expected");
  return 77;
}
#endif
