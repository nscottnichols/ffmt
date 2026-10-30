#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>
#include <ffmt/detail/checked.hpp>
#include <string>
struct legacy_counter {
  int value = 0;
};
template <> struct ffmt::formatter<legacy_counter> {
  static auto format(legacy_counter &item) {
    return ffmt::formatted<ffmt::detail::fixed_string{"{}"}, int>{
        {++item.value}};
  }
};

static_assert(ffmt::is_formattable<legacy_counter>);
static_assert(!ffmt::is_formattable<const legacy_counter>);
int main() {
  char output[32];
  legacy_counter value;
  auto result = ffmt::format_to(output, "{}", value);
  assert(result && value.value == 1 &&
         std::string(output, result.written) == "1");
  const legacy_counter immutable;
  auto rejected = ffmt::format_to(output, ffmt::runtime("{}"), immutable);
  assert(rejected.error == ffmt::format_error::incompatible_type);
}
