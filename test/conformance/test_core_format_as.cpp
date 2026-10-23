#ifdef NDEBUG
#undef NDEBUG
#endif
#include "core_compare.hpp"
#include <cassert>
#include <ffmt/detail/checked.hpp>
#include <limits>
#include <string>

struct mapped {
  unsigned value;
};
constexpr auto format_as(mapped value) { return value.value; }
static_assert(ffmt::has_formatter<mapped>);
int main() {
  char out[32];
  auto result = ffmt::format_to(out, "{:#x}", mapped{42});
  assert(result && std::string(out, result.written) == "0x2a");
  compare("{:>{}}", 42, mapped{7});
  auto width = ffmt::format_to(out, "{:>{}}", 42, mapped{7});
  assert(width && std::string(out, width.written) == "     42");
}
