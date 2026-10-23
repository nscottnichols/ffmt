#include "capture.hpp"

#define CONCAT2(a, b) a##b
#define CONCAT(a, b) CONCAT2(a, b)

// Trampoline so RUN(...) can build an extended device lambda inline and
// hand it to a single launch. Requires nvcc/clang-CUDA's --extended-lambda.
template <typename F>
__global__ void _ffmt_run_kernel(F f) { f(); }

bool CONCAT(test_, TEST_NAME)() {
  int _test_id = 0;

  auto std_out = capture_stdout([&]() {
#define FFMT_STD_PATH
#define PRINT(fmt_str, ...) std::cout << ::fmt::format(fmt_str __VA_OPT__(,) __VA_ARGS__)
#define PRINTLN(fmt_str, ...) std::cout << ::fmt::format(fmt_str __VA_OPT__(,) __VA_ARGS__) << '\n'
#define RUN(...) do { printf("Test %d\n", ++_test_id); \
  for (int _i = 0; _i < N; _i++) { __VA_ARGS__; } } while (0)
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

  auto cuda_out = capture_stdout([&]() {
#define PRINT(fmt_str, ...) FFMT_PRINT(fmt_str __VA_OPT__(,) __VA_ARGS__)
#define PRINTLN(fmt_str, ...) FFMT_PRINTLN(fmt_str __VA_OPT__(,) __VA_ARGS__)
// One launch per RUN. Single-thread (<<<1,1>>>) with an internal N-loop so
// per-test ordering matches the host reference, which also does N inline
// repetitions inside the std capture block above.
#define RUN(...) do {                                                         \
  printf("Test %d\n", ++_test_id);                                            \
  _ffmt_run_kernel<<<1, 1>>>([=] __device__ () {                               \
    for (int _i = 0; _i < N; _i++) { __VA_ARGS__; }                           \
  });                                                                         \
  cudaDeviceSynchronize();                                                    \
} while (0)
#define SKIP_IF(gate, reason, ...) do {                                       \
  if (gate) printf("Test %d\n# skipped: %s\n", ++_test_id, reason);           \
  else RUN(__VA_ARGS__);                                                      \
} while (0)
#include TEST_INC
#undef SKIP_IF
#undef RUN
#undef PRINTLN
#undef PRINT
  });

  return diff_output(TEST_INC, std_out, cuda_out);
}

#ifndef TEST_NO_MAIN
int main() {
  return CONCAT(test_, TEST_NAME)() ? 0 : 1;
}
#endif
