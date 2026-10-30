#ifndef TEST_INC
#define TEST_NAME integers
#define TEST_INC "test_integers.cpp"
#include "test_select_body.inc"
#else

RUN(PRINT("{}\n", 42));
RUN(PRINT("{}\n", -123));
RUN(PRINT("{}\n", 100u));
RUN(PRINT("{}\n", 1234567890L));
RUN(PRINT("{}\n", static_cast<int64_t>(9876543210LL)));
RUN(PRINT("{}\n", static_cast<uint64_t>(18446744073709551615ULL)));
RUN(PRINT("{}\n", static_cast<short>(32767)));
RUN(PRINT("{}\n", static_cast<int8_t>(-128)));
RUN(PRINT("{}\n", static_cast<uint8_t>(255)));
RUN(PRINT("{}\n", static_cast<int16_t>(-32768)));
RUN(PRINT("{}\n", 2147483647));
RUN(PRINT("{}\n", static_cast<int64_t>(-9223372036854775807LL)));
RUN(PRINT("{}\n", static_cast<uint64_t>(18446744073709551615ULL)));

// Type specifiers
RUN(PRINT("{:d}\n", 255));
RUN(PRINT("{:x}\n", 255u));
RUN(PRINT("{:X}\n", 255u));
RUN(PRINT("{:o}\n", 255u));
SKIP_IF(FFMT_PTX_CLANG_O0, "ptx-clang-o0", PRINT("{:#o}\n", 255u));

// Extremes
RUN(PRINT("{:d}\n", -2147483647 - 1));
RUN(PRINT("{:d}\n", static_cast<int64_t>(-9223372036854775807LL - 1)));
RUN(PRINT("{}\n", static_cast<int64_t>(-9223372036854775807LL - 1)));


// Specifiers-path bugs (printf flags that differ from {fmt}):
// '0' is ignored when an alignment is given ("%05d" zero-padded "{:>05}").
RUN(PRINT("[{:>05}]\n", 42));
RUN(PRINT("[{:<05}]\n", -42));
// {:d} on unsigned stays unsigned (it went through a signed int cast).
RUN(PRINT("{:d}\n", 4000000000u));
RUN(PRINT("{:d}\n", ~0ull));
RUN(PRINT("{:5d}\n", static_cast<uint8_t>(200)));

// Width and precision above 255 / 127 (they were stored in 8 bits: {:300}
// padded to 44).
RUN(PRINT("[{:300}]\n", 1));
RUN(PRINT("[{:<1000}]\n", -7));


// Regressions moved from the separate conformance fixture registry.
// int-min
RUN(PRINT("{}", INT64_MIN));
// uint-max
RUN(PRINT("{}", UINT64_MAX));
// uint-d
RUN(PRINT("{:d}", 4000000000u));
// signed-hex
RUN(PRINT("{:#010x}", -42));
// binary-max
RUN(PRINT("{:b}", UINT64_MAX));
// zero-alt
RUN(PRINT("{:#x} {:#o} {:#b}", 0u, 0u, 0u));

#endif
