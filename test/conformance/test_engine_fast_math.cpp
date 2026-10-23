// Compile with -ffast-math. Expectations come from the strict pinned oracle;
// compiling the oracle's inline dispatch with fast-math can flush subnormals.
#include <cassert>
#include <cstdint>
#include <ffmt/detail/checked.hpp>
#include <iostream>
#include <string>

static unsigned checks = 0;
template <typename T, typename Bits>
static void check(Bits bits, const char *pattern, const char *expected) {
  volatile Bits stored = bits;
  T value = __builtin_bit_cast(T, Bits(stored));
  char output[128];
  auto result =
      ffmt::format_to_n(output, sizeof output, ffmt::runtime(pattern), value);
  assert(result && result.written == result.required &&
         std::string(output, result.written) == expected);
  ++checks;
}
int main() {
  check<double>(std::uint64_t(1), "{}", "5e-324");
  check<double>(std::uint64_t(0x8000000000000001ull), "{}", "-5e-324");
  check<double>(std::uint64_t(0x000fffffffffffffull), "{}",
                "2.225073858507201e-308");
  check<float>(std::uint32_t(1), "{}", "1e-45");
  check<float>(std::uint32_t(0x007fffff), "{}", "1.1754942e-38");
  check<float>(std::uint32_t(0x80000001), "{}", "-1e-45");
  check<double>(std::uint64_t(0x8000000000000000ull), "{}", "-0");
  check<float>(std::uint32_t(0x80000000), "{}", "-0");
  check<double>(std::uint64_t(1), "{:#}", "5.e-324");
  check<double>(std::uint64_t(1), "{:e}", "4.940656e-324");
  check<double>(std::uint64_t(1), "{:g}", "4.94066e-324");
  check<double>(std::uint64_t(1), "{:a}", "0x0.0000000000001p-1022");
  check<float>(std::uint32_t(1), "{:a}", "0x1p-149");
  check<double>(std::uint64_t(0x7ff0000000000000ull), "{}", "inf");
  check<double>(std::uint64_t(0xfff0000000000000ull), "{}", "-inf");
  check<double>(std::uint64_t(0x7ff8000000000001ull), "{}", "nan");
  check<double>(std::uint64_t(0xfff8000000000001ull), "{}", "-nan");
  std::cout << "fast-math: " << checks << " representation checks passed\n";
}
