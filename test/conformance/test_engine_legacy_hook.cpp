#include <cassert>
#include <cstdio>
#include <string>
static std::string bytes;
static void capture_printf(const char *pattern) {
  char result[512];
  auto n = std::snprintf(result, sizeof result, pattern, 0);
  assert(n >= 0 && n < int(sizeof result));
  bytes.append(result, std::size_t(n));
}
#define FFMT_BUFFER_SIZE 16
#define FFMT_EMIT_BUFFER(out, escape_pct)                                      \
  do {                                                                         \
    if (escape_pct)                                                            \
      ffmt::detail::buffer_path::escape_percent_inplace(out);                  \
    capture_printf((out).data);                                                \
  } while (0)
#include <ffmt/detail/checked.hpp>
int main() {
  std::string expected(257, '%');
  auto result = ffmt::try_print("{}", expected.c_str());
  assert(result && result.written == expected.size() && bytes == expected);
  bytes.clear();
  result = ffmt::try_println("100% {}", 42);
  assert(result && bytes == "100% 42\n");
  bytes.clear();
  result = ffmt::try_print("a{}b", char(0));
  assert(result.error == ffmt::format_error::transport_error && bytes.empty());
}
