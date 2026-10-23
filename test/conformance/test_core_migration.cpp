#ifdef NDEBUG
#undef NDEBUG
#endif
#include <algorithm>
#include <cassert>
#include <ffmt/base.hpp>
#include <fmt/format.h>
#include <fmt/xchar.h>
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
int main() {
  wchar_t wide[128];
  auto actual =
      ffmt::format_to(wide, L"[{:>8}] {:.3f} {}", L"静夜", 1.25, point{2, 7});
  auto expected = fmt::format(L"[{:>8}] {:.3f} ({}, {})", L"静夜", 1.25, 7, 2);
  assert(actual && std::wstring(wide, actual.written) == expected);
}
