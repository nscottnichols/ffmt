#include <cassert>
#include <cstdio>
#include <ffmt/detail/checked.hpp>
// Enable the oracle extension only when this host compiler supports it.
#if FFMT_HAS_FLOAT128 && !defined(FMT_USE_FLOAT128)
#define FMT_USE_FLOAT128 1
#endif
#include <fmt/format.h>
#include <string>
#if FFMT_HAS_FLOAT128 && FMT_USE_FLOAT128
// The pinned oracle's public type mapper does not accept __float128 directly;
// its native formatter is exercised through this transparent custom adapter.
struct quad_value {
  __float128 value;
};
template <>
struct fmt::formatter<quad_value> : fmt::formatter<fmt::detail::float128> {
  template <typename Context>
  auto format(const quad_value &value, Context &ctx) const {
    return fmt::formatter<fmt::detail::float128>::format(value.value, ctx);
  }
};
#endif
int main() {
#if FFMT_HAS_FLOAT128 && FMT_USE_FLOAT128
  struct words {
    std::uint64_t lo, hi;
  };
  unsigned checks = 0;
  for (words raw :
       {words{0, 0}, words{0, 0x8000000000000000ull}, words{1, 0},
        words{0, 0x0001000000000000ull}, words{1, 0x3fff000000000000ull},
        words{0xffffffffffffffffull, 0x3fffffffffffffffull},
        words{0xffffffffffffffffull, 0x7ffeffffffffffffull},
        words{0, 0x7fff000000000000ull}, words{0, 0x7fff800000000000ull}}) {
    auto value = __builtin_bit_cast(__float128, raw);
    for (const char *pattern : {"{}", "{:#}", "{:.0f}", "{:.6f}", "{:.34g}",
                                "{:.17e}", "{:a}", "{:.0a}", "{:.3a}"}) {
      auto expected = fmt::format(fmt::runtime(pattern), quad_value{value});
      std::string actual(expected.size() + 1, '~');
      auto result = ffmt::format_to_n(actual.data(), expected.size(),
                                      ffmt::runtime(pattern), value);
      if (!result || result.required != expected.size() ||
          actual.substr(0, result.written) != expected) {
        std::fprintf(stderr, "quad mismatch %s err%d got=%.*s expected=%s\n",
                     pattern, int(result.error), int(result.written),
                     actual.data(), expected.c_str());
        return 1;
      }
      ++checks;
    }
  }
  std::printf("binary128 checks: %u\n", checks);
#else
  puts("binary128 extension unavailable in this compiler configuration");
  return 77;
#endif
}
