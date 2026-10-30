#include "capture.hpp"

bool test_integers(::sycl::queue& q);
bool test_floats(::sycl::queue& q);
bool test_strings(::sycl::queue& q);
bool test_layout(::sycl::queue& q);
bool test_misc(::sycl::queue& q);
bool test_formatter(::sycl::queue& q);

int main() {
  try {
    auto q = make_gpu_test_queue();

    bool ok = true;
    ok &= test_integers(q);
    ok &= test_floats(q);
    ok &= test_strings(q);
    ok &= test_layout(q);
    ok &= test_misc(q);
    ok &= test_formatter(q);
    return ok ? 0 : 1;
  } catch (const std::exception &error) {
    std::fprintf(stderr, "SYCL hardware tests failed: %s\n", error.what());
    return 1;
  }
}
