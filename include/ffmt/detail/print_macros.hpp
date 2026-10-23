#pragma once
// Convenience macro — nicer syntax without explicit template angle brackets

#define FFMT_PRINT(fmtstr, ...) ::ffmt::print(fmtstr __VA_OPT__(, ) __VA_ARGS__)
#define FFMT_PRINTLN(fmtstr, ...)                                              \
  ::ffmt::println(fmtstr __VA_OPT__(, ) __VA_ARGS__)

// Compatibility aliases used by existing custom-formatter examples.
#define FFMT_PRINTF(fmtstr, ...) FFMT_PRINT(fmtstr __VA_OPT__(, ) __VA_ARGS__)
#define FFMT_PRINTLNF(fmtstr, ...)                                             \
  FFMT_PRINTLN(fmtstr __VA_OPT__(, ) __VA_ARGS__)
