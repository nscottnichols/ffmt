#include "capture.hpp"

#define CONCAT2(a, b) a##b
#define CONCAT(a, b) CONCAT2(a, b)

bool CONCAT(test_, TEST_NAME)() {
  int _test_id = 0;

  auto std_out = capture_stdout([&]() {
#define FFMT_STD_PATH
#define PRINT(fmt_str, ...) std::cout << ::fmt::format(fmt_str __VA_OPT__(,) __VA_ARGS__)
#define PRINTLN(fmt_str, ...) std::cout << ::fmt::format(fmt_str __VA_OPT__(,) __VA_ARGS__) << '\n'
#define RUN(...) do { printf("Test %d\n", ++_test_id); \
  for (int _i = 0; _i < N; _i++) { __VA_ARGS__; } } while (0)
// SKIP_IF: when `gate` is true (a Makefile-defined per-backend bug flag),
// emit `Test N\n# skipped: <reason>` on both reference and target streams
// so diff_output reports it as `skipped`. When false, falls through to RUN.
#define SKIP_IF(gate, reason, ...) do {                                       \
  if (gate) printf("Test %d\n# skipped: %s\n", ++_test_id, reason);           \
  else RUN(__VA_ARGS__);                                                      \
} while (0)
#include TEST_INC
#undef SKIP_IF
#undef RUN
#undef PRINTLN
#undef PRINT
#undef FFMT_STD_PATH
  });

  _test_id = 0;

  auto omp_out = capture_stdout([&]() {
#define PRINT(fmt_str, ...) FFMT_PRINT(fmt_str __VA_OPT__(,) __VA_ARGS__)
#define PRINTLN(fmt_str, ...) FFMT_PRINTLN(fmt_str __VA_OPT__(,) __VA_ARGS__)
// icpx-OMP-SPIR64 quirk: when `_Pragma("omp target ...")` is inside a
// `do { ... } while(0)` block emitted by a macro, the kernel gets registered
// under one __LINE__ but referenced under another, and the runtime fails
// every offload with "Failed to load kernel __omp_offloading_*_l<N>". Drop
// the do/while wrapper — RUN(...) sits at top-level inside the lambda here,
// so a bare statement sequence is fine.
#define RUN(...) printf("Test %d\n", ++_test_id);                              \
  _Pragma("omp target teams distribute parallel for num_teams(1) thread_limit(N)") \
  for (int _i = 0; _i < N; _i++) { __VA_ARGS__; }
// SKIP_IF mirror for the omp-target side. Same caveat as RUN: keep
// `_Pragma("omp target ...")` directly followed by the `for` it
// targets — `distribute parallel for` requires an iteration statement
// next, and wrapping the for in `if` or `{}` either drops the kernel
// entirely (clang-OMP) or mismatches __LINE__ at registration (icpx).
// So we ALWAYS run the for; the gate just makes the body a no-op and
// emits the skip marker beforehand.
#define SKIP_IF(gate, reason, ...)                                            \
  if (gate) printf("Test %d\n# skipped: %s\n", ++_test_id, reason);           \
  else printf("Test %d\n", ++_test_id);                                       \
  _Pragma("omp target teams distribute parallel for num_teams(1) thread_limit(N)") \
  for (int _i = 0; _i < N; _i++) { if (!(gate)) { __VA_ARGS__; } }
#include TEST_INC
#undef SKIP_IF
#undef RUN
#undef PRINTLN
#undef PRINT
  });

  return diff_output(TEST_INC, std_out, omp_out);
}

#ifndef TEST_NO_MAIN
int main() {
  return CONCAT(test_, TEST_NAME)() ? 0 : 1;
}
#endif
