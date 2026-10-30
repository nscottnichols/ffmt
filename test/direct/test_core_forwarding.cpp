#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>
#include <ffmt/detail/checked.hpp>
#include <string>
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

static_assert(ffmt::is_formattable<counter>);
static_assert(!ffmt::is_formattable<const counter>);
void unbounded_memory_preserves_reference() {
  char output[32];
  counter value;
  auto result = ffmt::format_to(output, "{}", value);
  assert(result && std::string(output, result.written) == "1" &&
         value.value == 1);
}
void bounded_memory_preserves_reference() {
  char output[32];
  counter value;
  auto result =
      ffmt::format_to_n(output, sizeof(output), ffmt::runtime("{}"), value);
  assert(result && std::string(output, result.written) == "1" &&
         value.value == 1);
}
void size_query_and_temporary() {
  counter value;
  auto size = ffmt::formatted_size("{}", value);
  assert(size && size.required == 1 && value.value == 1);
  char output[32];
  auto temporary = ffmt::format_to(output, "{}", counter{});
  assert(temporary && std::string(output, temporary.written) == "1");
}
void const_value_is_rejected() {
  char output[32];
  const counter immutable;
  auto result = ffmt::format_to(output, ffmt::runtime("{}"), immutable);
  assert(result.error == ffmt::format_error::incompatible_type &&
         result.written == 0);
#ifdef FFMT_TEST_CONST_REJECT
  ffmt::format_to(output, "{}", immutable);
#endif
}
int main() {
  unbounded_memory_preserves_reference();
  bounded_memory_preserves_reference();
  size_query_and_temporary();
  const_value_is_rejected();
}
