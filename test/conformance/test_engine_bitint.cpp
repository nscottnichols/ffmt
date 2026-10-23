#include <cassert>
#include <cstdio>
#include <ffmt/detail/checked.hpp>
#include <fmt/std.h>
#include <string>
#if defined(__BITINT_MAXWIDTH__)
template <typename T> void check(T value) {
  for (const char *spec :
       {"{}", "{:b}", "{:#o}", "{:#020x}", "{:^30}", "{:d}"}) {
    auto expected = fmt::format(fmt::runtime(spec), value);
    char actual[256];
    auto result =
        ffmt::format_to_n(actual, sizeof actual, ffmt::runtime(spec), value);
    assert(result.error == ffmt::format_error::none &&
           result.required == expected.size() &&
           std::string(actual, result.written) == expected);
  }
}
template <int N> void width() {
  using S = _BitInt(N);
  using U = unsigned _BitInt(N);
  U high = U(1) << (N - 1);
  check(S(0));
  check(S(-1));
  check(S(high));
  check(S(high - 1));
  check(U(0));
  check(U(-1));
  check(high);
}
template <typename T> void unsupported_width() {
  static_assert(!ffmt::is_formattable<T>);
  char buffer[4] = {'!', '!', '!', '!'};
  auto result = ffmt::format_to_n(buffer + 1, 2, ffmt::runtime("{}"), T(1));
  assert(result.error == ffmt::format_error::unsupported_type &&
         result.written == 0);
  for (char c : buffer)
    assert(c == '!');
}
#endif
int main() {
#if defined(__BITINT_MAXWIDTH__)
  width<2>();
  width<7>();
  width<17>();
  width<31>();
  width<40>();
  width<63>();
  width<64>();
  unsigned _BitInt(1) bit = 1;
  check(bit);
  unsupported_width<_BitInt(65)>();
  unsupported_width<unsigned _BitInt(65)>();
  unsupported_width<_BitInt(128)>();
  unsupported_width<unsigned _BitInt(128)>();
  puts("native BitInt checks:300");
#else
  puts("native BitInt unavailable in this compiler");
  return 77;
#endif
}
