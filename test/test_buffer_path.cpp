#ifndef TEST_INC
#define TEST_NAME buffer_path
#define TEST_INC "test_buffer_path.cpp"
#include "test_select_body.inc"
#else

// ── Integers (from test_integers.cpp) ──
RUN(PRINT("{:b}\n", 255));
RUN(PRINT("{:B}\n", 255));
SKIP_IF(FFMT_PTX_CLANG_O0, "ptx-clang-o0", PRINT("{:#x}\n", 255));
SKIP_IF(FFMT_PTX_CLANG_O0, "ptx-clang-o0", PRINT("{:#b}\n", 255));
RUN(PRINT("{:x}\n", -2147483647 - 1));
RUN(PRINT("{:b}\n", -2147483647 - 1));
RUN(PRINT("{:b}\n", static_cast<uint64_t>(18446744073709551615ULL)));
RUN(PRINT("{:#b}\n", 0));
RUN(PRINT("{:x}\n", static_cast<int>(-1)));
RUN(PRINT("{:#x}\n", 0));

// INT64_MIN with non-decimal bases
RUN(PRINT("{:x}\n", static_cast<int64_t>(-9223372036854775807LL - 1)));
RUN(PRINT("{:X}\n", static_cast<int64_t>(-9223372036854775807LL - 1)));
RUN(PRINT("{:o}\n", static_cast<int64_t>(-9223372036854775807LL - 1)));
RUN(PRINT("{:b}\n", static_cast<int64_t>(-9223372036854775807LL - 1)));
SKIP_IF(FFMT_PTX_CLANG_O0, "ptx-clang-o0", PRINT("{:#x}\n", static_cast<int64_t>(-9223372036854775807LL - 1)));
SKIP_IF(FFMT_PTX_CLANG_O0, "ptx-clang-o0", PRINT("{:#b}\n", static_cast<int64_t>(-9223372036854775807LL - 1)));

// INT_MIN (32-bit) with non-decimal bases
RUN(PRINT("{:o}\n", -2147483647 - 1));
RUN(PRINT("{:X}\n", -2147483647 - 1));
SKIP_IF(FFMT_PTX_CLANG_O0, "ptx-clang-o0", PRINT("{:#o}\n", -2147483647 - 1));
SKIP_IF(FFMT_PTX_CLANG_O0, "ptx-clang-o0", PRINT("{:#b}\n", -2147483647 - 1));

// Sign + alternate + zero-pad combinations
SKIP_IF(FFMT_PTX_CLANG_O0, "ptx-clang-o0", PRINT("{:+#010x}\n", 255));
SKIP_IF(FFMT_PTX_CLANG_O0, "ptx-clang-o0", PRINT("{:+#010x}\n", -1));
SKIP_IF(FFMT_PTX_CLANG_O0, "ptx-clang-o0", PRINT("{: #010x}\n", 255));
SKIP_IF(FFMT_PTX_CLANG_O0, "ptx-clang-o0", PRINT("{: #010x}\n", -1));
SKIP_IF(FFMT_PTX_CLANG_O0, "ptx-clang-o0", PRINT("{:+#010b}\n", 42));
SKIP_IF(FFMT_PTX_CLANG_O0, "ptx-clang-o0", PRINT("{:+#010b}\n", -1));
SKIP_IF(FFMT_PTX_CLANG_O0, "ptx-clang-o0", PRINT("{: #010b}\n", 42));
SKIP_IF(FFMT_PTX_CLANG_O0, "ptx-clang-o0", PRINT("{:+#010o}\n", 255));
SKIP_IF(FFMT_PTX_CLANG_O0, "ptx-clang-o0", PRINT("{: #010o}\n", 255));
SKIP_IF(FFMT_PTX_CLANG_O0, "ptx-clang-o0", PRINT("{:+#010X}\n", 255));
SKIP_IF(FFMT_PTX_CLANG_O0, "ptx-clang-o0", PRINT("{:+#020b}\n", -42));

// ── Floats (from test_floats.cpp) ──
RUN(PRINT("{}\n", 1.0e10f));
RUN(PRINT("{}\n", 1.0e-10f));
RUN(PRINT("{}\n", 1.0e100));
RUN(PRINT("{}\n", 1.0e-100));

// Float extremes — default format
RUN(PRINT("{}\n", std::numeric_limits<float>::max()));
SKIP_IF(FFMT_PTX_CLANG_O0, "ptx-clang-o0", PRINT("{}\n", -std::numeric_limits<float>::max()));
RUN(PRINT("{}\n", std::numeric_limits<float>::min()));
RUN(PRINT("{}\n", std::numeric_limits<float>::denorm_min()));
RUN(PRINT("{}\n", -0.0f));
RUN(PRINT("{}\n", std::numeric_limits<double>::max()));
RUN(PRINT("{}\n", std::numeric_limits<double>::min()));
RUN(PRINT("{}\n", -0.0));
RUN(PRINT("{}\n", std::numeric_limits<float>::infinity()));
RUN(PRINT("{}\n", -std::numeric_limits<float>::infinity()));
RUN(PRINT("{}\n", std::numeric_limits<float>::quiet_NaN()));

SKIP_IF(FFMT_PTX_CLANG_O0, "ptx-clang-o0", PRINT("{:a}\n", 3.14159));
RUN(PRINT("{:A}\n", 3.14159));
RUN(PRINT("{:a}\n", 0.0));
SKIP_IF(FFMT_PTX_CLANG_O0, "ptx-clang-o0", PRINT("{:a}\n", -0.0));
SKIP_IF(FFMT_PTX_CLANG_O0, "ptx-clang-o0", PRINT("{:a}\n", 1.0));
SKIP_IF(FFMT_PTX_CLANG_O0, "ptx-clang-o0", PRINT("{:#a}\n", 3.14159));
RUN(PRINT("{:e}\n", 5e-324));
RUN(PRINT("{:g}\n", 5e-324));

// ── Strings (from test_strings.cpp) ──
RUN(PRINT("{:*^10}\n", true));
RUN(PRINTLN("{:08x}", 255));
RUN(PRINT("{:10c}\n", 'A'));
RUN(PRINT("{:^10c}\n", 'D'));
RUN(PRINT("{:*>5c}\n", 'X'));
RUN(PRINT("{:*<5c}\n", 'X'));
RUN(PRINT("{:*^5c}\n", 'X'));
#define _S (FFMT_SPIRV_O0 || FFMT_PTX_CLANG_O0)
SKIP_IF(_S, "spirv-o0|ptx-clang-o0", PRINT("{:20s}\n", "hello"));
SKIP_IF(_S, "spirv-o0|ptx-clang-o0", PRINT("{:*^10.5s}\n", "hello world"));
SKIP_IF(_S, "spirv-o0|ptx-clang-o0", PRINT("{:*<20s}\n", "hello"));
SKIP_IF(_S, "spirv-o0|ptx-clang-o0", PRINT("{:*>20s}\n", "hello"));
SKIP_IF(_S, "spirv-o0|ptx-clang-o0", PRINT("{:*^20s}\n", "hello"));
#undef _S
// Multi-byte UTF-8 fill (2, 3 and 4 bytes)
RUN(PRINT("[{:─<6}]\n", 42));
RUN(PRINT("[{:─>+8}]\n", 42));
RUN(PRINT("[{:─^9.2f}]\n", 3.14159));
RUN(PRINT("[{:😀>4c}]\n", 'x'));
#define _S (FFMT_SPIRV_O0 || FFMT_PTX_CLANG_O0)
SKIP_IF(_S, "spirv-o0|ptx-clang-o0", PRINT("[{:é^9}]\n", "abc"));
SKIP_IF(_S, "spirv-o0|ptx-clang-o0", PRINT("┌{0:─^{2}}┐\n│{1: ^{2}}│\n", "", "Hello", 9));
// Width and string precision count display columns, like {fmt}: 1 per code
// point, 2 for East Asian wide and emoji, 1 per invalid byte.
SKIP_IF(_S, "spirv-o0|ptx-clang-o0", PRINT("[{:^10}]\n", "hôpital"));
SKIP_IF(_S, "spirv-o0|ptx-clang-o0", PRINT("[{:*>8}]\n", "静夜思"));
SKIP_IF(_S, "spirv-o0|ptx-clang-o0", PRINT("[{:*<8.3}]\n", "hôpital"));
SKIP_IF(_S, "spirv-o0|ptx-clang-o0", PRINT("[{:.3}]\n", "静夜思"));
SKIP_IF(_S, "spirv-o0|ptx-clang-o0", PRINT("[{:.4}]\n", "静夜思"));
SKIP_IF(_S, "spirv-o0|ptx-clang-o0", PRINT("[{:─^7}]\n", "😀"));
SKIP_IF(_S, "spirv-o0|ptx-clang-o0", PRINT("[{:>5}]\n", "\xff\xe2\x94"));
// Overlong '/', a UTF-16 surrogate, and > U+10FFFF: each byte is invalid.
SKIP_IF(_S, "spirv-o0|ptx-clang-o0", PRINT("[{:>6}]\n", "\xc0\xaf"));
SKIP_IF(_S, "spirv-o0|ptx-clang-o0", PRINT("[{:>6}]\n", "\xed\xa0\x80"));
SKIP_IF(_S, "spirv-o0|ptx-clang-o0", PRINT("[{:>6}]\n", "\xf4\x90\x80\x80"));
// A spec with no width or precision: copied without measuring.
SKIP_IF(_S, "spirv-o0|ptx-clang-o0", PRINT("[{:s}] [{:<}]\n", "静夜思", "hôpital"));
SKIP_IF(_S, "spirv-o0|ptx-clang-o0", PRINT("[{:>{}.{}}]\n", "静夜思", 8, 5));
#undef _S

// ── Layout (from test_layout.cpp) ──
RUN(PRINT("{:^10d}\n", 42));
RUN(PRINT("{:*<10d}\n", 42));
RUN(PRINT("{:*>10d}\n", 42));
RUN(PRINT("{:*^10d}\n", 42));
RUN(PRINT("{:#^20b}\n", 255));
RUN(PRINT("{:+010x}\n", -1));
RUN(PRINT("{:>20b}\n", static_cast<uint8_t>(255)));
RUN(PRINT("{:*>5d}\n", -42));
RUN(PRINT("{:<10x}\n", 255u));
RUN(PRINT("{:>10x}\n", 255u));
RUN(PRINT("{:^10x}\n", 255u));
RUN(PRINT("{:<10o}\n", 255u));
RUN(PRINT("{:>10o}\n", 255u));
RUN(PRINT("{:*<10x}\n", 255u));
RUN(PRINT("{:*>10x}\n", 255u));
RUN(PRINT("{:*^10x}\n", 255u));
RUN(PRINT("{:*<10o}\n", 255u));
RUN(PRINT("{:1x}\n", 0xDEAD));
RUN(PRINT("{:1b}\n", 255));
RUN(PRINT("{:*<10}\n", true));
RUN(PRINT("{:*>10}\n", false));
RUN(PRINT("{:*^10}\n", true));

// ── Misc (from test_misc.cpp) ──
SKIP_IF(FFMT_PTX_CLANG_O0, "ptx-clang-o0", PRINT("{:#010x}\n", 255));
RUN(PRINT("{:*>15.6f}\n", 3.14));
RUN(PRINT("hex={:#x} pi={:.4f}\n", 255, 3.14159));
RUN(PRINT("{:a}\n", 1.0 / 0.0));
RUN(PRINT("{:A}\n", 0.0 / 0.0));
SKIP_IF(FFMT_PTX_CLANG_O0, "ptx-clang-o0", PRINT("{:a}\n", 0.125));
SKIP_IF(FFMT_PTX_CLANG_O0, "ptx-clang-o0", PRINT("{0:x} {0:d} {0:b}\n", 255));
RUN(PRINT("{:{}d}\n", 42, 10));
RUN(PRINT("{:{}d}\n", 42, 1));
RUN(PRINT("{:.{}f}\n", 3.14159, 2));
RUN(PRINT("{:.{}f}\n", 4.14159, 0));
RUN(PRINT("{:{}.{}f}\n", 5.14, 15, 6));
RUN(PRINT("{0:{1}x}\n", 255, 10));
RUN(PRINT("{0:{2}.{1}f}\n", 6.14, 4, 20));
RUN(PRINT("{:+x}\n", 255));
RUN(PRINT("{:+x}\n", -1));
RUN(PRINT("{: x}\n", 255));
RUN(PRINT("{: x}\n", -1));
RUN(PRINT("{:+o}\n", 255));
RUN(PRINT("{: o}\n", 255));
RUN(PRINT("{:+b}\n", 42));
RUN(PRINT("{: b}\n", 42));
RUN(PRINT("{:+b}\n", -1));
SKIP_IF(FFMT_PTX_CLANG_O0, "ptx-clang-o0", PRINT("{:+a}\n", 3.14));
SKIP_IF(FFMT_PTX_CLANG_O0, "ptx-clang-o0", PRINT("{:+a}\n", -3.14));
RUN(PRINT("{: a}\n", 3.14));
SKIP_IF(FFMT_PTX_CLANG_O0, "ptx-clang-o0", PRINT("{: a}\n", -3.14));
RUN(PRINT("{:+A}\n", 1.0));
RUN(PRINT("{:b}\n", 0u));
RUN(PRINT("{:B}\n", 0u));
RUN(PRINT("{:a}\n", 0.0));
RUN(PRINT("{:{}d}\n", 42, 0));
RUN(PRINT("{:.{}f}\n", 3.14159, 10));
RUN(PRINT("{:{}c}\n", 'A', 5));
RUN(PRINT("{:{}}\n", true, 10));
RUN(PRINT("{:{}.{}f}\n", 3.14, 20, 0));
RUN(PRINT("{:#010x}\n", 0));
RUN(PRINT("{:#o}\n", 0));
SKIP_IF(FFMT_PTX_CLANG_O0, "ptx-clang-o0", PRINT("{:#b}\n", 1));
SKIP_IF(FFMT_PTX_CLANG_O0, "ptx-clang-o0", PRINT("{:#010b}\n", 1));
SKIP_IF(FFMT_PTX_CLANG_O0, "ptx-clang-o0", PRINT("{:#X}\n", 255));
SKIP_IF(FFMT_PTX_CLANG_O0, "ptx-clang-o0", PRINT("{:#o}\n", 8));
SKIP_IF(FFMT_PTX_CLANG_O0, "ptx-clang-o0", PRINT("{:<20a}\n", 3.14));
SKIP_IF(FFMT_PTX_CLANG_O0, "ptx-clang-o0", PRINT("{:>20a}\n", 3.14));
SKIP_IF(FFMT_PTX_CLANG_O0, "ptx-clang-o0", PRINT("{:^20a}\n", 3.14));
SKIP_IF(FFMT_PTX_CLANG_O0, "ptx-clang-o0", PRINT("{:*>20a}\n", 3.14));

// Null-pointer formatting — buffer-path-only because the specifiers path
// routes through libc printf which emits "(nil)" for null pointers, while
// std::format (and our buffer-path implementation) emit "0x0".
RUN(PRINT("{}\n",         static_cast<void*>(nullptr)));
RUN(PRINT("{:p}\n",       static_cast<void*>(nullptr)));
// Centre alignment on pointers — '^' is buffer-path-only generally.
RUN(PRINT("[{:^20p}]\n",  reinterpret_cast<void*>(0xdeadbeef)));

// ── Type-instantiation coverage ──
// write_int_rt<int8_t/short/int64_t/uint64_t/long>: integer specs through
// less common types so the per-type buffer_path::write_int_rt instantiations
// actually get exercised by coverage.
RUN(PRINT("{:b}\n", static_cast<int8_t>(-5)));
RUN(PRINT("{:x}\n", static_cast<int8_t>(-5)));
RUN(PRINT("{:b}\n", static_cast<short>(-300)));
RUN(PRINT("{:x}\n", static_cast<short>(-300)));
RUN(PRINT("{:b}\n", static_cast<int64_t>(-1)));
RUN(PRINT("{:x}\n", static_cast<int64_t>(-1)));
RUN(PRINT("{:b}\n", static_cast<uint64_t>(0xDEADBEEFCAFEULL)));
RUN(PRINT("{:o}\n", static_cast<uint64_t>(0xDEADBEEFCAFEULL)));
SKIP_IF(FFMT_PTX_CLANG_O0, "ptx-clang-o0", PRINT("{:b}\n", 1L));
SKIP_IF(FFMT_PTX_CLANG_O0, "ptx-clang-o0", PRINT("{:#x}\n", -1L));

// hex_float_to_buf_rt<float>: {:a} on float (tests above only use double).
// PTX-clang at -O0 emits the exponent letter as uppercase 'P' even for
// the lowercase '{:a}' spec — distinct from the prefix/sign cluster above.
SKIP_IF(FFMT_PTX_CLANG_O0, "ptx-clang-o0", PRINT("{:a}\n", 3.14f));
RUN(PRINT("{:A}\n", 3.14f));
SKIP_IF(FFMT_PTX_CLANG_O0, "ptx-clang-o0", PRINT("{:a}\n", 0.0f));
SKIP_IF(FFMT_PTX_CLANG_O0, "ptx-clang-o0", PRINT("{:a}\n", -0.0f));
SKIP_IF(FFMT_PTX_CLANG_O0, "ptx-clang-o0", PRINT("{:a}\n", std::numeric_limits<float>::infinity()));
SKIP_IF(FFMT_PTX_CLANG_O0, "ptx-clang-o0", PRINT("{:#a}\n", 1.5f));

// resolve_int_arg dispatch for type combinations not hit elsewhere:
// dynamic width/precision with mixed integer types and floats.
RUN(PRINT("{:{}d}\n", 42, static_cast<short>(8)));      // resolve_int_arg<short>
RUN(PRINT("{:{}d}\n", 42, static_cast<int64_t>(8)));    // resolve_int_arg<int64_t>
RUN(PRINT("{:{}d}\n", 42, static_cast<uint64_t>(8)));   // resolve_int_arg<uint64_t>
RUN(PRINT("{:{}d}\n", 42, 8L));                         // resolve_int_arg<long>
RUN(PRINT("{:{}d}\n", 42, static_cast<unsigned>(8)));   // resolve_int_arg<unsigned>
RUN(PRINT("{:{}d}\n", 42, static_cast<int8_t>(8)));     // resolve_int_arg<int8_t>
RUN(PRINT("{:{}d}\n", 42, static_cast<uint8_t>(8)));    // resolve_int_arg<uint8_t>

// ── Writes near the buffer cap ──────────────────────────────────────────────
// Each case starts with 120 filler chars so the next write straddles the
// 128-byte cap. Before the fix these wrote past fmt_buf::data (up to 23 bytes
// for 64 binary digits) — invisible in the truncated output, so they only
// fail under -fsanitize=address. Keep running the host suite with ASan.
#define FILL120 "........................................................................................................................"
RUN(PRINT("{}{:b}\n", FILL120, ~0ull));                       // write_uint_raw, base 2
RUN(PRINT("{}{:o}\n", FILL120, ~0ull));                       // write_uint_raw, base 8
RUN(PRINT("{}{:#X}\n", FILL120, ~0ull));                      // prefix + base 16
RUN(PRINT("{}{}\n", FILL120, ~0ull));                         // write_arg_default
RUN(PRINT("{}{}\n", FILL120, reinterpret_cast<void*>(0x123456789abcdef0ULL)));
RUN(PRINT("{}{:p}\n", FILL120, reinterpret_cast<void*>(0x123456789abcdef0ULL)));
RUN(PRINT("{}{:a}\n", FILL120, 1e300));                       // hex-float exponent digits
RUN(PRINT("{}{}{}\n", FILL120, FILL120, 12345));              // already full
// println keeps its '\n' even when the line is truncated; before the fix the
// next "Test N" header was glued onto the truncated line.
RUN(PRINTLN("{}{}", FILL120, FILL120));
#undef FILL120

// ── Null char* ──────────────────────────────────────────────────────────────
// Match the oracle's rejection of null strings, with recoverable device status.
#ifdef FFMT_STD_PATH
#define CHECK_NULL(pattern) do {                                             \
  bool rejected = false;                                                    \
  try { (void)::fmt::format(::fmt::runtime(pattern), static_cast<const char*>(nullptr)); } \
  catch (const ::fmt::format_error&) { rejected = true; }                     \
  PRINT("{}\n", rejected);                                                  \
} while (0)
#else
#define CHECK_NULL(pattern) do {                                             \
  char buffer[32];                                                          \
  auto result = ::ffmt::format_to_n(buffer, sizeof(buffer),                    \
      ::ffmt::runtime(pattern), static_cast<const char*>(nullptr));           \
  PRINT("{}\n", result.error != ::ffmt::format_error::none);                 \
} while (0)
#endif
RUN(CHECK_NULL("{}"));
RUN(CHECK_NULL("[{:>10}]"));
RUN(CHECK_NULL("[{:.3}]"));
#undef CHECK_NULL

// ── escape_percent_inplace (host-only unit test) ───────────────────────────
// The ACPP device emit escapes % → %% in place. When the escaped text no
// longer fits, it must keep the longest fitting prefix (never half of a %%
// pair) and keep println's trailing '\n'. Before the fix the right-to-left
// copy overran unread source bytes and the output came out as "%%%%…".
#if !defined(_OPENMP) && !defined(__CUDACC__) && \
    !(defined(SYCL_LANGUAGE_VERSION) || FFMT_COMPILER_ACPP)
{
  auto make_input = [](int n, bool nl) {
    std::string in;
    for (int i = 0; i < n; i++) in += (i % 10 == 9) ? '%' : char('a' + i % 10);
    if (nl) in += '\n';
    return in;
  };
  for (int n : {5, 100, 116, 117, 118, 128}) {
    for (bool nl : {false, true}) {
      std::string in = make_input(n, nl);
#ifdef FFMT_STD_PATH
      // Reference: escape char by char while the result fits in the cap.
      std::string body = nl ? in.substr(0, in.size() - 1) : in, want;
      for (char c : body) {
        std::string e = (c == '%') ? "%%" : std::string(1, c);
        if (want.size() + e.size() > FFMT_BUFFER_SIZE) break;
        want += e;
      }
      if (nl) want += '\n';
      RUN(printf("[%s]\n", want.c_str()));
#else
      ffmt::detail::fmt_buf b;
      for (char c : in) b.data[b.len++] = c;
      ffmt::detail::buffer_path::escape_percent_inplace(b);
      b.data[b.len] = '\0';
      RUN(printf("[%s]\n", b.data));
#endif
    }
  }
}
#endif

// ── Float formatting fixes ──────────────────────────────────────────────────
// '+' on NaN. Buffer path only: on the specifiers path the sign of a NaN is
// up to the printf implementation (glibc: "+nan", macOS libc: "nan").
RUN(PRINT("[{:+}]\n", std::numeric_limits<float>::quiet_NaN()));

// Hex float: precision rounds half up like {fmt} (and may carry into the
// leading digit); '0' pads after the "0x".
RUN(PRINT("{:.0a}\n", 1.5));
RUN(PRINT("{:.0a}\n", 1.96875));
RUN(PRINT("{:.1a}\n", 1.03125));
RUN(PRINT("{:.1a}\n", 1.09375));
RUN(PRINT("{:.3a}\n", 1.0 / 3.0));
RUN(PRINT("{:.20a}\n", 1.0 / 3.0));
RUN(PRINT("{:.2a}\n", 0.0));
RUN(PRINT("{:#.0a}\n", 1.0));
RUN(PRINT("{:.3A}\n", -0.1f));
// Subnormal hex, where the standard libraries disagree with each other and
// with {fmt}: a float is printed as the equivalent double.
RUN(PRINT("{:a}\n", 1e-45f));                           // FLT_TRUE_MIN
RUN(PRINT("{:a}\n", 4.9406564584124654e-324));         // DBL_TRUE_MIN
RUN(PRINT("{:a}\n", 1.1754942e-38f));                   // largest float subnormal
RUN(PRINT("{:a}\n", -2.2250738585072009e-308));        // largest double subnormal
RUN(PRINT("{:.3a}\n", 4.9406564584124654e-324));
RUN(PRINT("{:.1a}\n", 1.3e-44f));
RUN(PRINT("[{:012a}]\n", -1.0));
RUN(PRINT("[{:+012a}]\n", 1.0));
// '0' is ignored for inf/nan.
RUN(PRINT("[{:010}]\n", std::numeric_limits<double>::infinity()));
RUN(PRINT("[{:010f}]\n", -std::numeric_limits<double>::infinity()));
RUN(PRINT("[{:010e}]\n", std::numeric_limits<double>::quiet_NaN()));
// Huge dynamic precision: used to write ~1000 bytes past the buffer.
RUN(PRINT("{:.{}f}\n", 1.5, 1000));
RUN(PRINT("{:.{}e}\n", 1.0 / 3.0, 5000));
RUN(PRINT("{:.{}g}\n", 0.1, 400));

// ── Differential sweep against {fmt} (host only) ───────────────────────────
// Deterministic pseudo-random doubles/floats (raw bit patterns, so every
// exponent and subnormals are hit) plus short decimals that land on rounding
// ties. Far denser than the hand-picked cases above; this is what would have
// caught the >2^53 precision bugs.
#if !defined(_OPENMP) && !defined(__CUDACC__) && \
    !(defined(SYCL_LANGUAGE_VERSION) || FFMT_COMPILER_ACPP)
{
  uint64_t st = 0x9E3779B97F4A7C15ULL;
  auto rnd = [&st]() { st ^= st << 13; st ^= st >> 7; st ^= st << 17; return st; };
  auto finite_bits = [&]<typename F, typename Bits>(size_t n) { // random finite F
    std::vector<F> v;
    while (v.size() < n)
      if (F x = __builtin_bit_cast(F, static_cast<Bits>(rnd())); x - x == 0) v.push_back(x);
    return v;
  };
  auto dv = finite_bits.operator()<double, uint64_t>(400);
  auto fv = finite_bits.operator()<float, uint32_t>(300);
  for (int i = 0; i < 200; i++) {                       // k / 10^j and k / 2^j
    double k = static_cast<double>(rnd() % 100000);
    dv.push_back(k / std::pow(10.0, static_cast<double>(rnd() % 8)));
    dv.push_back(std::ldexp(k + 0.5, -static_cast<int>(rnd() % 12)));
  }
#define SWEEP(vec, fmt) RUN(for (auto v : vec) PRINT(fmt "\n", v))
  SWEEP(dv, "{}");      SWEEP(dv, "{:f}");    SWEEP(dv, "{:.0f}");
  SWEEP(dv, "{:.3f}");  SWEEP(dv, "{:.17f}"); SWEEP(dv, "{:e}");
  SWEEP(dv, "{:.0e}");  SWEEP(dv, "{:.16e}"); SWEEP(dv, "{:.30e}");
  SWEEP(dv, "{:g}");    SWEEP(dv, "{:.1g}");  SWEEP(dv, "{:.17g}");
  SWEEP(dv, "{:#g}");   SWEEP(dv, "{:a}");    SWEEP(dv, "{:.0a}");
  SWEEP(dv, "{:.5a}");  SWEEP(dv, "{:#.3a}"); SWEEP(dv, "{:+15}");
  SWEEP(dv, "{:#}");    SWEEP(dv, "{:.5}");
  SWEEP(fv, "{}");      SWEEP(fv, "{:12}");   SWEEP(fv, "{:e}");
  SWEEP(fv, "{:.10f}"); SWEEP(fv, "{:g}");    SWEEP(fv, "{:a}");
  SWEEP(fv, "{:.2a}");  SWEEP(fv, "{:#}");
#undef SWEEP
}
#endif

#endif
