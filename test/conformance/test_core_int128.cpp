#ifdef NDEBUG
#undef NDEBUG
#endif
#include "core_compare.hpp"
#include <cassert>
#include <ffmt/detail/checked.hpp>
#include <limits>
#include <string>

int main() {
#if defined(__SIZEOF_INT128__)
  using U = unsigned __int128;
  U value = (U(1) << 100) + 42;
  compare("{} {:#x} {:#b}", value, value, value);
  compare("{}", -static_cast<__int128>(value));
#else
  return 77;
#endif
}
