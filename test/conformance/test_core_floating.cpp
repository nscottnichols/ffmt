#ifdef NDEBUG
#undef NDEBUG
#endif
#include "core_compare.hpp"
#include <cassert>
#include <ffmt/detail/checked.hpp>
#include <limits>
#include <string>

int main() {
  compare("{:.180f}", 0.125);
  compare("{:.2000f}", 0.125);
  compare("{:.2000e}", 0.125);
  compare("{:#.2000g}", 0.125);
  compare("{:.0f} {:.0f} {:.0f} {:.0f}", 0.5, 1.5, 2.5, 9.5);
  compare("{} {}", std::numeric_limits<double>::min(),
          std::numeric_limits<double>::denorm_min());
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
  }
  char out[8];
  auto huge = ffmt::format_to_n(out, 2, "{:#.2147483647g}", 0.125);
  assert(huge && huge.required == 2147483649ull && huge.written == 2);
}
