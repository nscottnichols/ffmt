// Native80/128-bit formatting against the independently compiled host oracle.
#include <array>
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <ffmt/detail/checked.hpp>
#include <fmt/format.h>
#include <limits>
#include <string>
static unsigned checks = 0;
static void check(long double value) {
  for (const char *pattern :
       {"{}", "{:#}", "{:.0f}", "{:.6f}", "{:.21g}", "{:.34g}", "{:.17e}",
        "{:a}", "{:.0a}", "{:.3a}", "{:#40.9a}"}) {
    auto expected = fmt::format(fmt::runtime(pattern), value);
    std::string actual(expected.size() + 8, '~');
    auto result = ffmt::format_to_n(actual.data(), expected.size(),
                                    ffmt::runtime(pattern), value);
    if (result.error != ffmt::format_error::none ||
        result.required != expected.size() ||
        actual.substr(0, result.written) != expected) {
      std::fprintf(stderr,
                   "extended mismatch %s error%d: actual=%.*s expected=%s\n",
                   pattern, int(result.error), int(result.written),
                   actual.data(), expected.c_str());
      std::abort();
    }
    assert(actual.substr(expected.size()) == std::string(8, '~'));
    auto bounded =
        ffmt::format_to_n(actual.data(), 1, ffmt::runtime(pattern), value);
    assert(bounded.error == ffmt::format_error::none && bounded.written == 1 &&
           bounded.required == expected.size());
    ++checks;
  }
}
template <typename LongDouble> static void check_raw() {
  if constexpr (std::numeric_limits<LongDouble>::digits == 64 &&
                sizeof(LongDouble) == 16) {
    struct raw80 {
      std::uint64_t significand;
      std::uint16_t exponent;
      std::uint16_t pad[3];
    };
    std::uint64_t state = 0x4d595df4d0f33173ull;
    for (unsigned i = 0; i < 96; ++i) {
      state ^= state << 13;
      state ^= state >> 7;
      state ^= state << 17;
      unsigned exp = unsigned((state >> 32) % 0x7fff);
      auto bits = raw80{exp ? state | 0x8000000000000000ull
                            : state & 0x7fffffffffffffffull,
                        std::uint16_t(exp | ((state & 1) ? 0x8000 : 0)),
                        {}};
      check(__builtin_bit_cast(LongDouble, bits));
    }
    for (std::uint64_t m : {1ull, 0x7fffffffffffffffull, 0x7000000000000000ull,
                            0x4000000000000000ull})
      check(__builtin_bit_cast(LongDouble, raw80{m, 0, {}}));
  }
  if constexpr (std::numeric_limits<LongDouble>::digits == 113 &&
                sizeof(LongDouble) == 16) {
    struct raw128 {
      std::uint64_t lo, hi;
    };
    std::uint64_t state = 0x9e3779b97f4a7c15ull;
    for (unsigned i = 0; i < 96; ++i) {
      state ^= state << 13;
      state ^= state >> 7;
      state ^= state << 17;
      auto low = state;
      state ^= state << 13;
      state ^= state >> 7;
      state ^= state << 17;
      auto high = state;
      if ((high & 0x7fff000000000000ull) == 0x7fff000000000000ull)
        high ^= 1ull << 48;
      check(__builtin_bit_cast(LongDouble, raw128{low, high}));
    }
    for (raw128 raw :
         {raw128{1, 0}, raw128{0xffffffffffffffffull, 0x0000ffffffffffffull},
          raw128{0, 0x0000800000000000ull}, raw128{0, 0x0000400000000000ull}})
      check(__builtin_bit_cast(LongDouble, raw));
  }
}
int main() {
  for (long double value :
       {0.L, -0.L, 1.L, 0.1L, 1.234567890123456789L, 1.9999999999999999999L,
        0x1.23456789abcdefp-100L, 0.5L, 1.5L, 2.5L,
        std::numeric_limits<long double>::min(),
        std::numeric_limits<long double>::denorm_min(),
        std::numeric_limits<long double>::max(),
        std::numeric_limits<long double>::infinity(),
        -std::numeric_limits<long double>::infinity(),
        std::numeric_limits<long double>::quiet_NaN()})
    check(value);
  check_raw<long double>();
  char out[8];
  auto large = ffmt::format_to_n(out, sizeof out, "{:.20000f}", 0.125L);
  assert(large && large.required == 20002);
  std::printf("extended checks: %u (digits=%d)\n", checks,
              std::numeric_limits<long double>::digits);
}
