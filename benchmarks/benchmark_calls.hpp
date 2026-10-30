#pragma once
#include <cstddef>
#include <cstdio>
#if defined(BENCH_REFERENCE)
#include <fmt/format.h>
#define BENCH_HD
#else
#include <ffmt/base.hpp>
#if defined(BENCH_CUDA) || defined(BENCH_HIP)
#define BENCH_HD __host__ __device__
#else
#define BENCH_HD
#endif
#endif

// Intel SYCL requires its device printf intrinsic rather than a variadic libc
// call. The format strings remain literal so they enter constant address space.
#if defined(BENCH_SYCL) && !defined(BENCH_REFERENCE) && !FFMT_COMPILER_ACPP
#define BENCH_PRINTF(...) ::sycl::ext::oneapi::experimental::printf(__VA_ARGS__)
#else
#define BENCH_PRINTF(...) ::printf(__VA_ARGS__)
#endif

#if defined(BENCH_SHORTEST) && BENCH_SHORTEST
#define BENCH_FLOAT_PATTERN "value={}\n"
#else
#define BENCH_FLOAT_PATTERN "value={:.8e}\n"
#endif

// Explicit-precision rows have equivalent format and printf specifications.
// Shortest conversion is measured only in memory against independent fmt.
// Values are exact binary fractions, so input construction adds no rounding.
#if defined(BENCH_OMP)
#pragma omp declare target
#endif
BENCH_HD inline std::size_t benchmark_call(char *output, int value,
                                           int floating, int printing,
                                           int native_printf) {
  const double number = static_cast<double>(value) / 8.0;
  if (printing) {
    if (native_printf) {
      return floating ? BENCH_PRINTF("value=%.8e\n", number)
                      : BENCH_PRINTF("value=%+012d\n", value);
    }
#if defined(BENCH_REFERENCE)
    if (floating)
      fmt::print(BENCH_FLOAT_PATTERN, number);
    else
      fmt::print("value={:+012d}\n", value);
#else
    if (floating)
      FFMT_PRINT(BENCH_FLOAT_PATTERN, number);
    else
      FFMT_PRINT("value={:+012d}\n", value);
#endif
    return 0;
  }
#if defined(BENCH_REFERENCE)
  auto r = floating ? fmt::format_to_n(output, 64, BENCH_FLOAT_PATTERN, number)
                    : fmt::format_to_n(output, 64, "value={:+012d}\n", value);
  return r.size;
#else
  auto r = floating ? ffmt::format_to_n(output, 64, BENCH_FLOAT_PATTERN, number)
                    : ffmt::format_to_n(output, 64, "value={:+012d}\n", value);
  return r.error == ffmt::format_error::none ? r.written : 65;
#endif
}

#if defined(BENCH_MULTI_TU)
BENCH_HD std::size_t benchmark_extra(char *, int, int, int, int);
#endif
#if defined(BENCH_OMP)
#pragma omp end declare target
#endif
