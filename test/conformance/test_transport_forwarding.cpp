#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cstddef>
#include <string>
static std::string bytes;
static void capture(const char *data, std::size_t size) {
  bytes.append(data, size);
}
#define FFMT_EMIT_CHUNK(data, size) capture(data, size)
#include <cassert>
#include <ffmt/detail/checked.hpp>
struct counter {
  int value = 0;
  counter() = default;
  counter(const counter &) = delete;
  counter(counter &&) = delete;
};
template <typename Char>
struct ffmt::formatter<counter, Char> : ffmt::formatter<int, Char> {
  template <class Context> auto format(counter &item, Context &ctx) const {
    return ffmt::formatter<int, Char>::format(++item.value, ctx);
  }
};

int main() {
  counter print_value;
  ffmt::print("{}", print_value);
  assert(bytes == "1" && print_value.value == 1);
  bytes.clear();
  counter checked_value;
  auto result = ffmt::try_print<"{}">(checked_value);
  assert(result && bytes == "1" && checked_value.value == 1);
  bytes.clear();
  counter line_value;
  ffmt::println("{}", line_value);
  assert(bytes == "1\n" && line_value.value == 1);
}
