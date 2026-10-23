// ffmt/base.hpp — std::format-like API for SYCL device kernels
//
// Compile-time converts "{}" / "{:spec}" format strings to printf format
// specifiers, then forwards to sycl::ext::oneapi::experimental::printf.
//
// Usage:
//   ffmt::print<"{} + {} = {}">(a, b, c);
//   FFMT_PRINT("{} + {} = {}", a, b, c);   // macro for nicer syntax

#pragma once

// ── Compiler detection (auto, not user-overridable) ─────────────────────
// FFMT_COMPILER_ACPP gates ACPP-only builtins (__acpp_if_target_sscp,
// AdaptiveCpp_jit, …). SYCL_LANGUAGE_VERSION is the canonical "SYCL is in
// scope" predicate (auto-defined by any SYCL compiler with -fsycl).
#if defined(__ADAPTIVECPP__) || defined(__HIPSYCL__) || defined(__ACPP__)
  #define FFMT_COMPILER_ACPP 1
#else
  #define FFMT_COMPILER_ACPP 0
#endif

// ── Knob 1: formatting path ─────────────────────────────────────────────
// 1 = buffer path  (build a char[] in fmt_buf, emit once via FFMT_EMIT_BUFFER).
// 0 = SPIRV path   (compile-time printf format string, emit via FFMT_EMIT_PRINTF
//                   with live args). The SPIRV path only handles printf-compatible
//                   specs; the buffer path handles the full feature set.
// Default: SPIRV only for the SPIR-V backends (DPC++ / icpx -fsycl, and
// icpx -fiopenmp -fopenmp-targets=spir64), whose printf needs a literal
// format string; buffer everywhere else (host, AdaptiveCpp, clang-OpenMP
// NVPTX, clang-CUDA).
// User may #define before include to override (the host test rig does this
// to exercise both paths through libc printf).
#if !defined(FFMT_BUFFER_PATH)
  #if !FFMT_COMPILER_ACPP && (defined(SYCL_LANGUAGE_VERSION) \
                              || (defined(__INTEL_LLVM_COMPILER) && defined(_OPENMP)))
    #define FFMT_BUFFER_PATH 0
  #else
    #define FFMT_BUFFER_PATH 1
  #endif
#endif

// FMT_HD: function attribute that makes a declaration callable from both
// host and device code under clang-CUDA. CUDA has no namespace/region-
// level "everything below is device code" pragma (unlike OpenMP's
// `#pragma omp declare target`), so we tag every transitively-reachable
// function from the public entry points. Expands to nothing for non-CUDA
// builds so the same header compiles unchanged for SYCL/OpenMP/host.
#if defined(__CUDACC__)
  #define FMT_HD __host__ __device__
#else
  #define FMT_HD
#endif

// ── SYCL headers ─────────────────────────────────────────────────────────
// Include SYCL only when we know SYCL types are in scope; otherwise pull in
// <cstdio> for the host/OpenMP backends (and the host test rig). The same
// derived condition gates the built-in formatter<sycl::range/id/item/nd_item>
// specializations at the bottom of the header.
#if defined(SYCL_LANGUAGE_VERSION) || FFMT_COMPILER_ACPP
  #if FFMT_COMPILER_ACPP
    #include <sycl/sycl.hpp>
  #else
    #include <sycl/ext/oneapi/experimental/builtins.hpp>
  #endif
#else
  #include <cstdio>
#endif

// ── OpenMP-target-SPIR64 auto-install (icpx-only) ────────────────────────
// When the TU is built with `icpx -fiopenmp -fopenmp-targets=spir64` (no
// -fsycl), set up the SPIRV path pointed at __spirv_ocl_printf. The user
// just `#include`s the header and uses FFMT_PRINTLN inside #pragma omp
// target — no scaffolding required.
//
// Gated on __INTEL_LLVM_COMPILER because __spirv_ocl_printf is an icpx
// builtin; nvc++/clang/gcc OpenMP backends use a different mechanism and
// shouldn't get this code path. Also gated off when SYCL is in the same TU
// (SYCL_LANGUAGE_VERSION) — there the SYCL printf wins. ACPP also takes
// precedence; if you're mixing acpp with OpenMP, you probably wanted the
// buffer path.
#if defined(__INTEL_LLVM_COMPILER) && defined(_OPENMP) \
    && !defined(SYCL_LANGUAGE_VERSION) && !FFMT_COMPILER_ACPP \
    && !defined(FFMT_EMIT_PRINTF)
  // __spirv_ocl_printf, declared as a template with both opencl_constant and
  // plain overloads — overload resolution picks the right one based on the
  // AS of our format string (always AS=2 thanks to FFMT_CONST_AS below).
  #if defined(__SPIR__) || defined(__SPIRV__)
    template <typename... Args>
    extern int __spirv_ocl_printf(
        const __attribute__((opencl_constant)) char* Format, Args... args);
    template <typename... Args>
    extern int __spirv_ocl_printf(const char* Format, Args... args);
  #endif
  namespace ffmt { namespace detail {
  #if defined(__SPIR__) || defined(__SPIRV__)
    template <typename... Args>
    inline int omp_printf(const __attribute__((opencl_constant)) char* fmt,
                          Args... args) {
      return __spirv_ocl_printf(fmt, args...);
    }
    template <typename... Args>
    inline int omp_printf(const char* fmt, Args... args) {
      return __spirv_ocl_printf(fmt, args...);
    }
  #else
    // Host pass of the same TU: forward to libc printf.
    template <typename... Args>
    inline int omp_printf(const char* fmt, Args... args) {
      return ::printf(fmt, args...);
    }
  #endif
  }} // namespace ffmt::detail
  #define FFMT_EMIT_PRINTF(...) \
    ::ffmt::detail::omp_printf(__VA_ARGS__)
#endif

// ── NVPTX auto-install: clang-OMP-CUDA, clang-CUDA ──────────────────────
// When the TU targets NVIDIA via either `clang -fopenmp --offload-arch`
// or `clang++ -x cuda`, the buffer path's emit is plain ::printf. Unlike
// icpx-SPIR64, NVPTX's printf accepts any pointer-to-char as the format
// string (verified empirically — runtime buffers, static constexpr arrays,
// all fine). The clang-OMP gate excludes __INTEL_LLVM_COMPILER (icpx
// defines __clang__ too, but routes through the SPIR64 path above).
// SYCL / ACPP / a user-defined hook all win over this.
#if !defined(FFMT_EMIT_BUFFER) && !defined(SYCL_LANGUAGE_VERSION) \
    && !FFMT_COMPILER_ACPP \
    && ( (defined(__clang__) && defined(_OPENMP) && !defined(__INTEL_LLVM_COMPILER)) \
         || defined(__CUDACC__) )
  // printf("%s", buf) — literal format, buffer as %s arg. % chars in the
  // buffer pass through verbatim (printf doesn't re-interpret %s args).
  #define FFMT_EMIT_BUFFER(out, escape_pct) \
    do { (void)(escape_pct); ::printf("%s", (out).data); } while (0)
#endif

// ── Address-space attribute for SPIRV-path format strings ───────────────
// The SPIR backend's printf requires the format string to live in
// address-space 2 (opencl_constant). SYCL has a clang pass
// (SYCLMutatePrintfAddrspacePass) that promotes strings automatically;
// other SPIR producers (OpenMP-target-SPIR64, plain SPIR-V emission) don't.
// Marking our `static constexpr char[]` with the attribute satisfies both:
// SYCL's pass treats it as a no-op, OpenMP-target avoids the
// `RequiresExtension: SPV_EXT_relaxed_printf_string_address_space` link
// error without needing -Xspirv-translator flags.
#if !defined(FFMT_CONST_AS)
  #if defined(__SPIR__) || defined(__SPIRV__)
    #define FFMT_CONST_AS __attribute__((opencl_constant))
  #else
    #define FFMT_CONST_AS
  #endif
#endif

// ── Emit hook defaults ──────────────────────────────────────────────────
// FFMT_EMIT_BUFFER(out, escape_pct) — buffer path: emit out.data as a
//   null-terminated char[]. `escape_pct` is a hint that the emit syscall
//   treats % as a format specifier (CUDA vprintf via ACPP); verbatim
//   writers ignore it.
// FFMT_EMIT_PRINTF(fmt, ...) — SPIRV path: invoke a printf-like sink.
// Either may be user-defined before include to plug in OpenMP, custom
// streams, instrumented emit, etc.
//
// Defaults:
//   ACPP buffer path: JIT-branch on device vs host backend (__acpp_sscp_print
//     vs fputs), escaping % only on the device path.
//   Otherwise buffer: plain fputs (host test, OpenMP, plain CPU).
//   SPIRV with SYCL: sycl::ext::oneapi::experimental::printf.
//   SPIRV without SYCL: ::printf (host test, etc.).
#if !defined(FFMT_EMIT_PRINTF)
  #if defined(SYCL_LANGUAGE_VERSION)
    #define FFMT_EMIT_PRINTF(...) ::sycl::ext::oneapi::experimental::printf(__VA_ARGS__)
  #else
    #define FFMT_EMIT_PRINTF(...) ::printf(__VA_ARGS__)
  #endif
#endif

// Default FFMT_EMIT_BUFFER. Signature: (fmt_buf& out, bool escape_pct).
// `out.data` is already null-terminated when this macro is expanded.
// `escape_pct` is a hint that the backend's emit syscall treats % as a
// format specifier (CUDA vprintf); ignore it if your emit is verbatim.
#if !defined(FFMT_EMIT_BUFFER)
  #if FFMT_COMPILER_ACPP
    // ACPP SSCP: JIT-branch on the actual backend.
    //   host SSCP: fputs (verbatim, no % escaping needed)
    //   device SSCP (PTX/CUDA): __acpp_sscp_print → vprintf, needs % escaping
    #define FFMT_EMIT_BUFFER(out, escape_pct)                              \
      __acpp_if_target_sscp(                                                   \
        ::sycl::AdaptiveCpp_jit::compile_if_else(                              \
          ::sycl::AdaptiveCpp_jit::reflect<                                    \
            ::sycl::AdaptiveCpp_jit::reflection_query::compiler_backend>() ==  \
            ::sycl::AdaptiveCpp_jit::compiler_backend::host,                   \
          [&](){ ::fputs((out).data, stdout); },                               \
          [&](){                                                               \
            if (escape_pct)                                                    \
              ::ffmt::detail::buffer_path::                    \
                escape_percent_inplace(out);                                   \
            (out).data[(out).len] = '\0';                                      \
            __acpp_sscp_print((out).data);                                     \
          });                                                                  \
      )
  #else
    // Everything else (host coverage, OpenMP, plain CPU): verbatim fputs.
    #define FFMT_EMIT_BUFFER(out, escape_pct) \
      do { (void)(escape_pct); ::fputs((out).data, stdout); } while (0)
  #endif
#endif

#include <algorithm> // std::copy_n
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <tuple>
#include <type_traits>
#include <utility> // std::index_sequence


namespace ffmt {

namespace detail {

// ============================================================
// Dragonbox — shortest-decimal float formatting for device code
// ============================================================
// Ported from fmtlib (https://github.com/fmtlib/fmt) v12.0.1
// Original algorithm: https://github.com/jk-jeon/dragonbox
// License: MIT (same as fmt)
//
// Produces the shortest decimal representation of float/double.
// Uses compressed cache tables (216 bytes) — GPU-friendly.

#if FFMT_BUFFER_PATH
// OpenMP target-offload backends (clang/CUDA) need the dragonbox lookup
// tables (and the functions that index into them) visible on the device
// side, otherwise nvlink fails with "Undefined reference to
// double_pow10_significands". `inline constexpr` makes the host emit the
// symbol but doesn't propagate it to NVPTX. The `#pragma omp declare
// target` wrap fixes that for any OpenMP-aware compiler; ignored when
// _OPENMP is undefined (icpx -fsycl, acpp, plain CPU coverage).
#ifdef _OPENMP
#pragma omp declare target
#endif
namespace dragonbox {

struct uint128 {
  uint64_t hi, lo;
  constexpr uint128(uint64_t h, uint64_t l) : hi(h), lo(l) {}
  constexpr uint64_t high() const { return hi; }
  constexpr uint64_t low() const { return lo; }
  constexpr uint128 &operator+=(uint64_t rhs) {
    uint64_t new_lo = lo + rhs;
    hi += (new_lo < lo) ? 1 : 0;
    lo = new_lo;
    return *this;
  }
};

FMT_HD inline auto umul128(uint64_t x, uint64_t y) noexcept -> uint128 {
  const uint64_t mask = 0xFFFFFFFFu;
  uint64_t a = x >> 32, b = x & mask;
  uint64_t c = y >> 32, d = y & mask;
  uint64_t ac = a * c, bc = b * c, ad = a * d, bd = b * d;
  uint64_t mid = (bd >> 32) + (ad & mask) + (bc & mask);
  return {ac + (mid >> 32) + (ad >> 32) + (bc >> 32), (mid << 32) + (bd & mask)};
}

FMT_HD inline auto umul128_upper64(uint64_t x, uint64_t y) noexcept -> uint64_t {
  return umul128(x, y).high();
}

FMT_HD inline auto umul192_upper128(uint64_t x, uint128 y) noexcept -> uint128 {
  uint128 r = umul128(x, y.high());
  r += umul128_upper64(x, y.low());
  return r;
}

FMT_HD inline auto umul192_lower128(uint64_t x, uint128 y) noexcept -> uint128 {
  uint64_t high = x * y.high();
  uint128 high_low = umul128(x, y.low());
  return {high + high_low.high(), high_low.low()};
}

FMT_HD inline auto umul96_upper64(uint32_t x, uint64_t y) noexcept -> uint64_t {
  return umul128_upper64(static_cast<uint64_t>(x) << 32, y);
}

FMT_HD inline auto umul96_lower64(uint32_t x, uint64_t y) noexcept -> uint64_t { return x * y; }

FMT_HD inline auto rotr(uint32_t n, uint32_t r) noexcept -> uint32_t {
  r &= 31;
  return (n >> r) | (n << (32 - r));
}

FMT_HD inline auto rotr(uint64_t n, uint32_t r) noexcept -> uint64_t {
  r &= 63;
  return (n >> r) | (n << (64 - r));
}

FMT_HD inline auto floor_log10_pow2(int e) noexcept -> int { return (e * 315653) >> 20; }

FMT_HD inline auto floor_log2_pow10(int e) noexcept -> int { return (e * 1741647) >> 19; }

FMT_HD inline auto floor_log10_pow2_minus_log10_4_over_3(int e) noexcept -> int {
  return (e * 631305 - 261663) >> 21;
}

template <typename T> struct float_info;

template <> struct float_info<float> {
  using carrier_uint = uint32_t;
  static constexpr int exponent_bits = 8;
  static constexpr int kappa = 1;
  static constexpr int big_divisor = 100;
  static constexpr int small_divisor = 10;
  static constexpr int min_k = -31;
  static constexpr int max_k = 46;
  static constexpr int shorter_interval_tie_lower_threshold = -35;
  static constexpr int shorter_interval_tie_upper_threshold = -35;
};

template <> struct float_info<double> {
  using carrier_uint = uint64_t;
  static constexpr int exponent_bits = 11;
  static constexpr int kappa = 2;
  static constexpr int big_divisor = 1000;
  static constexpr int small_divisor = 100;
  static constexpr int min_k = -292;
  static constexpr int max_k = 341;
  static constexpr int shorter_interval_tie_lower_threshold = -77;
  static constexpr int shorter_interval_tie_upper_threshold = -77;
};

template <typename T> struct decimal_fp {
  using significand_type = typename float_info<T>::carrier_uint;
  significand_type significand;
  int exponent;
};

template <typename Float> constexpr auto num_significand_bits() -> int {
  return std::numeric_limits<Float>::digits - 1;
}

template <typename Float>
constexpr auto exponent_mask() -> typename float_info<Float>::carrier_uint {
  using uint = typename float_info<Float>::carrier_uint;
  return ((uint(1) << float_info<Float>::exponent_bits) - 1) << num_significand_bits<Float>();
}

template <typename Float> constexpr auto exponent_bias() -> int {
  return std::numeric_limits<Float>::max_exponent - 1;
}

struct div_info {
  uint32_t divisor;
  int shift;
};
static constexpr div_info div_infos[] = {{10, 16}, {100, 16}};

template <int N> FMT_HD auto check_divisibility_and_divide_by_pow10(uint32_t &n) noexcept -> bool {
  constexpr auto info = div_infos[N - 1];
  constexpr uint32_t magic = (1u << info.shift) / info.divisor + 1;
  n *= magic;
  const uint32_t mask = (1u << info.shift) - 1;
  bool result = (n & mask) < magic;
  n >>= info.shift;
  return result;
}

FMT_HD inline auto divide_by_10_to_kappa_plus_1(uint32_t n) noexcept -> uint32_t {
  return static_cast<uint32_t>((static_cast<uint64_t>(n) * 1374389535) >> 37);
}

FMT_HD inline auto divide_by_10_to_kappa_plus_1(uint64_t n) noexcept -> uint64_t {
  return umul128_upper64(n, 2361183241434822607ull) >> 7;
}

template <typename T> struct cache_accessor;

static constexpr uint64_t float_pow10_table[] = {
    0x81ceb32c4b43fcf5, 0xa2425ff75e14fc32, 0xcad2f7f5359a3b3f, 0xfd87b5f28300ca0e,
    0x9e74d1b791e07e49, 0xc612062576589ddb, 0xf79687aed3eec552, 0x9abe14cd44753b53,
    0xc16d9a0095928a28, 0xf1c90080baf72cb2, 0x971da05074da7bef, 0xbce5086492111aeb,
    0xec1e4a7db69561a6, 0x9392ee8e921d5d08, 0xb877aa3236a4b44a, 0xe69594bec44de15c,
    0x901d7cf73ab0acda, 0xb424dc35095cd810, 0xe12e13424bb40e14, 0x8cbccc096f5088cc,
    0xafebff0bcb24aaff, 0xdbe6fecebdedd5bf, 0x89705f4136b4a598, 0xabcc77118461cefd,
    0xd6bf94d5e57a42bd, 0x8637bd05af6c69b6, 0xa7c5ac471b478424, 0xd1b71758e219652c,
    0x83126e978d4fdf3c, 0xa3d70a3d70a3d70b, 0xcccccccccccccccd, 0x8000000000000000,
    0xa000000000000000, 0xc800000000000000, 0xfa00000000000000, 0x9c40000000000000,
    0xc350000000000000, 0xf424000000000000, 0x9896800000000000, 0xbebc200000000000,
    0xee6b280000000000, 0x9502f90000000000, 0xba43b74000000000, 0xe8d4a51000000000,
    0x9184e72a00000000, 0xb5e620f480000000, 0xe35fa931a0000000, 0x8e1bc9bf04000000,
    0xb1a2bc2ec5000000, 0xde0b6b3a76400000, 0x8ac7230489e80000, 0xad78ebc5ac620000,
    0xd8d726b7177a8000, 0x878678326eac9000, 0xa968163f0a57b400, 0xd3c21bcecceda100,
    0x84595161401484a0, 0xa56fa5b99019a5c8, 0xcecb8f27f4200f3a, 0x813f3978f8940985,
    0xa18f07d736b90be6, 0xc9f2c9cd04674edf, 0xfc6f7c4045812297, 0x9dc5ada82b70b59e,
    0xc5371912364ce306, 0xf684df56c3e01bc7, 0x9a130b963a6c115d, 0xc097ce7bc90715b4,
    0xf0bdc21abb48db21, 0x96769950b50d88f5, 0xbc143fa4e250eb32, 0xeb194f8e1ae525fe,
    0x92efd1b8d0cf37bf, 0xb7abc627050305ae, 0xe596b7b0c643c71a, 0x8f7e32ce7bea5c70,
    0xb35dbf821ae4f38c, 0xe0352f62a19e306f};

static constexpr uint128 double_pow10_significands[] = {
    {0xff77b1fcbebcdc4f, 0x25e8e89c13bb0f7b}, {0xce5d73ff402d98e3, 0xfb0a3d212dc81290},
    {0xa6b34ad8c9dfc06f, 0xf42faa48c0ea481f}, {0x86a8d39ef77164bc, 0xae5dff9c02033198},
    {0xd98ddaee19068c76, 0x3badd624dd9b0958}, {0xafbd2350644eeacf, 0xe5d1929ef90898fb},
    {0x8df5efabc5979c8f, 0xca8d3ffa1ef463c2}, {0xe55990879ddcaabd, 0xcc420a6a101d0516},
    {0xb94470938fa89bce, 0xf808e40e8d5b3e6a}, {0x95a8637627989aad, 0xdde7001379a44aa9},
    {0xf1c90080baf72cb1, 0x5324c68b12dd6339}, {0xc350000000000000, 0x0000000000000000},
    {0x9dc5ada82b70b59d, 0xf020000000000000}, {0xfee50b7025c36a08, 0x02f236d04753d5b5},
    {0xcde6fd5e09abcf26, 0xed4c0226b55e6f87}, {0xa6539930bf6bff45, 0x84db8346b786151d},
    {0x865b86925b9bc5c2, 0x0b8a2392ba45a9b3}, {0xd910f7ff28069da4, 0x1b2ba1518094da05},
    {0xaf58416654a6babb, 0x387ac8d1970027b3}, {0x8da471a9de737e24, 0x5ceaecfed289e5d3},
    {0xe4d5e82392a40515, 0x0fabaf3feaa5334b}, {0xb8da1662e7b00a17, 0x3d6a751f3b936244},
    {0x95527a5202df0ccb, 0x0f37801e0c43ebc9}, {0xf13e34aabb430a15, 0x647726b9e7c68ff0},
};

static constexpr uint64_t double_powers_of_5_64[] = {
    0x0000000000000001, 0x0000000000000005, 0x0000000000000019, 0x000000000000007d,
    0x0000000000000271, 0x0000000000000c35, 0x0000000000003d09, 0x000000000001312d,
    0x000000000005f5e1, 0x00000000001dcd65, 0x00000000009502f9, 0x0000000002e90edd,
    0x000000000e8d4a51, 0x0000000048c27395, 0x000000016bcc41e9, 0x000000071afd498d,
    0x0000002386f26fc1, 0x000000b1a2bc2ec5, 0x000003782dace9d9, 0x00001158e460913d,
    0x000056bc75e2d631, 0x0001b1ae4d6e2ef5, 0x000878678326eac9, 0x002a5a058fc295ed,
    0x00d3c21bcecceda1, 0x0422ca8b0a00a425, 0x14adf4b7320334b9};

template <> struct cache_accessor<float> {
  using carrier_uint = uint32_t;
  using cache_entry_type = uint64_t;

  static FMT_HD auto get_cached_power(int k) noexcept -> uint64_t {
    return float_pow10_table[k - float_info<float>::min_k];
  }

  struct compute_mul_result {
    carrier_uint result;
    bool is_integer;
  };
  struct compute_mul_parity_result {
    bool parity;
    bool is_integer;
  };

  static FMT_HD auto compute_mul(carrier_uint u,
                          const cache_entry_type &cache) noexcept -> compute_mul_result {
    auto r = umul96_upper64(u, cache);
    return {static_cast<carrier_uint>(r >> 32), static_cast<carrier_uint>(r) == 0};
  }

  static FMT_HD auto compute_delta(const cache_entry_type &cache, int beta) noexcept -> uint32_t {
    return static_cast<uint32_t>(cache >> (64 - 1 - beta));
  }

  static FMT_HD auto compute_mul_parity(carrier_uint two_f, const cache_entry_type &cache,
                                 int beta) noexcept -> compute_mul_parity_result {
    auto r = umul96_lower64(two_f, cache);
    return {((r >> (64 - beta)) & 1) != 0, static_cast<uint32_t>(r >> (32 - beta)) == 0};
  }

  static FMT_HD auto compute_left_endpoint_for_shorter_interval_case(const cache_entry_type &cache,
                                                              int beta) noexcept -> carrier_uint {
    return static_cast<carrier_uint>((cache - (cache >> (num_significand_bits<float>() + 2))) >>
                                     (64 - num_significand_bits<float>() - 1 - beta));
  }

  static FMT_HD auto compute_right_endpoint_for_shorter_interval_case(const cache_entry_type &cache,
                                                               int beta) noexcept -> carrier_uint {
    return static_cast<carrier_uint>((cache + (cache >> (num_significand_bits<float>() + 1))) >>
                                     (64 - num_significand_bits<float>() - 1 - beta));
  }

  static FMT_HD auto compute_round_up_for_shorter_interval_case(const cache_entry_type &cache,
                                                         int beta) noexcept -> carrier_uint {
    return (static_cast<carrier_uint>(cache >> (64 - num_significand_bits<float>() - 2 - beta)) +
            1) /
           2;
  }
};

template <> struct cache_accessor<double> {
  using carrier_uint = uint64_t;
  using cache_entry_type = uint128;

  static FMT_HD auto get_cached_power(int k) noexcept -> uint128 {
    constexpr int compression_ratio = 27;

    int cache_index = (k - float_info<double>::min_k) / compression_ratio;
    int kb = cache_index * compression_ratio + float_info<double>::min_k;
    int offset = k - kb;

    uint128 base_cache = double_pow10_significands[cache_index];
    if (offset == 0)
      return base_cache;

    int alpha = floor_log2_pow10(kb + offset) - floor_log2_pow10(kb) - offset;

    uint64_t pow5 = double_powers_of_5_64[offset];
    uint128 recovered_cache = umul128(base_cache.high(), pow5);
    uint128 middle_low = umul128(base_cache.low(), pow5);

    recovered_cache += middle_low.high();

    uint64_t high_to_middle = recovered_cache.high() << (64 - alpha);
    uint64_t middle_to_low = recovered_cache.low() << (64 - alpha);

    recovered_cache = uint128{(recovered_cache.low() >> alpha) | high_to_middle,
                              ((middle_low.low() >> alpha) | middle_to_low)};
    return {recovered_cache.high(), recovered_cache.low() + 1};
  }

  struct compute_mul_result {
    carrier_uint result;
    bool is_integer;
  };
  struct compute_mul_parity_result {
    bool parity;
    bool is_integer;
  };

  static FMT_HD auto compute_mul(carrier_uint u,
                          const cache_entry_type &cache) noexcept -> compute_mul_result {
    auto r = umul192_upper128(u, cache);
    return {r.high(), r.low() == 0};
  }

  static FMT_HD auto compute_delta(const cache_entry_type &cache, int beta) noexcept -> uint32_t {
    return static_cast<uint32_t>(cache.high() >> (64 - 1 - beta));
  }

  static FMT_HD auto compute_mul_parity(carrier_uint two_f, const cache_entry_type &cache,
                                 int beta) noexcept -> compute_mul_parity_result {
    auto r = umul192_lower128(two_f, cache);
    return {((r.high() >> (64 - beta)) & 1) != 0,
            ((r.high() << beta) | (r.low() >> (64 - beta))) == 0};
  }

  static FMT_HD auto compute_left_endpoint_for_shorter_interval_case(const cache_entry_type &cache,
                                                              int beta) noexcept -> carrier_uint {
    return (cache.high() - (cache.high() >> (num_significand_bits<double>() + 2))) >>
           (64 - num_significand_bits<double>() - 1 - beta);
  }

  static FMT_HD auto compute_right_endpoint_for_shorter_interval_case(const cache_entry_type &cache,
                                                               int beta) noexcept -> carrier_uint {
    return (cache.high() + (cache.high() >> (num_significand_bits<double>() + 1))) >>
           (64 - num_significand_bits<double>() - 1 - beta);
  }

  static FMT_HD auto compute_round_up_for_shorter_interval_case(const cache_entry_type &cache,
                                                         int beta) noexcept -> carrier_uint {
    return ((cache.high() >> (64 - num_significand_bits<double>() - 2 - beta)) + 1) / 2;
  }
};

FMT_HD inline auto remove_trailing_zeros(uint32_t &n, int s = 0) noexcept -> int {
  constexpr uint32_t mod_inv_5 = 0xcccccccd;
  constexpr uint32_t mod_inv_25 = 0xc28f5c29;
  while (true) {
    auto q = rotr(n * mod_inv_25, 2);
    if (q > UINT32_MAX / 100)
      break;
    n = q;
    s += 2;
  }
  auto q = rotr(n * mod_inv_5, 1);
  if (q <= UINT32_MAX / 10) {
    n = q;
    s |= 1;
  }
  return s;
}

FMT_HD inline auto remove_trailing_zeros(uint64_t &n) noexcept -> int {
  constexpr uint32_t ten8 = 100000000u;
  if ((n % ten8) == 0) {
    auto n32 = static_cast<uint32_t>(n / ten8);
    int s = remove_trailing_zeros(n32, 8);
    n = n32;
    return s;
  }
  constexpr uint64_t mod_inv_5 = 0xcccccccccccccccd;
  constexpr uint64_t mod_inv_25 = 0x8f5c28f5c28f5c29;
  int s = 0;
  while (true) {
    auto q = rotr(n * mod_inv_25, 2);
    if (q > UINT64_MAX / 100)
      break;
    n = q;
    s += 2;
  }
  auto q = rotr(n * mod_inv_5, 1);
  if (q <= UINT64_MAX / 10) {
    n = q;
    s |= 1;
  }
  return s;
}

template <typename T>
FMT_HD auto is_left_endpoint_integer_shorter_interval(int exponent) noexcept -> bool {
  return exponent >= 2 && exponent <= 3;
}

template <typename T> FMT_HD inline auto shorter_interval_case(int exponent) noexcept -> decimal_fp<T> {
  decimal_fp<T> ret;
  const int minus_k = floor_log10_pow2_minus_log10_4_over_3(exponent);
  const int beta = exponent + floor_log2_pow10(-minus_k);

  using cache_entry_type = typename cache_accessor<T>::cache_entry_type;
  const cache_entry_type cache = cache_accessor<T>::get_cached_power(-minus_k);

  auto xi = cache_accessor<T>::compute_left_endpoint_for_shorter_interval_case(cache, beta);
  auto zi = cache_accessor<T>::compute_right_endpoint_for_shorter_interval_case(cache, beta);

  if (!is_left_endpoint_integer_shorter_interval<T>(exponent))
    ++xi;

  ret.significand = zi / 10;
  if (ret.significand * 10 >= xi) {
    ret.exponent = minus_k + 1;
    ret.exponent += remove_trailing_zeros(ret.significand);
    return ret;
  }

  ret.significand = cache_accessor<T>::compute_round_up_for_shorter_interval_case(cache, beta);
  ret.exponent = minus_k;

  if (exponent >= float_info<T>::shorter_interval_tie_lower_threshold &&
      exponent <= float_info<T>::shorter_interval_tie_upper_threshold) {
    ret.significand = ret.significand % 2 == 0 ? ret.significand : ret.significand - 1;
  } else if (ret.significand < xi) {
    ++ret.significand;
  }
  return ret;
}

template <typename T> FMT_HD auto to_decimal(T x) noexcept -> decimal_fp<T> {
  using carrier_uint = typename float_info<T>::carrier_uint;
  using cache_entry_type = typename cache_accessor<T>::cache_entry_type;
  auto br = __builtin_bit_cast(carrier_uint, x);

  const carrier_uint significand_mask =
      (static_cast<carrier_uint>(1) << num_significand_bits<T>()) - 1;
  carrier_uint significand = (br & significand_mask);
  int exponent = static_cast<int>((br & exponent_mask<T>()) >> num_significand_bits<T>());

  if (exponent != 0) {
    exponent -= exponent_bias<T>() + num_significand_bits<T>();
    if (significand == 0)
      return shorter_interval_case<T>(exponent);
    significand |= (static_cast<carrier_uint>(1) << num_significand_bits<T>());
  } else {
    if (significand == 0)
      return {0, 0};
    exponent = std::numeric_limits<T>::min_exponent - num_significand_bits<T>() - 1;
  }

  const bool include_left_endpoint = (significand % 2 == 0);
  const bool include_right_endpoint = include_left_endpoint;

  const int minus_k = floor_log10_pow2(exponent) - float_info<T>::kappa;
  const cache_entry_type cache = cache_accessor<T>::get_cached_power(-minus_k);
  const int beta = exponent + floor_log2_pow10(-minus_k);

  const uint32_t deltai = cache_accessor<T>::compute_delta(cache, beta);
  const carrier_uint two_fc = significand << 1;

  const typename cache_accessor<T>::compute_mul_result z_mul =
      cache_accessor<T>::compute_mul((two_fc | 1) << beta, cache);

  decimal_fp<T> ret;
  ret.significand = divide_by_10_to_kappa_plus_1(z_mul.result);
  uint32_t r = static_cast<uint32_t>(z_mul.result - float_info<T>::big_divisor * ret.significand);

  if (r < deltai) {
    if (r == 0 && (z_mul.is_integer & !include_right_endpoint)) {
      --ret.significand;
      r = float_info<T>::big_divisor;
      goto small_divisor;
    }
  } else if (r > deltai) {
    goto small_divisor;
  } else {
    const typename cache_accessor<T>::compute_mul_parity_result x_mul =
        cache_accessor<T>::compute_mul_parity(two_fc - 1, cache, beta);
    if (!(x_mul.parity | (x_mul.is_integer & include_left_endpoint)))
      goto small_divisor;
  }

  ret.exponent = minus_k + float_info<T>::kappa + 1;
  ret.exponent += remove_trailing_zeros(ret.significand);
  return ret;

small_divisor:
  ret.significand *= 10;
  ret.exponent = minus_k + float_info<T>::kappa;

  uint32_t dist = r - (deltai / 2) + (float_info<T>::small_divisor / 2);
  const bool approx_y_parity = ((dist ^ (float_info<T>::small_divisor / 2)) & 1) != 0;

  const bool divisible = check_divisibility_and_divide_by_pow10<float_info<T>::kappa>(dist);
  ret.significand += dist;
  if (!divisible)
    return ret;

  const auto y_mul = cache_accessor<T>::compute_mul_parity(two_fc, cache, beta);
  if (y_mul.parity != approx_y_parity)
    --ret.significand;
  else if (y_mul.is_integer & (ret.significand % 2 != 0))
    --ret.significand;
  return ret;
}

FMT_HD inline uint64_t pow10_u64(int n) {
  uint64_t p = 1;
  for (int i = 0; i < n; i++) p *= 10;
  return p;
}

FMT_HD inline auto count_digits(uint64_t n) -> int {
  int count = 1;
  while (n >= 10) {
    n /= 10;
    ++count;
  }
  return count;
}

FMT_HD inline auto write_digits(char *buf, uint64_t n, int num_digits) -> char * {
  char *end = buf + num_digits;
  char *p = end;
  while (n >= 10) {
    *--p = '0' + static_cast<char>(n % 10);
    n /= 10;
  }
  *--p = '0' + static_cast<char>(n);
  return end;
}

// {fmt}'s rule for the shortest ("{}") form: fixed notation when the decimal
// exponent is in [-4, exp_upper), where exp_upper is 16 for double and 7 for
// float (min(16, digits10 + 1)); scientific otherwise. (std::format instead
// picks whichever is shorter: 1e+06 where {fmt} prints 1000000.)
template <typename T> constexpr auto exp_upper() -> int {
  return std::numeric_limits<T>::digits10 + 1 < 16 ? std::numeric_limits<T>::digits10 + 1 : 16;
}

template <typename T> FMT_HD inline auto format_shortest(char *buf, T value) -> int {
  // Bit test, not value == 0: under -ffast-math (DAZ) a subnormal compares
  // equal to zero and would print as "0".
  using carrier = typename float_info<T>::carrier_uint;
  if ((__builtin_bit_cast(carrier, value) << 1) == 0) {
    buf[0] = '0';
    return 1;
  }

  auto dec = to_decimal(value);
  auto significand = static_cast<uint64_t>(dec.significand);
  int sig_size = count_digits(significand);
  int exponent = dec.exponent + sig_size - 1;

  char *p = buf;

  if (exponent >= -4 && exponent < exp_upper<T>()) {
    if (exponent >= 0) {
      int int_digits = exponent + 1;
      if (int_digits >= sig_size) {
        write_digits(p, significand, sig_size);
        p += sig_size;
        for (int i = 0; i < int_digits - sig_size; i++)
          *p++ = '0';
      } else {
        write_digits(p, significand, sig_size);
        for (int i = sig_size - 1; i >= int_digits; i--)
          p[i + 1] = p[i];
        p[int_digits] = '.';
        p += sig_size + 1;
      }
    } else {
      *p++ = '0';
      *p++ = '.';
      int leading_zeros = -(exponent + 1);
      for (int i = 0; i < leading_zeros; i++)
        *p++ = '0';
      write_digits(p, significand, sig_size);
      p += sig_size;
    }
  } else {
    write_digits(p, significand, sig_size);
    if (sig_size > 1) {
      for (int i = sig_size; i >= 2; i--)
        p[i] = p[i - 1];
      p[1] = '.';
      p += sig_size + 1;
    } else {
      p += 1;
    }
    *p++ = 'e';
    int abs_exp = exponent < 0 ? -exponent : exponent;
    *p++ = exponent < 0 ? '-' : '+';
    if (abs_exp >= 100) {
      *p++ = '0' + static_cast<char>(abs_exp / 100);
      abs_exp %= 100;
    }
    *p++ = '0' + static_cast<char>(abs_exp / 10);
    *p++ = '0' + static_cast<char>(abs_exp % 10);
  }
  return static_cast<int>(p - buf);
}

} // namespace dragonbox
#ifdef _OPENMP
#pragma omp end declare target
#endif
#endif // FFMT_BUFFER_PATH

// ============================================================
// fixed_string — compile-time string usable as NTTP
// ============================================================

template <size_t N> struct fixed_string {
  char data[N]{};

  constexpr fixed_string() = default;
  constexpr fixed_string(const char (&s)[N]) { std::copy_n(s, N, data); }

  constexpr char operator[](size_t i) const { return data[i]; }
};

template <size_t N> fixed_string(const char (&)[N]) -> fixed_string<N>;

// Length of a fixed_string (excluding null terminator)
template <size_t N> consteval size_t flen(const fixed_string<N> &) {
  static_assert(
      N >= 1,
      "Implementation Error: fixed_string must include null terminator. Something did go wrong");
  return N - 1;
}

// ============================================================
// placeholder_info — describes the first placeholder found
// ============================================================

struct placeholder_info {
  size_t open = 0;     // index of '{'
  size_t close = 0;    // index of '}'
  size_t spec_beg = 0; // index after ':' (or close if no spec)
  bool has_spec = false;
  int index = -1; // -1 = auto ({}), >=0 = positional ({N})
};

// find_placeholder's result, used like std::optional (if (ph), ph->...).
// Not std::optional itself: libstdc++'s accessors assert through a
// host-only function (__glibcxx_assert_fail), which clang-CUDA rejects in
// device code.
struct found_placeholder {
  placeholder_info info; // default when !found
  bool found = false;
  constexpr explicit operator bool() const { return found; }
  constexpr const placeholder_info *operator->() const { return &info; }
};

// ============================================================
// Literal segment: unescape {{ → {, }} → } (and optionally % → %%)
// ============================================================

// Literal text of Fmt[Begin, End): "{{" → "{", "}}" → "}", and for a printf
// format string "%" → "%%". out == nullptr only measures it.
template <fixed_string Fmt, size_t Begin, size_t End, bool EscapePercent = true>
consteval size_t walk_literal(char *out, size_t pos = 0) {
  size_t i = Begin;
  while (i < End) {
    if (i + 1 < End && Fmt[i] == '{' && Fmt[i + 1] == '{') {
      if (out)
        out[pos] = '{';
      pos++;
      i += 2;
    } else if (i + 1 < End && Fmt[i] == '}' && Fmt[i + 1] == '}') {
      if (out)
        out[pos] = '}';
      pos++;
      i += 2;
    } else if (EscapePercent && Fmt[i] == '%') {
      if (out) {
        out[pos] = '%';
        out[pos + 1] = '%';
      }
      pos += 2;
      i++;
    } else {
      if (out)
        out[pos] = Fmt[i];
      pos++;
      i++;
    }
  }
  return pos;
}

// Types supported by ffmt::print
template <typename T>
concept sycl_printable = std::same_as<T, bool> || std::same_as<T, char> || std::integral<T> ||
                         std::floating_point<T> || std::is_pointer_v<T>;

} // namespace detail

// ============================================================
// Customization point: formatter<T>
// Users specialize ffmt::formatter<T> to teach the
// library how to print a custom type. The specialization must
// expose a static `format(T)` returning a `formatted<Fmt, ...>`
// where Fmt is a compile-time format string and the values
// reduce, after recursive expansion, to sycl_printable types.
// ============================================================

template <detail::fixed_string Fmt, typename... Args>
struct formatted {
  static constexpr auto format_string = Fmt;
  std::tuple<Args...> values;
};

template <typename T> struct formatter; // primary, intentionally undefined

template <typename T>
concept has_formatter = requires(T v) {
  { formatter<std::decay_t<T>>::format(v) };
};

template <typename T>
concept sycl_formattable = detail::sycl_printable<T> || has_formatter<T>;

namespace detail {

// ============================================================
// Format spec parsing and printf format string generation
// ============================================================

constexpr bool is_type_char(char c) {
  return c == 'd' || c == 'x' || c == 'X' || c == 'o' || c == 'b' || c == 'B' || c == 'f' ||
         c == 'F' || c == 'e' || c == 'E' || c == 'g' || c == 'G' || c == 'a' || c == 'A' ||
         c == 'c' || c == 's' || c == 'p';
}

constexpr bool is_align_char(char c) { return c == '<' || c == '>' || c == '^'; }

// ── UTF-8 display width, as {fmt} computes it ───────────────────────────
// Code points of display width 2: Unicode East_Asian_Width W/F plus emoji,
// copied from {fmt}'s wide_cp_data so padding and string precision match.
struct wide_cp_range { uint32_t first, last; };
inline constexpr wide_cp_range wide_cp_ranges[] = {
    {0x1100, 0x115f}, {0x231a, 0x231b}, {0x2329, 0x232a}, {0x23e9, 0x23ec}, {0x23f0, 0x23f0},
    {0x23f3, 0x23f3}, {0x25fd, 0x25fe}, {0x2614, 0x2615}, {0x2630, 0x2637}, {0x2648, 0x2653},
    {0x267f, 0x267f}, {0x268a, 0x268f}, {0x2693, 0x2693}, {0x26a1, 0x26a1}, {0x26aa, 0x26ab},
    {0x26bd, 0x26be}, {0x26c4, 0x26c5}, {0x26ce, 0x26ce}, {0x26d4, 0x26d4}, {0x26ea, 0x26ea},
    {0x26f2, 0x26f3}, {0x26f5, 0x26f5}, {0x26fa, 0x26fa}, {0x26fd, 0x26fd}, {0x2705, 0x2705},
    {0x270a, 0x270b}, {0x2728, 0x2728}, {0x274c, 0x274c}, {0x274e, 0x274e}, {0x2753, 0x2755},
    {0x2757, 0x2757}, {0x2795, 0x2797}, {0x27b0, 0x27b0}, {0x27bf, 0x27bf}, {0x2b1b, 0x2b1c},
    {0x2b50, 0x2b50}, {0x2b55, 0x2b55}, {0x2e80, 0x2e99}, {0x2e9b, 0x2ef3}, {0x2f00, 0x2fd5},
    {0x2ff0, 0x303e}, {0x3041, 0x3096}, {0x3099, 0x30ff}, {0x3105, 0x312f}, {0x3131, 0x318e},
    {0x3190, 0x31e5}, {0x31ef, 0x321e}, {0x3220, 0x3247}, {0x3250, 0xa48c}, {0xa490, 0xa4c6},
    {0xa960, 0xa97c}, {0xac00, 0xd7a3}, {0xf900, 0xfaff}, {0xfe10, 0xfe19}, {0xfe30, 0xfe52},
    {0xfe54, 0xfe66}, {0xfe68, 0xfe6b}, {0xff01, 0xff60}, {0xffe0, 0xffe6}, {0x16fe0, 0x16fe4},
    {0x16ff0, 0x16ff1}, {0x17000, 0x187f7}, {0x18800, 0x18cd5}, {0x18cff, 0x18d08},
    {0x1aff0, 0x1aff3}, {0x1aff5, 0x1affb}, {0x1affd, 0x1affe}, {0x1b000, 0x1b122},
    {0x1b132, 0x1b132}, {0x1b150, 0x1b152}, {0x1b155, 0x1b155}, {0x1b164, 0x1b167},
    {0x1b170, 0x1b2fb}, {0x1d300, 0x1d356}, {0x1d360, 0x1d376}, {0x1f004, 0x1f004},
    {0x1f0cf, 0x1f0cf}, {0x1f18e, 0x1f18e}, {0x1f191, 0x1f19a}, {0x1f200, 0x1f202},
    {0x1f210, 0x1f23b}, {0x1f240, 0x1f248}, {0x1f250, 0x1f251}, {0x1f260, 0x1f265},
    {0x1f300, 0x1f64f}, {0x1f680, 0x1f6c5}, {0x1f6cc, 0x1f6cc}, {0x1f6d0, 0x1f6d2},
    {0x1f6d5, 0x1f6d7}, {0x1f6dc, 0x1f6df}, {0x1f6eb, 0x1f6ec}, {0x1f6f4, 0x1f6fc},
    {0x1f7e0, 0x1f7eb}, {0x1f7f0, 0x1f7f0}, {0x1f900, 0x1f9ff}, {0x1fa70, 0x1fa7c},
    {0x1fa80, 0x1fa89}, {0x1fa8f, 0x1fac6}, {0x1face, 0x1fadc}, {0x1fadf, 0x1fae9},
    {0x1faf0, 0x1faf8}, {0x20000, 0x2fffd}, {0x30000, 0x3fffd},
};

FMT_HD constexpr int display_width_of(uint32_t cp) {
  if (cp < 0x1100) return 1;
  int lo = 0, hi = static_cast<int>(sizeof(wide_cp_ranges) / sizeof(wide_cp_ranges[0]));
  while (lo < hi) {
    int mid = lo + (hi - lo) / 2;
    if (cp < wide_cp_ranges[mid].first) hi = mid;
    else if (cp > wide_cp_ranges[mid].last) lo = mid + 1;
    else return 2;
  }
  return 1;
}

inline constexpr uint32_t invalid_code_point = ~uint32_t(0);

// Decode the multi-byte UTF-8 sequence at s (lead byte >= 0x80; callers
// handle ASCII) into cp and return its byte length. Never reads past a NUL.
// An invalid sequence (bad lead or tail byte, overlong, surrogate,
// > U+10FFFF) is one byte with cp = invalid_code_point, which {fmt} also
// counts as width 1. No lookup tables: on GPUs a runtime-indexed local
// array can land in local memory.
FMT_HD constexpr int utf8_decode(const char *s, uint32_t &cp) {
  auto b = [s](int k) { return static_cast<unsigned char>(s[k]); };
  unsigned char c = b(0);
  int n = (c >> 5) == 0x6 ? 2 : (c >> 4) == 0xE ? 3 : (c >> 3) == 0x1E ? 4 : 0;
  cp = invalid_code_point;
  if (n == 0) return 1;
  uint32_t v = c & (0x7f >> n);
  for (int k = 1; k < n; k++) {
    if ((b(k) & 0xC0) != 0x80) return 1;
    v = (v << 6) | (b(k) & 0x3f);
  }
  uint32_t min = n == 2 ? 0x80 : n == 3 ? 0x800 : 0x10000; // shortest form
  if (v < min || (v >> 11) == 0x1b || v > 0x10FFFF) return 1;
  cp = v;
  return n;
}

constexpr bool is_float_format(char c) {
  return c == 'f' || c == 'F' || c == 'e' || c == 'E' || c == 'g' || c == 'G' || c == 'a' ||
         c == 'A';
}

// Consteval-only abort. Calling a non-constexpr function inside a consteval
// context makes the evaluation fail with a diagnostic citing the function
// name — which we use as the error message. Equivalent in info content to
// the old `throw "literal"` pattern (all our messages were static strings
// anyway) but doesn't require -fcxx-exceptions — matters under e.g.
// -fopenmp-targets=spir64 which disables exceptions on the device side.
// FMT_HD: these are reached (at compile time only) from code that also runs
// on the device, and clang-CUDA rejects references to host functions there.
namespace consteval_error {
[[noreturn]] FMT_HD void too_many_placeholders_max_16();
[[noreturn]] FMT_HD void format_argument_index_out_of_range();
[[noreturn]] FMT_HD void dynamic_width_argument_index_out_of_range();
[[noreturn]] FMT_HD void dynamic_precision_argument_index_out_of_range();
[[noreturn]] FMT_HD void format_spec_type_incompatible_with_argument_type();
[[noreturn]] FMT_HD void invalid_format_spec();
[[noreturn]] FMT_HD void invalid_placeholder();
[[noreturn]] FMT_HD void unmatched_closing_brace();
[[noreturn]] FMT_HD void cannot_mix_automatic_and_manual_indexing();
[[noreturn]] FMT_HD void width_or_precision_above_32767();
[[noreturn]] FMT_HD void dynamic_width_or_precision_must_be_an_integer();
}

// Fill: one UTF-8 code point (1-4 bytes), as in {fmt}'s "{:─^9}".
struct fill_char {
  char data[4] = {' '};
  uint8_t size = 1;
};

// Parsed format spec: [[fill]align][sign][#][0][width][.precision][type]
struct format_spec {
  fill_char fill{}; // only set together with align
  char align = '\0';
  char sign = '\0';
  char type = '\0';
  bool alt = false;
  bool zero_pad = false;
  uint16_t width = 0;
  int16_t precision = -1;
  int8_t width_arg = -1; // >=0: dynamic width from arg N, -1: static
  int8_t prec_arg = -1;  // >=0: dynamic precision from arg N, -1: static
  uint8_t dyn_count = 0; // number of auto-indexed dynamic args consumed

  constexpr char align_or(char def = '>') const { return align ? align : def; }
};

// Parse a dynamic width/precision reference ({} or {N}) at data[i] == '{' and
// advance i past its '}'. auto_idx >= 0 inside an automatically indexed
// placeholder ({} consumes it) and -1 inside a manual one; mixing the two is
// an error, as is anything else between the braces.
constexpr int parse_dynamic_arg(const char *data, size_t len, size_t &i, int &auto_idx) {
  int idx = -1;
  for (i++; i < len && data[i] >= '0' && data[i] <= '9'; i++)
    idx = (idx < 0 ? 0 : idx * 10) + (data[i] - '0');
  if (i >= len || data[i] != '}') consteval_error::invalid_format_spec();
  i++;
  if ((idx < 0) != (auto_idx >= 0)) consteval_error::cannot_mix_automatic_and_manual_indexing();
  return idx < 0 ? auto_idx++ : idx;
}

// ============================================================
// Runtime placeholder + spec parsing
// ============================================================
// Used at compile time by both paths (validate_format, print_string, the
// specifiers walkers); find_placeholder also runs on device for the strings
// of custom formatters on the buffer path.

// Find the first {} or {:spec} or {N} or {N:spec} placeholder in s[from..len).
constexpr found_placeholder find_placeholder(const char *s, int len, int from) {
  int i = from;
  while (i < len) {
    if (s[i] == '{') {
      if (i + 1 < len && s[i + 1] == '{') { i += 2; continue; }
      int j = i + 1;
      int index = -1;
      if (j < len && s[j] >= '0' && s[j] <= '9') {
        index = 0;
        while (j < len && s[j] >= '0' && s[j] <= '9') {
          index = index * 10 + (s[j] - '0');
          j++;
        }
      }
      bool has_spec = false;
      int spec_beg = j;
      if (j < len && s[j] == ':') {
        has_spec = true;
        spec_beg = j + 1;
        j++;
        int depth = 0;
        while (j < len) {
          if (s[j] == '{') depth++;
          else if (s[j] == '}') { if (depth == 0) break; depth--; }
          j++;
        }
      }
      int close = j;
      if (!has_spec) spec_beg = close;
      return {placeholder_info{static_cast<size_t>(i), static_cast<size_t>(close),
                               static_cast<size_t>(spec_beg), has_spec, index},
              true};
    } else if (s[i] == '}') {
      if (i + 1 < len && s[i + 1] == '}') { i += 2; continue; }
      i++;
    } else {
      i++;
    }
  }
  return {};
}

// Runtime version of parse_spec — works on const char* instead of fixed_string NTTP
// Parse data[begin, end). Only ever evaluated at compile time, so a
// malformed spec fails the build like it does with {fmt}.
constexpr format_spec parse_spec(const char *data, int begin, int end,
                                    int dyn_auto_start = -1) {
  format_spec s{};
  size_t i = static_cast<size_t>(begin);
  size_t e = static_cast<size_t>(end);
  int dyn_auto = dyn_auto_start;
  auto number = [&] { // width or precision digits
    int n = 0;
    for (; i < e && data[i] >= '0' && data[i] <= '9'; i++)
      if ((n = n * 10 + (data[i] - '0')) > 0x7fff) consteval_error::width_or_precision_above_32767();
    return n;
  };

  // Nothing else in a spec is non-ASCII, so a non-ASCII byte here must start
  // a valid UTF-8 fill.
  size_t n = i < e ? 1 : 0;
  if (n && static_cast<unsigned char>(data[i]) >= 0x80) {
    uint32_t cp = 0;
    n = static_cast<size_t>(utf8_decode(data + i, cp));
    if (cp == invalid_code_point) consteval_error::invalid_format_spec();
  }
  if (n && i + n < e && is_align_char(data[i + n])) {
    if (data[i] == '{' || data[i] == '}') consteval_error::invalid_format_spec();
    for (size_t k = 0; k < n; k++) s.fill.data[k] = data[i + k];
    s.fill.size = static_cast<uint8_t>(n);
    s.align = data[i + n]; i += n + 1;
  } else if (i < e && is_align_char(data[i])) {
    s.align = data[i]; i++;
  }
  if (i < e && (data[i] == '+' || data[i] == '-' || data[i] == ' ')) {
    s.sign = data[i]; i++;
  }
  if (i < e && data[i] == '#') { s.alt = true; i++; }
  if (i < e && data[i] == '0') { s.zero_pad = true; i++; }
  if (i < e && data[i] == '{') s.width_arg = static_cast<int8_t>(parse_dynamic_arg(data, e, i, dyn_auto));
  else s.width = static_cast<uint16_t>(number());
  if (i < e && data[i] == '.') {
    i++;
    if (i < e && data[i] == '{') s.prec_arg = static_cast<int8_t>(parse_dynamic_arg(data, e, i, dyn_auto));
    else s.precision = static_cast<int16_t>(number());
  }
  if (i < e && is_type_char(data[i])) s.type = data[i++];
  if (i != e) consteval_error::invalid_format_spec();
  s.dyn_count = static_cast<uint8_t>(dyn_auto_start >= 0 ? dyn_auto - dyn_auto_start : 0);
  return s;
}

// Runtime version of effective_type — spec_type is a runtime parameter
template <typename U> constexpr char effective_type(char spec_type) {
  if (spec_type != '\0') return spec_type;
  if constexpr (std::same_as<U, bool>) return 's';
  else if constexpr (std::same_as<U, char>) return 'c';
  else if constexpr (std::floating_point<U>) return 'g';
  else if constexpr (std::signed_integral<U>) return 'd';
  else if constexpr (std::unsigned_integral<U>) return 'u';
  else if constexpr (std::is_pointer_v<U>) {
    using P = std::remove_cv_t<std::remove_pointer_t<U>>;
    if constexpr (std::same_as<P, char>) return 's';
    else return 'p';
  }
}

template <typename T>
consteval bool type_can_produce_pct() {
  using U = std::decay_t<T>;
  if constexpr (std::same_as<U, char>) return true;
  else if constexpr (std::is_pointer_v<U>)
    return std::same_as<std::remove_cv_t<std::remove_pointer_t<U>>, char>;
  else if constexpr (sycl_printable<U>) return false;
  else return true; // formatter args — be conservative; sub-string may contain '%'
}

// ============================================================
// Shared consteval validation (both paths)
// ============================================================


// Spec/argument compatibility, following {fmt}'s compile-time checks.
// Presentation types:
//   integer: d b B o x X c    bool: d b B o x X s    char: d b B o x X c
//   float:   a A e E f F g G  char*: s p             other pointer: p
// A sign only on signed integers (not char) and floats — std::format also
// allows unsigned; '#' and '0' only on arithmetic types (a char needs an
// integer presentation); a precision only on floats and strings.
template <typename T>
consteval bool spec_compatible_with_arg(const format_spec &spec) {
  using U = std::decay_t<T>;
  constexpr bool is_str = std::is_pointer_v<U> &&
                          std::same_as<std::remove_cv_t<std::remove_pointer_t<U>>, char>;
  constexpr bool is_char = std::same_as<U, char>;
  char t = spec.type;
  bool numeric = std::is_arithmetic_v<U> && !(is_char && (t == '\0' || t == 'c'));
  if (spec.sign && !(std::floating_point<U> || (std::signed_integral<U> && !is_char)))
    return false;
  if ((spec.alt || spec.zero_pad) && !numeric) return false;
  if ((spec.precision >= 0 || spec.prec_arg >= 0) && !(std::floating_point<U> || is_str))
    return false;
  if constexpr (!sycl_printable<U>) // custom formatter: only "{}" / "{:}"
    return !spec.align && !spec.sign && !spec.alt && !spec.zero_pad &&
           !spec.width && spec.precision < 0 && spec.width_arg < 0 && spec.prec_arg < 0 && !t;
  if (t == '\0') return true;
  if constexpr (is_str) {
    return t == 's' || t == 'p';
  } else if constexpr (std::is_pointer_v<U>) {
    return t == 'p';
  } else if constexpr (std::same_as<U, bool>) {
    return t == 'd' || t == 'b' || t == 'B' || t == 'o' ||
           t == 'x' || t == 'X' || t == 's';
  } else if constexpr (std::is_integral_v<U>) {
    return t == 'd' || t == 'b' || t == 'B' || t == 'o' ||
           t == 'x' || t == 'X' || t == 'c';
  } else {
    return t == 'a' || t == 'A' || t == 'e' || t == 'E' ||
           t == 'f' || t == 'F' || t == 'g' || t == 'G';
  }
}

// {fmt}'s compile-time checks over a whole format string, for both paths:
// brace structure, argument ids, no mixing of automatic and manual indexing,
// spec syntax (parse_spec) and spec/argument compatibility.
template <typename... Args>
consteval void validate_format(const char *s, int len) {
  constexpr int n_args = static_cast<int>(sizeof...(Args));
  int auto_idx = 0;
  bool automatic = false, manual = false;
  for (int i = 0; i < len; i++) {
    if (s[i] != '{' && s[i] != '}') continue;
    if (i + 1 < len && s[i + 1] == s[i]) { i++; continue; } // "{{" or "}}"
    if (s[i] == '}') consteval_error::unmatched_closing_brace();
    auto ph = find_placeholder(s, len, i).info; // s[i] is an unescaped '{'
    int close = static_cast<int>(ph.close);
    if (close >= len || s[close] != '}') consteval_error::invalid_placeholder();
    (ph.index < 0 ? automatic : manual) = true;
    int arg = ph.index < 0 ? auto_idx++ : ph.index;
    format_spec spec = parse_spec(s, static_cast<int>(ph.spec_beg), close,
                                     ph.index < 0 ? auto_idx : -1);
    if (ph.index < 0) auto_idx += spec.dyn_count;
    if (automatic && manual) consteval_error::cannot_mix_automatic_and_manual_indexing();
    if (arg >= n_args) consteval_error::format_argument_index_out_of_range();
    if (spec.width_arg >= n_args) consteval_error::dynamic_width_argument_index_out_of_range();
    if (spec.prec_arg >= n_args) consteval_error::dynamic_precision_argument_index_out_of_range();
    int j = 0;
    bool ok = true, dyn_ok = true;
    ((ok = ok && (j != arg || spec_compatible_with_arg<Args>(spec)),
      dyn_ok = dyn_ok && ((j != spec.width_arg && j != spec.prec_arg) ||
                          std::integral<std::decay_t<Args>>),
      j++), ...);
    if (!ok) consteval_error::format_spec_type_incompatible_with_argument_type();
    if (!dyn_ok) consteval_error::dynamic_width_or_precision_must_be_an_integer();
    i = close;
  }
}

#if FFMT_BUFFER_PATH
// ============================================================
// print_string — consteval-validated format string for ACPP
// ============================================================

// Pre-parsed placeholder entry — populated at compile time, consumed at runtime.
struct ph_entry {
  uint8_t open;
  uint8_t close;
  int8_t arg_idx;
  bool has_spec;
  format_spec spec;
};

// print_string — consteval-validated format string for print("...", args...) syntax.
// Placeholders and specs are pre-parsed at compile time into phs[].
template <sycl_formattable... Args>
struct print_string {
  static constexpr int MAX_LEN = 256;
  static constexpr int MAX_PH = 16;
  char str[MAX_LEN]{};
  int len;
  ph_entry phs[MAX_PH]{};
  int ph_count = 0;
  bool needs_pct_escape = false;

  template <size_t N>
  consteval print_string(const char (&s)[N]) : len(static_cast<int>(N - 1)) {
    static_assert(N <= MAX_LEN, "format string too long");
    // Open-coded copy: clang-CUDA's host-attribute check walks transitive
    // callees even from consteval ctors, so std::copy_n's libstdc++
    // __assign_one trips a "host-only function called from device" error.
    for (size_t i = 0; i < N; i++) str[i] = s[i];
    validate_format<Args...>(s, len);
    int auto_idx = 0;
    for (auto ph = find_placeholder(s, len, 0); ph;
         ph = find_placeholder(s, len, static_cast<int>(ph->close) + 1)) {
      const placeholder_info &info = ph.info;
      if (ph_count >= MAX_PH)
        consteval_error::too_many_placeholders_max_16();
      ph_entry &e = phs[ph_count++];
      e.open = static_cast<uint8_t>(info.open);
      e.close = static_cast<uint8_t>(info.close);
      e.arg_idx = static_cast<int8_t>(info.index >= 0 ? info.index : auto_idx);
      e.has_spec = info.has_spec && info.close > info.spec_beg;
      e.spec = parse_spec(s, static_cast<int>(info.spec_beg), static_cast<int>(info.close),
                             info.index < 0 ? auto_idx + 1 : -1);
      if (info.index < 0) auto_idx += 1 + e.spec.dyn_count;
    }
    needs_pct_escape = (type_can_produce_pct<Args>() || ...);
    if (!needs_pct_escape) {
      for (int i = 0; i < len; i++) {
        if (str[i] == '%') { needs_pct_escape = true; break; }
      }
    }
  }
};
#endif // FFMT_BUFFER_PATH

// ============================================================
// Device-side buffer (used by ACPP accumulator path)
// ============================================================

#ifndef FFMT_BUFFER_SIZE
#define FFMT_BUFFER_SIZE 128
#endif

// 1: string width/precision count display columns like {fmt} (UTF-8 decode
// plus an ~800 B wide-character table, emitted whenever a string arg has a
// spec). 0: count bytes, like printf; no decoder or table in the binary.
// Mirrors {fmt}'s FMT_UNICODE. Fills are UTF-8-aware either way.
#ifndef FFMT_UNICODE
#define FFMT_UNICODE 1
#endif

// Output of one print on the buffer path. The 32 bytes past cap let
// dragonbox write directly into data[len] (len is clamped to cap afterwards)
// and leave room for println's '\n' and the terminator.
struct fmt_buf {
  static constexpr int cap = FFMT_BUFFER_SIZE;
  char data[cap + 32]{};
  int len = 0;
  FMT_HD void push(char c) {
    if (len < cap)
      data[len++] = c;
  }
  FMT_HD void push_n(char c, int n) {
    for (int i = 0; i < n && len < cap; i++)
      data[len++] = c;
  }
  // Whole code points only, so truncation never splits a multi-byte fill.
  FMT_HD void push_n(const fill_char &f, int n) {
    for (int i = 0; i < n && len + f.size <= cap; i++)
      push_data(f.data, f.size);
  }
  FMT_HD void push_data(const char *s, int n) {
    for (int i = 0; i < n && len < cap; i++)
      data[len++] = s[i];
  }
  FMT_HD void push_str(const char *s) {
    while (*s)
      push(*s++);
  }
};

// Write an unsigned integer in any base into raw (data, len, cap) right-to-left.
// Digits that would land at or past `cap` are dropped (the low-order ones,
// matching push()'s silent truncation) — never written out of bounds.
template <int Base, bool Upper = false, typename U>
  requires (Base == 2 || Base == 8 || Base == 10 || Base == 16)
FMT_HD inline void write_uint_raw(char *data, int &len, int cap, U val) {
  if (val == 0) { if (len < cap) data[len++] = '0'; return; }
  int n = 0;
  for (U t = val; t > 0; t /= U(Base)) n++;
  int pos = len + n - 1;
  while (val > 0) {
    int d = static_cast<int>(val % U(Base));
    if (pos < cap) data[pos] = Upper ? "0123456789ABCDEF"[d] : "0123456789abcdef"[d];
    pos--;
    val /= U(Base);
  }
  len += n;
  if (len > cap) len = cap;
}

template <int Base, bool Upper = false, typename U>
FMT_HD inline void write_uint_direct(fmt_buf &buf, U val) {
  write_uint_raw<Base, Upper>(buf.data, buf.len, fmt_buf::cap, val);
}

// Hex digit helper
FMT_HD inline char hex_digit(int d, bool upper) {
  if (d < 10)
    return static_cast<char>('0' + d);
  return static_cast<char>((upper ? 'A' : 'a') + d - 10);
}

// float → double through the bits. A real conversion is an FP operation, and
// under DAZ (-ffast-math on x86, icpx's default) it flushes subnormals to 0.
FMT_HD inline double widen_exact(double d) { return d; }
FMT_HD inline double widen_exact(long double d) { return static_cast<double>(d); }
FMT_HD inline double widen_exact(float f) {
  uint32_t b = __builtin_bit_cast(uint32_t, f);
  if ((b & 0x7F800000u) != 0 || (b & 0x7FFFFFu) == 0) return static_cast<double>(f);
  double d = static_cast<double>(b & 0x7FFFFFu) * 0x1p-149; // subnormal: m·2^-149
  return (b >> 31) ? -d : d;
}

// Cast an arg to the type its printf conversion expects.
template <char Type, bool Wide, typename T> FMT_HD inline auto printf_cast(T arg) {
  if constexpr (std::same_as<T, bool> && Type == 's')
    return arg ? "true" : "false";
  else if constexpr (Type == 'c')
    return static_cast<char>(arg);
  else if constexpr (Type == 'd')
    return static_cast<std::conditional_t<Wide, long long, int>>(arg);
  else if constexpr (Type == 'u' || Type == 'x' || Type == 'X' || Type == 'o')
    return static_cast<std::conditional_t<Wide, unsigned long long, unsigned>>(arg);
  else if constexpr (is_float_format(Type))
    return widen_exact(arg); // not static_cast: DAZ would flush float subnormals
  else
    return arg;
}

#if !FFMT_BUFFER_PATH
namespace specifiers_path {

// A printf conversion built at compile time.
struct printf_fmt_buf {
  char data[32]{};
  size_t len = 0;

  constexpr void push(char c) { data[len++] = c; }
  constexpr void push_int(int val) {
    char tmp[10]{};
    int n = 0;
    do { tmp[n++] = static_cast<char>('0' + val % 10); val /= 10; } while (val > 0);
    while (n > 0) push(tmp[--n]);
  }
};

// %[flags][width][.precision][ll]type
template <format_spec Spec, char Type, bool Wide>
consteval printf_fmt_buf build_printf_fmt() {
  printf_fmt_buf buf;
  buf.push('%');
  if (Spec.align == '<') buf.push('-');
  if (Spec.sign == '+' || Spec.sign == ' ') buf.push(Spec.sign);
  if (Spec.alt) buf.push('#');
  if (Spec.zero_pad && !Spec.align) buf.push('0'); // ignored once an alignment is given
  if (Spec.width > 0) buf.push_int(Spec.width);
  if (Spec.precision >= 0) { buf.push('.'); buf.push_int(Spec.precision); }
  if (Wide && (Type == 'd' || Type == 'u' || Type == 'x' || Type == 'X' || Type == 'o'))
    { buf.push('l'); buf.push('l'); }
  buf.push(Type);
  return buf;
}

// How one placeholder is printed: its printf conversion and argument. A float
// with no type ("{}", "{:10}") prints like {:g}: printf has no shortest
// round-trip conversion, and Intel GPU printf supports neither `*` widths nor
// %s on computed strings, so {fmt}'s shortest form needs the buffer path.
template <typename U, format_spec Spec> struct conversion {
  static constexpr char type = [] {
    char t = effective_type<U>(Spec.type);
    return std::unsigned_integral<U> && t == 'd' ? 'u' : t;
  }();
  static constexpr bool wide = sizeof(U) > 4;
  static constexpr auto format = build_printf_fmt<Spec, type, wide>();
  // What printf cannot express (these need the buffer path).
  static constexpr bool supported =
      type != 'b' && type != 'B' && type != 'a' && type != 'A' && Spec.align != '^' &&
      Spec.fill.size == 1 && Spec.fill.data[0] == ' ' && Spec.width_arg < 0 && Spec.prec_arg < 0 &&
      !(std::signed_integral<U> && (type == 'x' || type == 'X' || type == 'o')) &&
      !(Spec.alt && (type == 'x' || type == 'X'));
  static auto arg(U v) { return printf_cast<type, wide>(v); }
};

// The placeholder at or after Pos, resolved once for the walkers below.
template <fixed_string Fmt, size_t Pos, size_t AutoIdx> struct placeholder {
  static constexpr auto ph =
      find_placeholder(Fmt.data, static_cast<int>(flen(Fmt)), static_cast<int>(Pos));
  static constexpr bool found = ph.found;
  // A default info when !found keeps the members below well formed; the
  // walkers never use them then.
  static constexpr placeholder_info info = ph.info;
  static constexpr size_t index = info.index < 0 ? AutoIdx : static_cast<size_t>(info.index);
  static constexpr format_spec spec =
      parse_spec(Fmt.data, static_cast<int>(info.spec_beg), static_cast<int>(info.close),
                 info.index < 0 ? static_cast<int>(AutoIdx) + 1 : -1);
  static constexpr size_t next_auto = info.index < 0 ? AutoIdx + 1 + spec.dyn_count : AutoIdx;
};

template <typename P, typename... Args>
using arg_t = std::decay_t<std::tuple_element_t<P::index, std::tuple<Args...>>>;

// Can printf express every placeholder? (validate_format has already checked
// that the format string is well formed.)
template <fixed_string Fmt, size_t Pos, size_t AutoIdx, typename... Args>
consteval bool supported() {
  using P = placeholder<Fmt, Pos, AutoIdx>;
  if constexpr (!P::found)
    return true;
  else
    return conversion<arg_t<P, Args...>, P::spec>::supported &&
           supported<Fmt, P::info.close + 1, P::next_auto, Args...>();
}

// The whole printf format string; out == nullptr only measures it.
template <fixed_string Fmt, size_t Pos, size_t AutoIdx, typename... Args>
consteval size_t write_printf_fmt(char *out, size_t pos = 0) {
  using P = placeholder<Fmt, Pos, AutoIdx>;
  if constexpr (!P::found) {
    return walk_literal<Fmt, Pos, flen(Fmt)>(out, pos);
  } else {
    pos = walk_literal<Fmt, Pos, P::info.open>(out, pos);
    constexpr auto f = conversion<arg_t<P, Args...>, P::spec>::format;
    for (size_t i = 0; i < f.len; i++, pos++)
      if (out) out[pos] = f.data[i];
    return write_printf_fmt<Fmt, P::info.close + 1, P::next_auto, Args...>(out, pos);
  }
}

// The printf arguments, in placeholder order ("{0} {1} {0}" → a, b, a).
template <fixed_string Fmt, size_t Pos, size_t AutoIdx, typename... Args>
inline auto printf_args(const std::tuple<Args...> &args) {
  using P = placeholder<Fmt, Pos, AutoIdx>;
  if constexpr (!P::found) {
    return std::tuple<>();
  } else {
    using C = conversion<arg_t<P, Args...>, P::spec>;
    return std::tuple_cat(std::tuple(C::arg(std::get<P::index>(args))),
                          printf_args<Fmt, P::info.close + 1, P::next_auto>(args));
  }
}

template <fixed_string Fmt, typename... Args> consteval auto printf_fmt() {
  fixed_string<write_printf_fmt<Fmt, 0, 0, Args...>(nullptr) + 1> r{};
  write_printf_fmt<Fmt, 0, 0, Args...>(r.data);
  return r;
}

// No arguments: the literal text ('%' kept as is) goes through "%s", since
// printf(fmt) alone trips -Wformat-security. Passed directly rather than via
// a tuple: Intel's SPIR-V printf at -O0 faults on a string pointer loaded
// from memory.
template <fixed_string Fmt> consteval auto literal_text() {
  fixed_string<walk_literal<Fmt, 0, flen(Fmt), false>(nullptr) + 1> r{};
  walk_literal<Fmt, 0, flen(Fmt), false>(r.data);
  return r;
}

template <fixed_string Lit, size_t... Is> inline void emit_literal(std::index_sequence<Is...>) {
  static constexpr FFMT_CONST_AS char s[] = {Lit.data[Is]..., '\0'};
  static constexpr FFMT_CONST_AS char fmt_s[] = "%s";
  FFMT_EMIT_PRINTF(fmt_s, s);
}

// One printf call; the format string lands in constant address space.
template <fixed_string PrintfFmt, size_t... Is, typename Tuple, size_t... Js>
inline void emit_printf(std::index_sequence<Is...>, const Tuple &args, std::index_sequence<Js...>) {
  static constexpr FFMT_CONST_AS char s[] = {PrintfFmt.data[Is]..., '\0'};
  FFMT_EMIT_PRINTF(s, std::get<Js>(args)...);
}

template <fixed_string Fmt, typename... Args> inline void print(Args... args) {
  static_assert(supported<Fmt, 0, 0, Args...>(),
                "This format string uses features the specifiers path (DPC++, "
                "icpx OpenMP) cannot express with printf: {:b}, {:a}, {:^}, custom "
                "fill, {:x}/{:o} with signed int, {:#x}, dynamic width/precision.");
  constexpr auto pf = printf_fmt<Fmt, Args...>();
  if constexpr (sizeof...(Args) == 0) {
    constexpr auto lit = literal_text<Fmt>();
    if constexpr (flen(lit) > 0) emit_literal<lit>(std::make_index_sequence<flen(lit)>{});
  } else {
    auto a = printf_args<Fmt, 0, 0>(std::tuple<Args...>(args...));
    emit_printf<pf>(std::make_index_sequence<flen(pf)>{}, a,
                    std::make_index_sequence<std::tuple_size_v<decltype(a)>>{});
  }
}
} // namespace specifiers_path
#endif // !FFMT_BUFFER_PATH

#if FFMT_BUFFER_PATH
// ============================================================
// ACPP accumulator path
//
// Collects the entire formatted output into one fmt_buf, then
// flushes with a single sycl::detail::print call — atomic per
// work-item, no interleaving between format args.
// ============================================================

namespace buffer_path {

template <typename T> FMT_HD inline void write_decimal(fmt_buf &out, T val) {
  using U = std::make_unsigned_t<T>;
  U uval;
  if constexpr (std::signed_integral<T>) {
    if (val < 0) {
      out.push('-');
      // Store in a typed variable: subtraction of two uint8_t/uint16_t promotes
      // to int, so passing the expression directly would deduce U=int instead of
      // the actual unsigned type, making t>0 false on a negative int.
      uval = U(0) - static_cast<U>(val);
    } else {
      uval = static_cast<U>(val);
    }
  } else {
    uval = static_cast<U>(val);
  }
  write_uint_direct<10>(out, uval);
}


// Byte length of NUL-terminated s, stopping before its display width would
// exceed max_cols (-1: no limit, else string precision); the width goes to
// cols. ASCII, the common case, costs one compare per byte and never decodes.
// With FFMT_UNICODE=0 every byte is one column.
FMT_HD inline int str_width(const char *s, int max_cols, int &cols) {
  int i = 0, w = 0;
  while (s[i] && (max_cols < 0 || w < max_cols)) {
#if FFMT_UNICODE
    if (static_cast<unsigned char>(s[i]) >= 0x80) {
      uint32_t cp;
      int n = utf8_decode(s + i, cp);
      int cw = display_width_of(cp);
      if (max_cols >= 0 && w + cw > max_cols) break;
      i += n;
      w += cw;
      continue;
    }
#endif
    i++;
    w++;
  }
  cols = w;
  return i;
}

// len is in bytes, cols (data's display width) is what width counts.
FMT_HD inline void apply_padding_data(fmt_buf &out, const char *data, int len, int cols,
                               const fill_char &fill, char align, int width) {
  int pad = width > cols ? width - cols : 0;
  if (pad == 0) { out.push_data(data, len); }
  else if (align == '<') { out.push_data(data, len); out.push_n(fill, pad); }
  else if (align == '^') { out.push_n(fill, pad / 2); out.push_data(data, len); out.push_n(fill, pad - pad / 2); }
  else { out.push_n(fill, pad); out.push_data(data, len); }
}

// Pad/sign/zfill content already at out.data[content_start..out.len) in place.
// Final layout: [lpad fill][sign][prefix][zfill '0'][content][rpad fill].
// Avoids a stack temporary by shifting the already-written content right by
// `prepend` bytes, then filling the gap. Per-write `p < end` checks let the
// loops keep running past the buffer cap (matching push_n's silent-truncate).
FMT_HD inline void pad_in_place(fmt_buf &out, int content_start,
                         char sign_ch, const char *prefix, int prefix_n, int zfill,
                         const fill_char &fill, char align, int width) {
  int content_len = out.len - content_start;
  int sign_n = sign_ch ? 1 : 0;
  int total = sign_n + prefix_n + zfill + content_len;
  int pad = width > total ? width - total : 0;
  int lpad = (align == '>') ? pad : (align == '^') ? pad / 2 : 0;
  int rpad = pad - lpad;
  int prepend = lpad * fill.size + sign_n + prefix_n + zfill;

  if (prepend > 0) {
    int end = fmt_buf::cap;
    // Drop digits that wouldn't fit after the shift, then move what survives.
    int kept = content_len;
    if (content_start + prepend + kept > end) kept = end - content_start - prepend;
    if (kept < 0) kept = 0;
    for (int i = kept - 1; i >= 0; i--)
      out.data[content_start + prepend + i] = out.data[content_start + i];
    int p = content_start;
    for (int i = 0; i < lpad && p + fill.size <= end; i++)
      for (int k = 0; k < fill.size; k++) out.data[p++] = fill.data[k];
    if (sign_ch && p < end) out.data[p++] = sign_ch;
    for (int i = 0; i < prefix_n && p < end; i++) out.data[p++] = prefix[i];
    for (int i = 0; i < zfill && p < end; i++) out.data[p++] = '0';
    out.len = p + kept;
  }
  out.push_n(fill, rpad);
}

// ── Float formatting helpers ──────────────────────────────────────────────────
//
// {:f} / {:e} / {:g} need correctly rounded digits at any precision. Scaling
// by 10^prec in a double/uint64 is only exact while val·10^prec < 2^53, so it
// garbled e.g. {:f} of 1e15 or {:.20f} of 0.1. Instead, every finite double
// is M·2^E (M < 2^53), so its decimal expansion is finite and a small bignum
// yields it digit by digit, exactly:
//   E >= 0: the value is the integer M·2^E (<= 309 digits), kept in base 1e9.
//   E <  0: the integer part M >> k (k = -E) fits a uint64; the fraction
//           r/2^k is kept in binary and each r *= 10 step yields one digit.
// One 35-limb array (140 B) covers both: 2^1024 < 10^315 = 35 base-1e9 limbs,
// and r·10 < 2^(k+4) <= 2^1078 fits in 34 binary limbs.

// Exact decimal digits of a positive finite double, most significant first.
// next() walks the integer digits, then the fraction digits (0 forever once
// the expansion ends). int_digits == 0 when the value is below 1.
struct exact_decimal {
  static constexpr int LIMBS = 35;
  static constexpr uint32_t BASE = 1000000000u;
  uint32_t a[LIMBS];
  int n = 0;           // limbs in use
  int k = 0;           // fraction width in bits; 0 when the value is an integer
  uint64_t ipart = 0;  // integer part when k > 0
  int int_digits = 0;
  int top_digits = 0;  // decimal digits in a[n-1] when k == 0
  int pos = 0;         // digits consumed by next()

  FMT_HD explicit exact_decimal(double v) {
    uint64_t bits = __builtin_bit_cast(uint64_t, v);
    int be = static_cast<int>((bits >> 52) & 0x7FF);
    uint64_t m = bits & 0x000FFFFFFFFFFFFFULL;
    int e;
    if (be == 0) e = -1074;
    else { m |= 1ULL << 52; e = be - 1075; }
    if (e >= 0) {
      for (uint64_t t = m; t; t /= BASE) a[n++] = static_cast<uint32_t>(t % BASE);
      while (e > 0) { // multiply by 2^e, at most 2^32 per pass
        int s = e > 32 ? 32 : e;
        e -= s;
        uint64_t carry = 0;
        for (int i = 0; i < n; i++) {
          uint64_t x = (static_cast<uint64_t>(a[i]) << s) + carry;
          a[i] = static_cast<uint32_t>(x % BASE);
          carry = x / BASE;
        }
        while (carry) { a[n++] = static_cast<uint32_t>(carry % BASE); carry /= BASE; }
      }
      top_digits = dragonbox::count_digits(a[n - 1]);
      int_digits = 9 * (n - 1) + top_digits;
    } else {
      k = -e;
      uint64_t r = m;
      if (k < 64) { ipart = m >> k; r = m & ((1ULL << k) - 1); }
      n = (k + 4 + 31) / 32;
      for (int i = 0; i < n; i++) a[i] = 0;
      a[0] = static_cast<uint32_t>(r);
      if (n > 1) a[1] = static_cast<uint32_t>(r >> 32);
      int_digits = ipart ? dragonbox::count_digits(ipart) : 0;
    }
  }

  // j-th integer digit from the top (k == 0 only).
  FMT_HD int int_digit(int j) const {
    if (j < top_digits)
      return static_cast<int>(a[n - 1] / dragonbox::pow10_u64(top_digits - 1 - j) % 10);
    j -= top_digits;
    return static_cast<int>(a[n - 2 - j / 9] / dragonbox::pow10_u64(8 - j % 9) % 10);
  }

  FMT_HD int next() {
    int j = pos++;
    if (j < int_digits) {
      if (k == 0) return int_digit(j);
      return static_cast<int>(ipart / dragonbox::pow10_u64(int_digits - 1 - j) % 10);
    }
    if (k == 0) return 0;
    uint64_t carry = 0;
    for (int i = 0; i < n; i++) {
      uint64_t x = static_cast<uint64_t>(a[i]) * 10 + carry;
      a[i] = static_cast<uint32_t>(x);
      carry = x >> 32;
    }
    // The digit is bits [k, k+4); clear them to keep r < 2^k.
    int li = k / 32, sh = k % 32;
    uint64_t w = a[li];
    if (li + 1 < n) w |= static_cast<uint64_t>(a[li + 1]) << 32;
    a[li] &= (1u << sh) - 1;
    if (li + 1 < n) a[li + 1] = 0;
    return static_cast<int>((w >> sh) & 0xF);
  }

  // Is any digit at or after the current position nonzero?
  FMT_HD bool rest_nonzero() const {
    if (k == 0) {
      for (int j = pos; j < int_digits; j++)
        if (int_digit(j)) return true;
      return false;
    }
    if (pos < int_digits && ipart % dragonbox::pow10_u64(int_digits - pos) != 0) return true;
    for (int i = 0; i < n; i++)
      if (a[i]) return true;
    return false;
  }
};

// Writes the digits of one number into out. Digits past the cap are dropped
// like push() does, but remembered: a rounding carry only crosses them into
// the visible digits when they are all '9'.
struct digit_writer {
  fmt_buf &out;
  int first;               // index of the first digit
  int last = 0;            // last digit put (its parity decides ties)
  bool blocked = false;    // a digit past the cap was not '9': no carry gets out

  FMT_HD explicit digit_writer(fmt_buf &o) : out(o), first(o.len) {}

  FMT_HD void put(int d) {
    last = d;
    if (out.len < fmt_buf::cap) out.data[out.len++] = static_cast<char>('0' + d);
    else if (d != 9) blocked = true;
  }

  // Round half to even, given the first dropped digit and whether anything
  // nonzero follows it. Returns true when the carry ran off the front
  // (99.9 → 00.0); the caller then supplies the leading '1'.
  FMT_HD bool round(int next_digit, bool sticky) {
    if (next_digit < 5 || (next_digit == 5 && !sticky && (last & 1) == 0)) return false;
    if (blocked) return false;
    for (int i = out.len - 1; i >= first; i--) {
      char c = out.data[i];
      if (c == '.') continue;
      if (c != '9') { out.data[i] = static_cast<char>(c + 1); return false; }
      out.data[i] = '0';
    }
    return true;
  }
};

// Puts the decimal point (if prec > 0 or alt) and `prec` more digits of x,
// rounded half to even. Returns true when the carry ran off the front.
FMT_HD inline bool put_fraction(fmt_buf &out, digit_writer &w, exact_decimal &x,
                                int prec, bool alt) {
  if (prec > 0 || alt) out.push('.');
  // The expansion is finite: once it runs out the rest is zeros and nothing
  // needs rounding, so even a huge precision costs at most ~1100 steps.
  int i = 0;
  for (; i < prec && x.rest_nonzero(); i++) w.put(x.next());
  if (i < prec) { out.push_n('0', prec - i); return false; }
  int next_digit = x.next();
  return w.round(next_digit, x.rest_nonzero());
}

// Fixed notation with `prec` fraction digits, for a non-negative finite val.
FMT_HD inline void fmt_fixed(fmt_buf &out, double val, int prec, bool alt = false) {
  digit_writer w(out);
  exact_decimal x(val);
  if (x.int_digits == 0) w.put(0);
  for (int i = 0; i < x.int_digits; i++) w.put(x.next());
  if (put_fraction(out, w, x, prec, alt)) {
    // 99.9 → 100.0: shift right by one and prepend the carry.
    int end = out.len < fmt_buf::cap ? out.len + 1 : fmt_buf::cap;
    for (int j = end - 1; j > w.first; j--) out.data[j] = out.data[j - 1];
    out.data[w.first] = '1';
    out.len = end;
  }
}

// Scientific notation d.ddd…e±XX with `prec` fraction digits, for a
// non-negative finite val. Returns the decimal exponent after rounding.
FMT_HD inline int fmt_sci(fmt_buf &out, double val, int prec, bool upper, bool alt = false) {
  digit_writer w(out);
  exact_decimal x(val);
  int exp = x.int_digits - 1; // exponent of the next digit
  int d = 0;
  if (x.rest_nonzero())
    while ((d = x.next()) == 0) exp--; // skip leading zeros of a value < 1
  else
    exp = 0;                           // zero: 0.000e+00
  w.put(d);
  if (put_fraction(out, w, x, prec, alt)) { // 9.99e+00 → 1.00e+01
    out.data[w.first] = '1';
    exp++;
  }
  out.push(upper ? 'E' : 'e');
  out.push(exp < 0 ? '-' : '+');
  int abs_exp = exp < 0 ? -exp : exp;
  if (abs_exp < 10)
    out.push('0'); // at least 2 exponent digits
  write_decimal(out, static_cast<unsigned>(abs_exp));
  return exp;
}

// Remove trailing zeros (and decimal point) from buf[start..len),
// stopping at 'e'/'E' if present (scientific notation).
FMT_HD inline void trim_trailing_zeros(fmt_buf &buf, int start = 0) {
  int dot_pos = -1;
  int e_pos = buf.len;
  for (int i = start; i < buf.len; i++) {
    if (buf.data[i] == '.')
      dot_pos = i;
    else if (buf.data[i] == 'e' || buf.data[i] == 'E') {
      e_pos = i;
      break;
    }
  }
  if (dot_pos < 0)
    return;
  int trim = e_pos;
  while (trim > dot_pos + 1 && buf.data[trim - 1] == '0')
    trim--;
  if (trim == dot_pos + 1)
    trim = dot_pos; // remove dot itself
  int new_len = trim;
  for (int i = e_pos; i < buf.len; i++)
    buf.data[new_len++] = buf.data[i];
  buf.len = new_len;
  buf.data[new_len] = '\0';
}

// g/G per [format.string.std]: with P = prec (0 → 1) and X the exponent the
// 'e' form would have at precision P-1, use fixed with P-1-X digits when
// P > X >= -4, else scientific; trailing zeros are dropped unless alt.
FMT_HD inline void fmt_g(fmt_buf &out, double val, int prec, bool upper, bool alt) {
  if (prec == 0)
    prec = 1;
  int start = out.len;
  int x = fmt_sci(out, val, prec - 1, upper, alt);
  if (x >= -4 && x < prec) {
    out.len = start;
    fmt_fixed(out, val, prec - 1 - x, alt);
  }
  if (!alt)
    trim_trailing_zeros(out, start);
}

// ============================================================
// Runtime-spec formatting (for print("...", args...) syntax)
// ============================================================

// {:a} / {:A} digits for a non-negative finite double, as {fmt} prints them
// after its "0x" prefix (the sign and prefix come from write_float_rt;
// floats are widened exactly first). Leading 1, or 0 for zero and
// subnormals (0x0.0000000000001p-1022); 13 hex digits with trailing zeros
// trimmed. An explicit precision rounds half up on the first dropped hex
// digit, like {fmt} (0x1.08p+0 at .1 → 0x1.1p+0), and may carry into the
// leading digit (0x1.f8p+0 at .0 → 0x2p+0).
FMT_HD inline void hex_float_to_buf_rt(fmt_buf &content, double val, int prec, bool alt, bool upper) {
  constexpr int ndig = 13;
  uint64_t bits = __builtin_bit_cast(uint64_t, val);
  int biased = static_cast<int>((bits >> 52) & 0x7FF);
  uint64_t mant = bits & 0x000FFFFFFFFFFFFFULL;
  int lead = biased == 0 ? 0 : 1;
  int exponent = biased != 0 ? biased - 1023 : (mant == 0 ? 0 : -1022);
  int digits = ndig;
  if (prec >= 0 && prec < ndig) {
    int drop = (ndig - prec) * 4;
    bool round_up = ((mant >> (drop - 4)) & 0xF) >= 8;
    mant >>= drop;
    if (round_up) {
      mant++;
      if (mant >> (prec * 4)) { mant = 0; lead++; }
    }
    digits = prec;
  } else if (prec < 0) {
    while (digits > 0 && (mant & 0xF) == 0) { mant >>= 4; digits--; }
  }
  content.push(hex_digit(lead, upper));
  if (digits > 0 || alt) content.push('.');
  for (int i = digits - 1; i >= 0; i--)
    content.push(hex_digit(static_cast<int>((mant >> (i * 4)) & 0xF), upper));
  if (prec > ndig) content.push_n('0', prec - ndig);
  content.push(upper ? 'P' : 'p');
  if (exponent >= 0) content.push('+');
  else { content.push('-'); exponent = -exponent; }
  write_uint_direct<10>(content, static_cast<unsigned>(exponent));
}

// write_int with runtime spec and etype — writes directly to out (no stack temp).
// Digits are written into out at content_start; pad_in_place then shifts them
// right to make room for sign/prefix/zfill/lpad. Saves the 68 B `dgt[68]`.
template <typename T>
FMT_HD inline void write_int_rt(fmt_buf &out, T arg, const format_spec &spec, char etype, int width) {
  using U = std::decay_t<T>;
  using Uns = std::conditional_t<(sizeof(U) <= 4), unsigned, unsigned long long>;

  bool neg = false;
  Uns uval;
  if constexpr (std::same_as<U, bool>) {
    uval = static_cast<Uns>(arg);
  } else if constexpr (std::signed_integral<U>) {
    if (arg < 0) { neg = true; uval = Uns(0) - static_cast<Uns>(arg); }
    else uval = static_cast<Uns>(arg);
  } else {
    uval = static_cast<Uns>(arg);
  }

  char sc = neg ? '-' : (spec.sign == '+') ? '+' : (spec.sign == ' ') ? ' ' : '\0';

  int base = (etype == 'b' || etype == 'B') ? 2 : (etype == 'o') ? 8
             : (etype == 'x' || etype == 'X') ? 16 : 10;
  bool upper = (etype == 'X' || etype == 'B');

  char pfx[3] = {};
  int pfx_n = 0;
  if (spec.alt) {
    if (base == 2)       { pfx[0] = '0'; pfx[1] = upper ? 'B' : 'b'; pfx_n = 2; }
    else if (base == 16) { pfx[0] = '0'; pfx[1] = upper ? 'X' : 'x'; pfx_n = 2; }
    else if (base == 8 && uval != 0) { pfx[0] = '0'; pfx_n = 1; }
  }

  int content_start = out.len;
  switch (base) {
    case 2:  write_uint_raw<2,  false>(out.data, out.len, fmt_buf::cap, uval); break;
    case 8:  write_uint_raw<8,  false>(out.data, out.len, fmt_buf::cap, uval); break;
    case 16: if (upper) write_uint_raw<16, true>(out.data, out.len, fmt_buf::cap, uval);
             else       write_uint_raw<16, false>(out.data, out.len, fmt_buf::cap, uval);
             break;
    default: write_uint_raw<10, false>(out.data, out.len, fmt_buf::cap, uval); break;
  }

  int dlen = out.len - content_start;
  int content_w = (sc ? 1 : 0) + pfx_n + dlen;
  bool zpad = spec.zero_pad && !spec.align;
  int zfill = (zpad && width > content_w) ? width - content_w : 0;

  pad_in_place(out, content_start, sc, pfx, pfx_n, zfill,
               spec.fill, spec.align_or(), width);
}

// write_float with runtime spec and etype — writes digits directly into out,
// then pad_in_place handles sign/zfill/alignment. No stack temp.
template <typename T>
FMT_HD inline void write_float_rt(fmt_buf &out, T arg, const format_spec &spec, char etype,
                           int dyn_w, int dyn_p) {
  using bits_t = std::conditional_t<sizeof(T) == 4, uint32_t, uint64_t>;
  constexpr int mant_bits = std::numeric_limits<T>::digits - 1;
  constexpr bits_t sign_bit = bits_t(1) << (sizeof(T) * 8 - 1);
  constexpr bits_t mant_mask = (bits_t(1) << mant_bits) - 1;
  constexpr bits_t exp_mask = ~sign_bit & ~mant_mask;
  bool upper = (etype == 'F' || etype == 'E' || etype == 'G' || etype == 'A');

  bits_t bits = __builtin_bit_cast(bits_t, arg);
  bool neg = (bits & sign_bit) != 0;
  bool finite = (bits & exp_mask) != exp_mask;
  T val = __builtin_bit_cast(T, static_cast<bits_t>(bits & ~sign_bit));
  char sign_ch = neg ? '-' : (spec.sign == '+') ? '+' : (spec.sign == ' ') ? ' ' : '\0';

  const char *prefix = nullptr;
  int prefix_n = 0;
  int content_start = out.len;
  if (!finite) {
    bool nan = (bits & mant_mask) != 0;
    out.push_str(nan ? (upper ? "NAN" : "nan") : (upper ? "INF" : "inf"));
  } else if (etype == 'a' || etype == 'A') {
    prefix = upper ? "0X" : "0x"; // {fmt} always prints it; '0' pads after it
    prefix_n = 2;
    hex_float_to_buf_rt(out, widen_exact(val), dyn_p, spec.alt, upper);
  } else if (spec.type == '\0' && dyn_p < 0) {
    // No type, no precision ("{:10}", "{:+}", ...): the same shortest
    // round-trip form as plain "{}", in the argument's own precision.
    out.len += dragonbox::format_shortest(out.data + out.len, val);
    if (out.len > fmt_buf::cap) out.len = fmt_buf::cap;
    if (spec.alt) { // '#' forces a point: "1" → "1.", "1e+20" → "1.e+20"
      int e = content_start;
      while (e < out.len && out.data[e] != '.' && out.data[e] != 'e') e++;
      if ((e == out.len || out.data[e] == 'e') && out.len < fmt_buf::cap) {
        for (int i = out.len; i > e; i--) out.data[i] = out.data[i - 1];
        out.data[e] = '.';
        out.len++;
      }
    }
  } else {
    int prec = dyn_p >= 0 ? dyn_p : 6;
    double dv = widen_exact(val);
    if (etype == 'f' || etype == 'F') fmt_fixed(out, dv, prec, spec.alt);
    else if (etype == 'e' || etype == 'E') fmt_sci(out, dv, prec, upper, spec.alt);
    else fmt_g(out, dv, prec, upper, spec.alt); // g, G, or none + precision
  }

  int dlen = out.len - content_start;
  int content_w = (sign_ch ? 1 : 0) + prefix_n + dlen;
  // '0' is ignored for inf/nan (and whenever an alignment is given).
  bool zpad = spec.zero_pad && !spec.align && finite;
  int zfill = (zpad && dyn_w > content_w) ? dyn_w - content_w : 0;

  pad_in_place(out, content_start, sign_ch, prefix, prefix_n, zfill,
               spec.fill, spec.align_or(), dyn_w);
}

template <typename T> FMT_HD inline void write_arg_default(fmt_buf &out, T arg) {
  using U = std::decay_t<T>;
  if constexpr (std::same_as<U, bool>) {
    out.push_str(arg ? "true" : "false");
  } else if constexpr (std::same_as<U, char>) {
    out.push(arg);
  } else if constexpr (std::signed_integral<U> || std::unsigned_integral<U>) {
    write_decimal(out, arg);
  } else if constexpr (std::floating_point<U>) {
    write_float_rt(out, arg, format_spec{}, 'g', 0, -1); // shortest round-trip
  } else if constexpr (std::is_pointer_v<U>) {
    using Pointee = std::remove_cv_t<std::remove_pointer_t<U>>;
    if constexpr (std::same_as<Pointee, char>) {
      out.push_str(arg ? arg : "(null)"); // UB in std::format; glibc's choice
    } else {
      out.push_str("0x");
      write_uint_direct<16>(out, reinterpret_cast<std::uintptr_t>(arg));
    }
  }
}


// Workaround for clang-OMP-CUDA -O0 codegen bug: bundle two trailing
// by-value scalar ints into a struct so dispatch_arg / write_arg_rt can
// receive them via reference. Two trailing by-value ints on a function
// called from inside the dispatch_pack lambda triggers alloca aliasing
// at -O0 (V29 standalone reproducer; see implementation_limitation/).
struct dyn_args { int w; int p; };

// Format one argument with runtime spec — dispatches based on type + etype.
// No fmt_buf temporaries for bool/char/string; writes directly to out.
template <typename T>
FMT_HD inline void write_arg_rt(fmt_buf &out, T arg, const format_spec &spec, const dyn_args &dyn) {
  int dyn_w = dyn.w;
  int dyn_p = dyn.p;
  using U = std::decay_t<T>;

  if constexpr (std::same_as<U, bool>) {
    if (spec.type == '\0' || spec.type == 's') {
      const char *bs = arg ? "true" : "false";
      int blen = arg ? 4 : 5;
      apply_padding_data(out, bs, blen, blen, spec.fill, spec.align_or('<'), dyn_w);
      return;
    }
  }

  // Per-arg spec/type compatibility is enforced at consteval (see
  // spec_compatible_with_arg), so each `if constexpr` branch below covers
  // every (U, etype) pair that can actually reach this point.
  if constexpr (std::is_pointer_v<U> &&
                std::same_as<std::remove_cv_t<std::remove_pointer_t<U>>, char>) {
    if (spec.type != 'p') { // {:p} prints the address: falls through below
      const char *str = arg ? arg : "(null)"; // UB in std::format; never fault on device
      if (dyn_w <= 0 && dyn_p < 0) { out.push_str(str); return; }
      int cols = 0;
      int slen = str_width(str, dyn_p, cols);
      apply_padding_data(out, str, slen, cols, spec.fill, spec.align_or('<'), dyn_w);
      return;
    }
  }
  if constexpr (std::is_pointer_v<U>) {
    int content_start = out.len;
    write_uint_raw<16>(out.data, out.len, fmt_buf::cap,
                       reinterpret_cast<std::uintptr_t>(arg));
    pad_in_place(out, content_start, '\0', "0x", 2, 0,
                 spec.fill, spec.align_or('>'), dyn_w);
  } else if constexpr (std::floating_point<U>) {
    write_float_rt(out, arg, spec, effective_type<U>(spec.type), dyn_w, dyn_p);
  } else if constexpr (std::integral<U>) {
    char etype = effective_type<U>(spec.type);
    if (etype == 'c') {
      char ch = static_cast<char>(arg);
      apply_padding_data(out, &ch, 1, 1, spec.fill, spec.align_or('<'), dyn_w);
    } else {
      write_int_rt(out, arg, spec, etype, dyn_w);
    }
  }
}

// ============================================================
// Runtime pack dispatch + format loop
// ============================================================
// dispatch_pack folds over the args pack directly.
template <typename F, typename... Args>
FMT_HD inline void dispatch_pack(int idx, F &&fn, Args &&... args) {
  int i = 0;
  (((i++ == idx) ? (fn(args), void()) : void()), ...);
}

template <typename... Args>
FMT_HD inline void resolve_int_arg(int idx, int &out, Args&&... args) {
  dispatch_pack(idx,
    [&out](auto val) {
      if constexpr (std::integral<std::decay_t<decltype(val)>>)
        out = static_cast<int>(val);
    }, args...);
}

// Copy str[from, to) unescaping "{{" and "}}". Pointer-based: GCC's
// -Warray-bounds misjudged the index form as possibly negative.
FMT_HD inline void write_literal_segment(fmt_buf &out, const char *str, int from, int to) {
  for (const char *p = str + from, *end = str + to; p < end; p++) {
    if ((*p == '{' || *p == '}') && p + 1 < end && p[1] == *p) p++;
    out.push(*p);
  }
}

// Forward decl: the literal-walking format loop is needed by dispatch_arg
// (for formatter inner sub-strings) but its definition wants dispatch_arg.
template <sycl_formattable... Args>
FMT_HD inline void format_lit_rt(fmt_buf &out, const char *fmt, int fmt_len, Args... args);

// Per-arg dispatch. Primitives use the spec-aware writers; formatter args
// recurse into format_lit_rt on the formatter's inner format string + values.
// Both branches are if-constexpr so the primitive path stays byte-identical.
template <typename... Args>
FMT_HD inline void dispatch_arg(fmt_buf &out, int idx, bool has_spec,
                         const format_spec &spec, const dyn_args &dyn,
                         Args&&... args) {
  dispatch_pack(idx,
    [&]<typename T>(T arg) {
      if constexpr (sycl_printable<std::decay_t<T>>) {
        if (has_spec) write_arg_rt(out, arg, spec, dyn);
        else write_arg_default(out, arg);
      } else {
        // has_formatter<T> — specs/dynamic-width on custom args are not
        // supported on ACPP at runtime; the inner format literal is rewalked.
        auto inner = ::ffmt::formatter<std::decay_t<T>>::format(arg);
        constexpr auto inner_fmt = decltype(inner)::format_string;
        std::apply(
            [&](auto... vs) {
              format_lit_rt(out, inner_fmt.data, static_cast<int>(flen(inner_fmt)), vs...);
            },
            inner.values);
      }
    }, args...);
}

// Top-level walker. Reuses the pre-parsed phs[] for the outer format string
// (so spec handling for primitives is unchanged) and falls through to
// dispatch_arg for both primitive and formatter args.
template <sycl_formattable... Args, typename... A2>
FMT_HD inline void format_rt(fmt_buf &out, const print_string<Args...> &ps, A2&&... args) {
  int pos = 0;
  for (int i = 0; i < ps.ph_count; i++) {
    const auto &e = ps.phs[i];
    write_literal_segment(out, ps.str, pos, e.open);
    dyn_args dyn{e.spec.width, e.spec.precision};
    if (e.spec.width_arg >= 0) resolve_int_arg(e.spec.width_arg, dyn.w, args...);
    if (e.spec.prec_arg >= 0)  resolve_int_arg(e.spec.prec_arg,  dyn.p, args...);
    dispatch_arg(out, e.arg_idx, e.has_spec, e.spec, dyn, args...);
    pos = e.close + 1;
  }
  write_literal_segment(out, ps.str, pos, ps.len);
}

// Inner walker for formatter sub-strings. These don't have a print_string
// (no compile-time pre-parse), so we re-parse with find_placeholder.
// No spec/dyn-width support here — Stage 1 forbids them on custom args.
template <sycl_formattable... Args>
FMT_HD inline void format_lit_rt(fmt_buf &out, const char *fmt, int fmt_len, Args... args) {
  int pos = 0;
  int auto_idx = 0;
  format_spec empty{};
  while (true) {
    auto ph = find_placeholder(fmt, fmt_len, pos);
    if (!ph) break;
    write_literal_segment(out, fmt, pos, static_cast<int>(ph->open));
    int idx = (ph->index >= 0) ? ph->index : auto_idx++;
    dyn_args dyn{0, -1};
    dispatch_arg(out, idx, /*has_spec*/ false, empty, dyn, args...);
    pos = static_cast<int>(ph->close) + 1;
  }
  write_literal_segment(out, fmt, pos, fmt_len);
}

// Escape % → %% in place. Required for backends whose emit syscall is a
// printf-family function (CUDA's vprintf interprets %); harmless to skip
// when emit is a verbatim writer like fputs. Lives outside flush_buf so
// custom FFMT_EMIT_BUFFER overrides can opt into it if they need it.
//
// When the escaped text no longer fits in `cap`, the longest prefix whose
// escaped form fits is kept (a '%' is never split from its twin), and a
// trailing '\n' from println survives the truncation.
FMT_HD inline void escape_percent_inplace(fmt_buf &out) {
  bool nl = out.len > 0 && out.data[out.len - 1] == '\n';
  int body = out.len - (nl ? 1 : 0);
  int src_n = 0, dst_n = 0;
  while (src_n < body) {
    int w = (out.data[src_n] == '%') ? 2 : 1;
    if (dst_n + w > fmt_buf::cap) break;
    dst_n += w;
    src_n++;
  }
  if (dst_n == src_n && src_n == body) return; // no '%' and nothing dropped
  // Expand right-to-left. dst - src equals the number of '%' still to the
  // left, so it never goes negative: no unread byte is overwritten.
  for (int s = src_n - 1, d = dst_n - 1; s >= 0; s--) {
    char c = out.data[s];
    out.data[d--] = c;
    if (c == '%') out.data[d--] = '%';
  }
  out.len = dst_n;
  if (nl) out.data[out.len++] = '\n';
}

// Flush buf: null-terminate and hand the bytes to FFMT_EMIT_BUFFER.
// The default macro selects the right syscall per backend; users can override
// to point at OpenMP, an instrumented stream, etc. The `escape_pct` flag is
// honored by the default ACPP emit (CUDA vprintf), ignored otherwise.
FMT_HD inline void flush_buf(fmt_buf &out, bool escape_pct = true) {
  out.data[out.len] = '\0';
  FFMT_EMIT_BUFFER(out, escape_pct);
}
} // namespace buffer_path

#endif // FFMT_BUFFER_PATH

} // namespace detail


// ============================================================
// formatter_expand — splice user `formatter<T>` results into
// the parent format string + arg pack at compile time. DPC++
// only: ACPP dispatches formatter args at runtime inside
// buffer_path::dispatch_arg, so this machinery would be dead
// weight there.
// ============================================================

#if !FFMT_BUFFER_PATH
namespace detail {
namespace formatter_expand {

// One round of splicing: each custom-formatter argument is replaced by its
// inner format string and its values. The spliced string uses explicit
// indices into the flattened value list, so positional and repeated
// placeholders keep pointing at the right values ("{1} {0}" and "({1}, {0})"
// both work). Custom values left over are spliced by the next print<>.
template <typename T>
inline constexpr auto inner_format_string =
    decltype(formatter<std::decay_t<T>>::format(std::declval<T>()))::format_string;

template <typename T> consteval size_t n_values() {
  if constexpr (sycl_printable<T>) return 1;
  else return std::tuple_size_v<decltype(formatter<T>::format(std::declval<T>()).values)>;
}

struct inner_fmt {
  const char *data;
  int len;
};

struct splice_arg {
  size_t base = 0;                 // index of its first value in the flattened list
  std::optional<inner_fmt> inner;  // custom formatter's format string, if any
};

template <typename T> consteval splice_arg splice_arg_of(size_t base) {
  if constexpr (sycl_printable<T>) return {base, std::nullopt};
  else return {base, inner_fmt{inner_format_string<T>.data, static_cast<int>(flen(inner_format_string<T>))}};
}

template <typename... Args> struct splice_table {
  splice_arg a[sizeof...(Args)];
  consteval splice_table() {
    size_t base = 0, i = 0;
    ((a[i++] = splice_arg_of<Args>(base), base += n_values<Args>()), ...);
  }
};

// Writes (out != nullptr) or measures the spliced format string.
template <typename... Args> consteval size_t splice(char *out, const char *s, int len) {
  constexpr splice_table<Args...> tab;
  size_t op = 0;
  auto put = [&](char c) { if (out) out[op] = c; op++; };
  auto copy = [&](const char *b, const char *e) { while (b < e) put(*b++); };
  // Copies text, rewriting each placeholder's index i to base + i.
  auto reindex = [&](const char *t, int n, size_t base, auto &&on_arg) {
    int pos = 0, auto_idx = 0;
    for (auto ph = find_placeholder(t, n, 0); ph;
         ph = find_placeholder(t, n, pos)) {
      copy(t + pos, t + ph->open);
      on_arg(static_cast<size_t>(ph->index < 0 ? auto_idx++ : ph->index), base,
             t + (ph->has_spec ? ph->spec_beg - 1 : ph->close), t + ph->close);
      pos = static_cast<int>(ph->close) + 1;
    }
    copy(t + pos, t + n);
  };
  auto placeholder = [&](size_t i, size_t base, const char *spec, const char *spec_end) {
    char d[20];
    int k = 0;
    for (size_t v = base + i; k == 0 || v; v /= 10) d[k++] = static_cast<char>('0' + v % 10);
    put('{');
    while (k) put(d[--k]);
    copy(spec, spec_end); // ":spec" or nothing
    put('}');
  };
  reindex(s, len, 0, [&](size_t i, size_t, const char *spec, const char *spec_end) {
    const splice_arg &a = tab.a[i];
    // Not a null-pointer sentinel: GCC rejects comparing a variable
    // template's address with null in a constant expression.
    if (!a.inner) placeholder(0, a.base, spec, spec_end);
    else reindex(a.inner->data, a.inner->len, a.base, placeholder);
  });
  return op;
}

template <fixed_string Fmt, typename... Args> consteval auto splice_format() {
  fixed_string<splice<Args...>(nullptr, Fmt.data, static_cast<int>(flen(Fmt))) + 1> r{};
  splice<Args...>(r.data, Fmt.data, static_cast<int>(flen(Fmt)));
  return r;
}

template <typename T> constexpr auto values_of(T arg) {
  if constexpr (sycl_printable<T>) return std::tuple<T>{arg};
  else return formatter<T>::format(arg).values;
}

// Forward to the public print<Fmt>(args...) by unpacking a tuple of expanded args.
// Kept as a free function (not a lambda) so Fmt2 can be used as an NTTP without
// the C++ "captureless lambda + non-type template param" pitfall that clang flags.
template <fixed_string Fmt2, typename Tup, size_t... Is>
inline void apply_print(Tup &t, std::index_sequence<Is...>);

} // namespace formatter_expand
} // namespace detail
#endif // !FFMT_BUFFER_PATH

namespace detail {

template <fixed_string Fmt> consteval auto append_newline() {
  constexpr size_t len = flen(Fmt);
  fixed_string<len + 2> result; // +1 for '\n', +1 for '\0'
  for (size_t i = 0; i < len; ++i)
    result.data[i] = Fmt[i];
  result.data[len] = '\n';
  result.data[len + 1] = '\0';
  return result;
}

} // namespace detail


// ============================================================
// Public API
// ============================================================

#if !FFMT_BUFFER_PATH

// DPC++ path: the format string must reach the consteval splicer as an NTTP,
// so the public entry point is a function template parameterized on it. The
// FFMT_PRINT macro hides the angle-bracket call shape.
template <detail::fixed_string Fmt, sycl_formattable... Args>
inline void print(Args... args) {
  if constexpr ((detail::sycl_printable<std::decay_t<Args>> && ...)) {
    detail::validate_format<std::decay_t<Args>...>(Fmt.data, static_cast<int>(detail::flen(Fmt)));
    detail::specifiers_path::print<Fmt>(args...);
  } else {
    // Splice the custom formatters in (compile time) and print the result;
    // validated first so a bad index fails here, not inside the splicer.
    detail::validate_format<std::decay_t<Args>...>(Fmt.data, static_cast<int>(detail::flen(Fmt)));
    constexpr auto Fmt2 = detail::formatter_expand::splice_format<Fmt, std::decay_t<Args>...>();
    auto values = std::tuple_cat(detail::formatter_expand::values_of(args)...);
    detail::formatter_expand::apply_print<Fmt2>(
        values, std::make_index_sequence<std::tuple_size_v<decltype(values)>>{});
  }
}

template <detail::fixed_string Fmt, sycl_formattable... Args>
inline void println(Args... args) {
  print<detail::append_newline<Fmt>()>(args...);
}

#else // FFMT_BUFFER_PATH

// ACPP path: keep the value-style public API
//   ffmt::println("…", args…)
// for both primitive and formatter args. The format literal is captured by
// print_string's consteval ctor (so no NTTP at the call site). format_rt
// now dispatches both primitive and formatter args via if constexpr — the
// primitive path stays byte-identical, formatter args recurse into the
// inner walker on the formatter's sub-format-string.
template <sycl_formattable... Args>
FMT_HD inline void print(const detail::print_string<std::type_identity_t<Args>...> &ps, Args... args) {
  detail::fmt_buf out;
  detail::buffer_path::format_rt(out, ps, args...);
  detail::buffer_path::flush_buf(out, ps.needs_pct_escape);
}

template <sycl_formattable... Args>
FMT_HD inline void println(const detail::print_string<std::type_identity_t<Args>...> &ps, Args... args) {
  detail::fmt_buf out;
  detail::buffer_path::format_rt(out, ps, args...);
  // Not push(): a truncated line must still end in '\n', or the next print
  // gets glued onto it. data[] has ExtraPad bytes past cap for this.
  out.data[out.len++] = '\n';
  detail::buffer_path::flush_buf(out, ps.needs_pct_escape);
}

#endif // FFMT_BUFFER_PATH

} // namespace ffmt

// Definition of the apply_print helper forward-declared above.
// Lives outside the public API section because it calls back into
// ffmt::print<Fmt2>(...) which must already be declared.
#if !FFMT_BUFFER_PATH
namespace ffmt::detail::formatter_expand {
template <::ffmt::detail::fixed_string Fmt2, typename Tup, size_t... Is>
inline void apply_print(Tup &t, std::index_sequence<Is...>) {
  ::ffmt::print<Fmt2>(std::get<Is>(t)...);
}
} // namespace ffmt::detail::formatter_expand
#endif // !FFMT_BUFFER_PATH

// ============================================================
// Built-in formatter specializations for SYCL types
// ============================================================
// Only enabled when <sycl/sycl.hpp> is in play (skips host-only coverage builds
// which include the header without SYCL).

#if defined(SYCL_LANGUAGE_VERSION) || FFMT_COMPILER_ACPP

namespace ffmt {
namespace detail {

// Build "{}", "{}x{}", "{}x{}x{}", ... with a chosen separator.
template <int N, char Sep> consteval auto make_sep_fmt() {
  static_assert(N >= 1, "make_sep_fmt requires N >= 1");
  // Output size: N copies of "{}" + (N-1) separators + '\0'
  constexpr size_t Sz = static_cast<size_t>(N) * 2 + (N - 1) + 1;
  fixed_string<Sz> r{};
  size_t p = 0;
  for (int i = 0; i < N; i++) {
    if (i > 0) r.data[p++] = Sep;
    r.data[p++] = '{';
    r.data[p++] = '}';
  }
  r.data[p] = '\0';
  return r;
}

// Build "({})", "({}, {})", "({}, {}, {})", ...
template <int N> consteval auto make_id_fmt() {
  static_assert(N >= 1, "make_id_fmt requires N >= 1");
  // "(" + N×"{}" + (N-1)×", " + ")" + '\0'
  constexpr size_t Sz = 1 + static_cast<size_t>(N) * 2 + (N - 1) * 2 + 1 + 1;
  fixed_string<Sz> r{};
  size_t p = 0;
  r.data[p++] = '(';
  for (int i = 0; i < N; i++) {
    if (i > 0) { r.data[p++] = ','; r.data[p++] = ' '; }
    r.data[p++] = '{';
    r.data[p++] = '}';
  }
  r.data[p++] = ')';
  r.data[p] = '\0';
  return r;
}

} // namespace detail

// sycl::range<N> -> "AxBxC"
template <int N>
struct formatter<::sycl::range<N>> {
  static constexpr auto format(::sycl::range<N> r) {
    return [&]<size_t... Is>(std::index_sequence<Is...>) {
      return formatted<detail::make_sep_fmt<N, 'x'>(),
                       std::decay_t<decltype(r[Is])>...>{ {r[Is]...} };
    }(std::make_index_sequence<N>{});
  }
};

// sycl::id<N> -> "(A, B, C)"
template <int N>
struct formatter<::sycl::id<N>> {
  static constexpr auto format(::sycl::id<N> id) {
    return [&]<size_t... Is>(std::index_sequence<Is...>) {
      return formatted<detail::make_id_fmt<N>(),
                       std::decay_t<decltype(id[Is])>...>{ {id[Is]...} };
    }(std::make_index_sequence<N>{});
  }
};

// sycl::item<N, WithOffset> -> "item(global=(...), range=...)" — recursively
// resolved through the formatters for sycl::id and sycl::range. The second
// template parameter exists in both DPC++ and ACPP; a single specialization
// covers both backends and both WithOffset values.
template <int N, bool WithOffset>
struct formatter<::sycl::item<N, WithOffset>> {
  static constexpr auto format(::sycl::item<N, WithOffset> it) {
    return formatted<detail::fixed_string{"item(global={}, range={})"},
                     ::sycl::id<N>, ::sycl::range<N>>{
      {it.get_id(), it.get_range()}
    };
  }
};

// sycl::nd_item<N> -> "nd_item(global=..., local=..., range=...)"
template <int N>
struct formatter<::sycl::nd_item<N>> {
  static constexpr auto format(::sycl::nd_item<N> nd) {
    return formatted<detail::fixed_string{"nd_item(global={}, local={}, range={})"},
                     ::sycl::id<N>, ::sycl::id<N>, ::sycl::range<N>>{
      {nd.get_global_id(), nd.get_local_id(), nd.get_global_range()}
    };
  }
};

} // namespace ffmt

#endif // SYCL_LANGUAGE_VERSION || ACPP

// Convenience macro — nicer syntax without explicit template angle brackets
#if FFMT_BUFFER_PATH
#define FFMT_PRINT(fmtstr, ...) ::ffmt::print(fmtstr __VA_OPT__(,) __VA_ARGS__)
#define FFMT_PRINTLN(fmtstr, ...) ::ffmt::println(fmtstr __VA_OPT__(,) __VA_ARGS__)
#else
#define FFMT_PRINT(fmtstr, ...) ::ffmt::print<fmtstr>(__VA_ARGS__)
#define FFMT_PRINTLN(fmtstr, ...) ::ffmt::println<fmtstr>(__VA_ARGS__)
#endif
