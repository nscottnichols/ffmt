#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>
#include <ffmt/detail/checked.hpp>
#include <limits>
#include <string>

struct handler {
  std::string literal_text;
  int count = 0;
  void literal(char c) { literal_text += c; }
  ffmt::format_error argument(int index, ffmt::string_view,
                              ffmt::format_parse_context &pc) {
    assert(index == count++);
    auto spec = ffmt::detail::parse_standard_specs(pc);
    assert(spec.width == 4 && spec.type == 'x');
    return pc.error();
  }
};
int main() {
  handler events;
  assert(ffmt::detail::walk_format<char>("{{{:04x}}}", events) ==
         ffmt::format_error::none);
  assert(events.literal_text == "{}" && events.count == 1);
  for (const char *bad : {"{", "}", "{00}", "{0} {}", "{999999999999999}"}) {
    struct reject {
      void literal(char) {}
      ffmt::format_error argument(int, ffmt::string_view,
                                  ffmt::format_parse_context &pc) {
        ffmt::detail::parse_standard_specs(pc);
        return pc.error();
      }
    } h;
    assert(ffmt::detail::walk_format<char>(bad, h) != ffmt::format_error::none);
  }
  ffmt::format_parse_context invalid{{nullptr, 1}};
  assert(invalid.error() == ffmt::format_error::invalid_format);
}
