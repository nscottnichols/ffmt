#include "family_test_support.hpp"
#include <array>
#include <ffmt/args.hpp>
#include <ffmt/ranges.hpp>
#include <ffmt/std.hpp>
#include <fmt/ranges.h>
#include <fmt/std.h>
struct mutable_numbers {
  int *first;
  int *last;
  int *begin() { return first; }
  int *end() { return last; }
};
namespace {
ffmt_test::differential_suite suite("feature integration");
void named_range_width() {
  std::array<int, 3> numbers{1, -2, 33};
  suite.equal_expected("{values:*>{width}:03}",
                       fmt::format("{values:*>{width}:03}",
                                   fmt::arg("values", numbers),
                                   fmt::arg("width", 25)),
                       ffmt::arg("values", numbers), ffmt::arg("width", 25));
}
void named_complex_width() {
  auto value = std::complex<double>{1, -2.2};
  suite.equal_expected("{value:>{width}.2f}",
                       fmt::format("{value:>{width}.2f}",
                                   fmt::arg("value", value),
                                   fmt::arg("width", 25)),
                       ffmt::arg("value", value), ffmt::arg("width", 25));
}
void named_mutable_range() {
  int numbers[] = {1, -2, 33};
  mutable_numbers value{numbers, numbers + 3};
  std::array<char, 256> output;
  auto result = ffmt::format_to_n(output.data(), output.size(),
                                  "{items:*>20:03}", ffmt::arg("items", value));
  assert(result.error == ffmt::format_error::none);
  assert(std::string(output.data(), result.written) ==
         fmt::format("{:*>20:03}", value));
  suite.record();
}
} // namespace
int main() {
  named_range_width();
  named_complex_width();
  named_mutable_range();
  suite.report();
}
