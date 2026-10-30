#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>
#include <ffmt/enum.hpp>
#include <fmt/std.h>
#include <string>
namespace enum_example {
using ffmt::enums::format_as;
enum class status : int { warning = -3, success = 42 };
} // namespace enum_example
int main() {
  char output[64];
  auto result = ffmt::format_to_n(output, sizeof(output), "{:+05}",
                                  enum_example::status::warning);
  auto expected = fmt::format(
      "{:+05}", fmt::enums::format_as(enum_example::status::warning));
  assert(result && std::string(output, result.written) == expected);
  auto underlying =
      ffmt::format_to_n(output, sizeof(output), "{:#04x}",
                        ffmt::underlying(enum_example::status::success));
  expected =
      fmt::format("{:#04x}", fmt::underlying(enum_example::status::success));
  assert(underlying && std::string(output, underlying.written) == expected);
}
