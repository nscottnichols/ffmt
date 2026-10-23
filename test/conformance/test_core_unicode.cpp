#ifdef NDEBUG
#undef NDEBUG
#endif
#include "core_compare.hpp"
#include <cassert>
#include <ffmt/detail/checked.hpp>
#include <limits>
#include <string>

int main() {
  compare("[{:.3}] [{:─^9}]", "静夜思", "é");
  compare("{:?}", "a\n\t\"\\\x7f");
  compare("{:?}", '\n');
  compare("{:.3?}", "a\nb");
  compare("{:.2?}", "界");
  compare("{:.3?}", "界");
  compare("{:.4?}", "界");
  compare("{:.2?}", "😀");
  std::uint32_t point;
  const char truncated[] = {'\xf0', '\x9f'};
  assert(ffmt::detail::decode_utf8(truncated, sizeof truncated, point) == 1 &&
         point == ffmt::detail::invalid_code_point);
}
