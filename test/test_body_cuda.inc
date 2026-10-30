#include "capture.hpp"

// Native CUDA and HIP share one launcher; only runtime calls differ.
#if defined(__HIPCC__)
#include <hip/hip_runtime.h>
#define FFMT_TEST_LAST_ERROR hipGetLastError
#define FFMT_TEST_SYNCHRONIZE hipDeviceSynchronize
#define FFMT_TEST_ERROR_STRING hipGetErrorString
#else
#include <cuda_runtime.h>
#define FFMT_TEST_LAST_ERROR cudaGetLastError
#define FFMT_TEST_SYNCHRONIZE cudaDeviceSynchronize
#define FFMT_TEST_ERROR_STRING cudaGetErrorString
#endif

static bool synchronize_test_kernel() {
  auto launch_error = FFMT_TEST_LAST_ERROR();
  auto execution_error = FFMT_TEST_SYNCHRONIZE();
  for (auto error : {launch_error, execution_error}) {
    if (error != 0) {
      std::fprintf(stderr, "device test runtime error: %s\n",
                   FFMT_TEST_ERROR_STRING(error));
      return false;
    }
  }
  return true;
}

#undef FFMT_TEST_LAST_ERROR
#undef FFMT_TEST_SYNCHRONIZE
#undef FFMT_TEST_ERROR_STRING

#define CONCAT2(a, b) a##b
#define CONCAT(a, b) CONCAT2(a, b)

// Trampoline so RUN(...) can build an extended device lambda inline and
// hand it to a single launch. Requires nvcc/clang-CUDA's --extended-lambda.
template <typename F>
__global__ void _ffmt_run_kernel(F f) { f(); }

bool CONCAT(test_, TEST_NAME)() {
  int _test_id = 0;
  bool runtime_ok = true;

#include "test_reference_body.inc"

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
  runtime_ok = synchronize_test_kernel() && runtime_ok;                      \
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

  return diff_output(TEST_INC, std_out, cuda_out) && runtime_ok;
}

#ifndef TEST_NO_MAIN
int main() {
  return CONCAT(test_, TEST_NAME)() ? 0 : 1;
}
#endif
