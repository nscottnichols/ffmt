#include "capture.hpp"

bool test_integers();
bool test_floats();
bool test_strings();
bool test_layout();
bool test_misc();
bool test_formatter();
bool test_features();
#if FFMT_BUFFER_PATH
bool test_buffer_path();
#endif

int main() {
  bool ok = true;
  ok &= test_integers();
  ok &= test_floats();
  ok &= test_strings();
  ok &= test_layout();
  ok &= test_misc();
  ok &= test_formatter();
  ok &= test_features();
#if FFMT_BUFFER_PATH
  ok &= test_buffer_path();
#endif
  return ok ? 0 : 1;
}
