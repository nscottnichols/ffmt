#include <ffmt/base.hpp>
#include <fmt/std.h>
#include <cassert>
#include <cstdio>
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
  char buffer[2];
  auto unsupported =
      ffmt::format_to_n(buffer, sizeof buffer, ffmt::runtime("{}"),
                        static_cast<unsigned _BitInt(128)>(1));
  assert(unsupported.error == ffmt::format_error::unsupported_type);
  puts("native BitInt checks:300");
#else
  puts("native BitInt unavailable in this compiler");
  return 77;
#endif
}
