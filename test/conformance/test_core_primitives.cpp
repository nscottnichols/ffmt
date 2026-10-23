#ifdef NDEBUG
#undef NDEBUG
#endif
#include "core_compare.hpp"
#include <cassert>
#include <ffmt/detail/checked.hpp>
#include <limits>
#include <string>

int main() {
  compare("");
  compare("{}", std::string{});
  compare("a{}b", char(0));
  compare("{0} {0} {1}", 42, "hello");
  compare("[{0:>{1}}]", 42, 400);
  compare("{}", ffmt::string_view{"x\0y", 3});
  []<std::size_t... I>(std::index_sequence<I...>) {
    compare("{0:>{139}}", int(I)...);
  }(std::make_index_sequence<140>{});
  std::string pattern(513, 'x');
  pattern += "{0}";
  for (int i = 0; i < 32; ++i)
    pattern += "{0}";
  compare(pattern, 123);
  std::uint64_t state = 0x4d595df4d0f33173ull;
  for (unsigned i = 0; i < 4096; ++i) {
    state ^= state << 13;
    state ^= state >> 7;
    state ^= state << 17;
    compare("{:#020x} {:#070b} {:o}", state, state, state);
    compare("{}", static_cast<std::int64_t>(state));
  }
  char out[32];
  auto embedded = ffmt::format_to(out, "a\0{}", 7);
  assert(embedded.written == 3 && out[0] == 'a' && out[1] == 0 &&
         out[2] == '7');
  auto large = ffmt::format_to_n(out, 2, "{:100000}", 7);
  assert(large && large.required == 100000 && large.written == 2);
  auto nullstring = ffmt::format_to_n(out, sizeof out, ffmt::runtime("{}"),
                                      static_cast<const char *>(nullptr));
  assert(nullstring.error == ffmt::format_error::invalid_format);
  for (const char *bad : {"{", "}", "{} {}", "{0} {}", "{0:!}", "{99}",
                          "{999999999999999999999}"})
    assert(!ffmt::format_to_n(out, sizeof out, ffmt::runtime(bad), 42));
  auto negative =
      ffmt::format_to_n(out, sizeof out, ffmt::runtime("{:{}}"), 42, -1);
  auto invalid =
      ffmt::format_to_n(out, sizeof out, ffmt::runtime("{:{}}"), 42, 1.5);
  assert(negative.error == ffmt::format_error::invalid_dynamic_spec &&
         invalid.error == ffmt::format_error::invalid_dynamic_spec);
  wchar_t wide[32];
  auto result = ffmt::format_to(wide, L"{} {:d}", 'a', 'a');
  assert(result && std::wstring(wide, result.written) == L"a 97");
}
