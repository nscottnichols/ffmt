#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>
#include <ffmt/detail/checked.hpp>
#include <string>
struct named_value {
  const char *name;
  int value;
};
template <> struct ffmt::formatter<named_value> {
  constexpr auto parse(ffmt::format_parse_context &ctx) { return ctx.begin(); }
  template <typename Context>
  auto format(const named_value &item, Context &ctx) const {
    return ffmt::format_to(ctx.out(), "{}={}", item.name, item.value).out;
  }
};

int main() {
  char output[64];
  auto result = ffmt::format_to_n(output, sizeof(output), "{}",
                                  named_value{"ordinary", 42});
  assert(result && result.required == 11);
  assert(std::string(output, result.written) == "ordinary=42");
}
