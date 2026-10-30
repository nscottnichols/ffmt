#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>
#include <ffmt/detail/checked.hpp>
#include <limits>
#include <string>

struct parsed {
  int value;
};
template <typename Char>
struct ffmt::formatter<parsed, Char> : ffmt::formatter<int, Char> {
  template <typename Context>
  auto format(const parsed &value, Context &ctx) const {
    return ffmt::formatter<int, Char>::format(value.value, ctx);
  }
};
struct unsupported {};
static_assert(ffmt::has_formatter<int> && ffmt::has_formatter<parsed> &&
              !ffmt::has_formatter<unsupported>);
int main() {
  char out[32];
  auto result = ffmt::format_to(out, "{:04x}", parsed{42});
  assert(result && std::string(out, result.written) == "002a");
}
