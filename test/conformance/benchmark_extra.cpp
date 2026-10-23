#include "benchmark_calls.hpp"
BENCH_HD std::size_t benchmark_extra(char *output, int value, int floating,
                                     int printing, int native_printf) {
  return benchmark_call(output, value, floating, printing, native_printf);
}
