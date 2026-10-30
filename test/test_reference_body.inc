// Included inside the backend test function. The oracle always runs on host.
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
