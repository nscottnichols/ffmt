#ifndef TEST_INC
#define TEST_NAME fast_math
#define TEST_INC "test_fast_math.cpp"
#include "test_select_body.inc"
#else

#if defined(SYCL_LANGUAGE_VERSION) || defined(_OPENMP) || defined(__CUDACC__)
#error "test_fast_math is a host-only test (DAZ is the host CPU's mode)"
#endif

// Built with -ffast-math (make test-fast-math). On x86 that enables DAZ
// (denormals-are-zero), and icpx does so by default: a subnormal then
// compares equal to 0.0, so the old `val == 0.0` checks printed 5e-324 as
// "0". Zero tests are now done on the bits.
//
// No reference compiled with these flags can be trusted: {fmt} itself prints
// {:e} of 5e-324 as "0.000000e+00" under DAZ (as does libstdc++'s
// std::format), and -0.0 as "0" (-fno-signed-zeros). Subnormal expectations are therefore literals, taken from
// {fmt} built without -ffast-math.
//
// Values go through a volatile (host-only test, never kernel code) so the
// widening and zero tests happen at run time; with plain literals the
// compiler folds them and DAZ never gets a chance to flush anything.
[[maybe_unused]] auto opaque = [](auto v) { volatile decltype(v) x = v; return x; };
#ifdef FFMT_STD_PATH
#define EXPECT(want, fmt_str, v) printf("%s\n", want)
#else
#define EXPECT(want, fmt_str, v) PRINT(fmt_str "\n", opaque(v))
#endif

RUN(EXPECT("4.940656e-324", "{:e}", 4.9406564584124654e-324));
RUN(EXPECT("4.941e-324", "{:.3e}", 4.9406564584124654e-324));
RUN(EXPECT("4.94066e-324", "{:g}", 4.9406564584124654e-324));
RUN(EXPECT("2.225074e-308", "{:e}", 2.2250738585072009e-308));
RUN(EXPECT("1.401298e-45", "{:e}", 1e-45f));
RUN(EXPECT("-0", "{}", -0.0)); // -fno-signed-zeros: {fmt} built with it prints "0"
RUN(EXPECT("9.9999999999999694493e-311", "{:.20g}", 1e-310));
RUN(EXPECT("5e-324", "{}", 4.9406564584124654e-324));
RUN(EXPECT("-5e-324", "{}", -4.9406564584124654e-324));
RUN(EXPECT("2.225073858507201e-308", "{}", 2.2250738585072009e-308));
RUN(EXPECT("1e-45", "{}", 1e-45f));
RUN(EXPECT("0x0.0000000000001p-1022", "{:a}", 4.9406564584124654e-324));
RUN(EXPECT("0x0.fffffffffffffp-1022", "{:a}", 2.2250738585072009e-308));
RUN(EXPECT("0x1p-149", "{:a}", 1e-45f));
#undef EXPECT

// Normal values: {fmt} is still a valid reference here.
RUN(PRINT("{:.17g}\n", 0.1));
RUN(PRINT("{:f}\n", 1e15));
RUN(PRINT("{:.3e}\n", 9.9995));
RUN(PRINT("{:.0f}\n", 2.5));
RUN(PRINT("{}\n", 0.0));

#endif
