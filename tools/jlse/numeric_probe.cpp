#if defined(PROBE_SYCL)
#include <sycl/sycl.hpp>
#endif
#include <cstdint>
#include <cstdio>
#if defined(PROBE_OPTNONE)
#pragma clang optimize off
#endif
#include <ffmt/base.hpp>
#if defined(PROBE_OPTNONE)
#pragma clang optimize on
#endif

struct conversion {
  unsigned long long significand;
  int exponent;
  int length;
  char output[128];
};
template <class T> void fill(conversion& output, T value) {
  auto decimal = ffmt::detail::dragonbox::to_decimal(value);
  output.significand = decimal.significand;
  output.exponent = decimal.exponent;
  output.length = ffmt::detail::dragonbox::format_shortest(output.output, value);
}
void fill_all(conversion* records) {
  fill(records[0], 0.1f);
  fill(records[1], 1e15);
  fill(records[2], 1.23456789);
  fill(records[3], __builtin_bit_cast(float, std::uint32_t(1)));
}
int main() {
  conversion* records;
#if defined(PROBE_SYCL)
  sycl::queue queue{sycl::gpu_selector_v};
  records = sycl::malloc_shared<conversion>(4, queue);
  queue.single_task([=] {fill_all(records);}).wait_and_throw();
#else
  conversion host_records[4]{};
  records = host_records;
  fill_all(records);
#endif
  for (int i=0; i<4; ++i) {
    auto& result = records[i];
    if (result.length < 0 || result.length > 128) return 81;
    std::printf("%d %llu %d %.*s\n", i, result.significand, result.exponent, result.length, result.output);
  }
#if defined(PROBE_SYCL)
  sycl::free(records, queue);
#endif
}
