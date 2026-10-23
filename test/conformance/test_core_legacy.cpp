#ifdef NDEBUG
#undef NDEBUG
#endif
#include "core_compare.hpp"
#include <cassert>
#include <ffmt/detail/checked.hpp>
#include <limits>
#include <string>

struct point {
  int x, y;
};
template <> struct ffmt::formatter<point> {
  static constexpr auto format(point value) {
    return ffmt::formatted<ffmt::detail::fixed_string{"({1}, {0})"}, int, int>{
        {value.x, value.y}};
  }
};
static_assert(ffmt::has_formatter<point>);
int main() {
  char out[32];
  auto result = ffmt::format_to(out, "{}", point{2, 7});
  assert(result && std::string(out, result.written) == "(7, 2)");
  wchar_t wide[32];
  auto actual = ffmt::format_to(wide, L"{}", point{2, 7});
  assert(actual && std::wstring(wide, actual.written) == L"(7, 2)");
}
