#include <ffmt/base.hpp>
#include <string_view>

int main() {
  char output[8];
  const auto result = ffmt::format_to_n(output, sizeof output, "{:04x}", 42);
  return result && std::string_view(output, result.written) == "002a" ? 0 : 1;
}
