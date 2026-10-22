#ifndef TEST_INC
#define TEST_NAME floats
#define TEST_INC "test_floats.cpp"
#include "test_select_body.inc"
#else

// Floats — default format ({}) for float and double
RUN(PRINT("{}\n", 3.14f));
RUN(PRINT("{}\n", 0.1f));
RUN(PRINT("{}\n", 1.0f));
SKIP_IF(FFMT_PTX_CLANG_O0, "ptx-clang-o0", PRINT("{}\n", -2.5f));
RUN(PRINT("{}\n", 3.14));
RUN(PRINT("{}\n", 0.1));
RUN(PRINT("{}\n", 1.0));
SKIP_IF(FFMT_PTX_CLANG_O0, "ptx-clang-o0", PRINT("{}\n", -2.5));

// Floats — explicit specs
RUN(PRINT("{:g}\n", 3.14));
RUN(PRINT("{:g}\n", 2.718281828459045));
RUN(PRINT("{:g}\n", -0.001));
RUN(PRINT("{:g}\n", 1.0e+300));
RUN(PRINT("{:g}\n", 1.0e-300));

// Float type specifiers
RUN(PRINT("{:f}\n", 3.14159));
RUN(PRINT("{:e}\n", 3.14159));
RUN(PRINT("{:E}\n", 3.14159));
RUN(PRINT("{:g}\n", 3.14159));
RUN(PRINT("{:G}\n", 3.14159));

// Precision
RUN(PRINT("{:.2f}\n", 3.14159265));
RUN(PRINT("{:.10f}\n", 3.14159265));
RUN(PRINT("{:.0f}\n", 3.14159265));
RUN(PRINT("{:.3e}\n", 0.000123456));

// Float specials (inf, nan, -0)
RUN(PRINT("{:g}\n", 1.0 / 0.0));
RUN(PRINT("{:g}\n", -1.0 / 0.0));
RUN(PRINT("{:f}\n", 1.0 / 0.0));
RUN(PRINT("{:e}\n", 1.0 / 0.0));
RUN(PRINT("{:+f}\n", 3.14));

// {:F} — uppercase INF/NAN
RUN(PRINT("{:F}\n", 7.14));
RUN(PRINT("{:F}\n", 1.0 / 0.0));
RUN(PRINT("{:F}\n", -1.0 / 0.0));

// {:#} — alternate form on floats
RUN(PRINT("{:#g}\n", 1.0));
RUN(PRINT("{:#g}\n", 100.0));
RUN(PRINT("{:#.0f}\n", 3.0));
RUN(PRINT("{:#e}\n", 1.0));

// Precision on g
RUN(PRINT("{:.1g}\n", 3.14159));
RUN(PRINT("{:.2g}\n", 3.14159));
RUN(PRINT("{:.4g}\n", 3.14159));
RUN(PRINT("{:.1g}\n", 0.00123));
RUN(PRINT("{:.6g}\n", 1234567.0));

// Alt form forces decimal point with precision 0
RUN(PRINT("{:#.0f}\n", 3.0));
RUN(PRINT("{:#.0f}\n", 100.0));
RUN(PRINT("{:#.0f}\n", 0.0));
RUN(PRINT("{:#.0e}\n", 1.0));

// Float edge values
RUN(PRINT("{:e}\n", 2.2250738585072014e-308));
RUN(PRINT("{:e}\n", 1.7976931348623157e+308));
RUN(PRINT("{:g}\n", 1.7976931348623157e+308));
RUN(PRINT("{:f}\n", 0.0));
RUN(PRINT("{:f}\n", -0.0));

// Float with explicit spec on float (not double)
RUN(PRINT("{:f}\n", 3.14f));
RUN(PRINT("{:e}\n", 3.14f));
RUN(PRINT("{:g}\n", 3.14f));
RUN(PRINT("{:.2f}\n", 3.14f));
RUN(PRINT("{:+.4e}\n", 0.001f));
RUN(PRINT("{:012.3f}\n", 42.0f));

// Float extremes with explicit specs (float 32-bit)
RUN(PRINT("{:g}\n", std::numeric_limits<float>::max()));
RUN(PRINT("{:e}\n", std::numeric_limits<float>::max()));
RUN(PRINT("{:g}\n", -std::numeric_limits<float>::max()));
RUN(PRINT("{:g}\n", std::numeric_limits<float>::min()));   // smallest normal
RUN(PRINT("{:e}\n", std::numeric_limits<float>::min()));
RUN(PRINT("{:g}\n", std::numeric_limits<float>::denorm_min()));

// Negative zero
RUN(PRINT("{:f}\n", -0.0f));
RUN(PRINT("{:e}\n", -0.0f));
RUN(PRINT("{:g}\n", -0.0f));
RUN(PRINT("{:g}\n", -0.0));
RUN(PRINT("{:e}\n", -0.0));

// NaN (quiet)
RUN(PRINT("{:f}\n", std::numeric_limits<float>::quiet_NaN()));
RUN(PRINT("{:g}\n", std::numeric_limits<float>::quiet_NaN()));
RUN(PRINT("{:F}\n", std::numeric_limits<float>::quiet_NaN()));
RUN(PRINT("{:f}\n", std::numeric_limits<double>::quiet_NaN()));

// Negative inf (float)
RUN(PRINT("{:f}\n", -1.0f / 0.0f));
RUN(PRINT("{:g}\n", -1.0f / 0.0f));
RUN(PRINT("{:F}\n", -1.0f / 0.0f));

// Default format for float/double specials (covers ACPP dragonbox inf/nan path)
RUN(PRINT("{}\n", 1.0 / 0.0));
RUN(PRINT("{}\n", -1.0 / 0.0));
RUN(PRINT("{}\n", 0.0 / 0.0));
RUN(PRINT("{}\n", 1.0f / 0.0f));
RUN(PRINT("{}\n", -1.0f / 0.0f));
RUN(PRINT("{}\n", 0.0f / 0.0f));

// {:g} with precision that triggers prec=1 fallback
RUN(PRINT("{:.0g}\n", 3.14));
RUN(PRINT("{:.0g}\n", 0.5));

// More float values with explicit spec (exercises format_float paths)
RUN(PRINT("{:g}\n", 0.0));
RUN(PRINT("{:g}\n", -0.0));
RUN(PRINT("{:e}\n", 0.0f));
RUN(PRINT("{:f}\n", 0.0f));
RUN(PRINT("{:g}\n", 0.0f));
RUN(PRINT("{:g}\n", 1.0e-7));
RUN(PRINT("{:g}\n", 9.99e+4));

// {:g} rounding at fixed/scientific boundary
RUN(PRINT("{:g}\n", 9.999995e+5));
RUN(PRINT("{:g}\n", 9.9999950000001e+5));
RUN(PRINT("{:g}\n", 9.9e+5));

// fmt_fixed rounding: midpoint ties with IEEE double-rounding
RUN(PRINT("{:.2g}\n", 9.95));
RUN(PRINT("{:.1g}\n", 0.95));
RUN(PRINT("{:.4g}\n", 9999.5));
RUN(PRINT("{:.0f}\n", 0.5));
RUN(PRINT("{:.0f}\n", 1.5));
RUN(PRINT("{:.0f}\n", 2.5));
RUN(PRINT("{:.1f}\n", 9.95));

// Default format for zero (exercises dragonbox format_shortest zero path)
RUN(PRINT("{}\n", 0.0));
RUN(PRINT("{}\n", -0.0));
RUN(PRINT("{}\n", 0.0f));
RUN(PRINT("{}\n", -0.0f));

// Default format for small values (format_shortest leading zeros path)
RUN(PRINT("{}\n", 0.001));
RUN(PRINT("{}\n", 0.001f));


// ACPP-only: dragonbox paths that round numbers don't hit. These default
// `{}` outputs use shortest-round-trip representation which std::format
// matches but printf("%g") (DPC++ path) does not (capped at 6 sig digits).
//   - round-up fallback in shorter_interval_case<double> (large powers of 2)
//   - small_divisor path → check_divisibility_and_divide_by_pow10
//     + compute_mul_parity<double> + umul192_lower128
//     + remove_trailing_zeros<uint64_t> inner loop
#if FFMT_BUFFER_PATH
RUN(PRINT("{}\n", 1.2676506002282294e+30));  // 2^100 (shorter_interval round-up)
RUN(PRINT("{}\n", 7.888609052210118e-31));   // 2^-100 (shorter_interval round-up)
RUN(PRINT("{}\n", 0.30000000000000004));     // small_divisor, full precision
RUN(PRINT("{}\n", 1234567.89));              // small_divisor
RUN(PRINT("{}\n", 1.234));                   // small_divisor
RUN(PRINT("{}\n", 7.89));                    // small_divisor

// Dragonbox small_divisor + parity-check coverage.  These doubles are
// chosen so r ∈ {0, deltai, > deltai}, hitting the three branches in
// compute_nearest_normal<double>: the right-endpoint-tie `goto
// small_divisor` (line ~666), the unconditional `goto small_divisor`
// (line ~669), and the parity-disambiguation path (lines ~671-675).
RUN(PRINT("{}\n", 9007199254740994.0));      // 2^53 + 2 — boundary, exact
RUN(PRINT("{}\n", 1.4012984643248171e-44));  // exercises compute_mul_parity
RUN(PRINT("{}\n", 0.1));                     // 0.1 binary — tie-break parity
RUN(PRINT("{}\n", 0.2));                     // 0.2 binary — tie-break parity
RUN(PRINT("{}\n", 9.999999999999998));       // r > deltai branch
RUN(PRINT("{}\n", 4.9406564584124654e-324)); // smallest subnormal double
RUN(PRINT("{}\n", 2.2250738585072009e-308)); // largest subnormal double

// Same paths for float-precision dragonbox (compute_mul_parity<float>).
RUN(PRINT("{}\n", 0.1f));
RUN(PRINT("{}\n", 0.2f));
RUN(PRINT("{}\n", 16777218.0f));             // 2^24 + 2 — fixed/sci boundary
RUN(PRINT("{}\n", 1.4013e-45f));             // float denorm range

// Fixed/scientific boundary — std::format picks the shorter form, tie → fixed.
// Buffer path uses dragonbox + use_fixed() length comparison; the SPIRV path
// uses printf %g which follows a different rule (exp < -4 || exp >= precision).
RUN(PRINT("{}\n", 1000000.0));         // exp=6, sig=1 → sci (1e+06 vs 1000000)
RUN(PRINT("{}\n", 100000.0));          // exp=5, sig=1 → sci (1e+05 vs 100000)
RUN(PRINT("{}\n", 10000.0));           // exp=4, sig=1 → tie → fixed (10000)
RUN(PRINT("{}\n", 1234567890.0));      // exp=9, sig=10 → fixed
RUN(PRINT("{}\n", 1234567890123456.0));// exp=15, sig=16 → fixed
RUN(PRINT("{}\n", 1.0e15));            // exp=15, sig=1 → sci
RUN(PRINT("{}\n", 0.0001));            // exp=-4, sig=1 → sci (1e-04)
#endif


// ── Exact digits beyond 2^53 ────────────────────────────────────────────────
// The buffer path used to compute round(val·10^prec) in a double/uint64,
// which is exact only while that product stays below 2^53: {:f} of 1e15
// printed "0.000000" and {:.20f} of 0.1 lost its trailing "555". Every case
// here has val·10^prec >= 2^53 (or a hard rounding tie) and compares against
// std::format's exact digits.
RUN(PRINT("{:f}\n", 1e15));
RUN(PRINT("{:f}\n", 1e22));
RUN(PRINT("{:f}\n", 123456789012345678.0));
RUN(PRINT("{:.3f}\n", 9007199254740993.0));             // 2^53 + 1 → rounds to even
RUN(PRINT("{:.20f}\n", 0.1));
RUN(PRINT("{:.30f}\n", 1.0 / 3.0));
RUN(PRINT("{:.17f}\n", 0.30000000000000004));
RUN(PRINT("{:.10f}\n", 1e-5));
RUN(PRINT("{:.60f}\n", 5e-60));
RUN(PRINT("{:f}\n", 1.7976931348623157e308));          // 309 integer digits
RUN(PRINT("{:.17e}\n", 0.1));
RUN(PRINT("{:.20e}\n", 1e23));                          // 1e23 is not a double
RUN(PRINT("{:.16e}\n", 2.2250738585072014e-308));       // DBL_MIN
RUN(PRINT("{:e}\n", 4.9406564584124654e-324));         // smallest subnormal
RUN(PRINT("{:.25g}\n", 1e24));
RUN(PRINT("{:.17g}\n", 0.1));
RUN(PRINT("{:.40g}\n", 1.0 / 3.0));
RUN(PRINT("{:g}\n", 1.7976931348623157e308));
// Round half to even on exact binary ties, and carries that change the
// exponent or add an integer digit.
RUN(PRINT("{:.1f}\n", 0.25));
RUN(PRINT("{:.1f}\n", 0.35));                           // 0.35 is just below the tie
RUN(PRINT("{:.2f}\n", 1.125));
RUN(PRINT("{:.0f}\n", 3.5));
RUN(PRINT("{:.0f}\n", 1e16 + 2));
RUN(PRINT("{:.2f}\n", 999.996));
RUN(PRINT("{:.0e}\n", 9.5));
RUN(PRINT("{:.3e}\n", 9.9995));
RUN(PRINT("{:.2e}\n", 1.125e10));
RUN(PRINT("{:g}\n", 9.9999995));
RUN(PRINT("{:g}\n", 999999.5));
RUN(PRINT("{:g}\n", 0.000099999995));
RUN(PRINT("{:.3g}\n", 0.00009995));
// Precision on float args: digits come from the float's exact value.
RUN(PRINT("{:.10f}\n", 0.1f));
RUN(PRINT("{:.12e}\n", 3.14159f));
RUN(PRINT("{:f}\n", 3.4028234663852886e38f));          // FLT_MAX



// Default presentation always uses fmt's shortest form on every backend.
#define OR_ON_SPECIFIERS(g_out, fmt_str, v) PRINT(fmt_str, v)
RUN(OR_ON_SPECIFIERS("1.15292e+18", "{}\n", 1152921504606846976.0));
RUN(OR_ON_SPECIFIERS("1.23457e+17", "{}\n", 123456789012345678.0));
RUN(OR_ON_SPECIFIERS("9.22337e+18", "{}\n", 9.223372036854775807e18));
RUN(OR_ON_SPECIFIERS("4.72237e+21", "{}\n", 4.722366482869645e21));
RUN(OR_ON_SPECIFIERS("-1.07431e+12", "{}\n", -1074311135232.0f));
RUN(OR_ON_SPECIFIERS("1.07374e+09", "{}\n", 16777217.0f * 64));
RUN(OR_ON_SPECIFIERS("1.23457e+06", "{}\n", 1234567.0));
RUN(PRINT("{}\n", 1e16));
RUN(OR_ON_SPECIFIERS("1.23457e+07", "{}\n", 12345678.0f));
RUN(OR_ON_SPECIFIERS("[     3.14159]", "[{:12}]\n", 3.14159265));
RUN(PRINT("[{:<12}]\n", 0.1));
RUN(PRINT("[{:+}]\n", 1e100));
RUN(PRINT("[{:012}]\n", -2.5e-7));
RUN(PRINT("[{:10}]\n", 0.1f));
RUN(OR_ON_SPECIFIERS("[1.00000]", "[{:#}]\n", 1.0));
RUN(OR_ON_SPECIFIERS("[1.00000e+20]", "[{:#}]\n", 1e20));
RUN(OR_ON_SPECIFIERS("[0.500000]", "[{:#}]\n", 0.5));
RUN(PRINT("[{:>12}]\n", -0.0));
RUN(PRINT("[{:<+10}]\n", 1.5e-300));
RUN(OR_ON_SPECIFIERS("[1.00000e-07]", "[{:#}]\n", 1e-7f));
RUN(PRINT("[{:012}]\n", std::numeric_limits<double>::infinity()));
RUN(PRINT("[{:.3}]\n", 3.14159265));                    // no type + precision = g
#undef OR_ON_SPECIFIERS

// Precision above 127 (stored in 8 bits, it wrapped negative).
RUN(PRINT("{:.200f}\n", 0.1));
RUN(PRINT("{:.150e}\n", 1.0 / 3.0));

#endif
