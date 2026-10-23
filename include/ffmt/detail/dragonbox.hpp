// The following Dragonbox conversion is adapted from fmt 12.0.1 and the
// Dragonbox implementation by Junekey Jeon. The fmt adaptation is MIT licensed;
// Dragonbox upstream offers Boost/Apache-2.0-with-LLVM-exception licenses. This
// implementation is independent of the pinned host oracle; see NOTICE.
#pragma once
#ifdef _OPENMP
#pragma omp declare target
#endif
namespace ffmt::detail {
namespace dragonbox {

struct uint128 {
  uint64_t hi, lo;
  FMT_HD constexpr uint128(uint64_t h, uint64_t l) : hi(h), lo(l) {}
  FMT_HD constexpr uint64_t high() const { return hi; }
  FMT_HD constexpr uint64_t low() const { return lo; }
  FMT_HD constexpr uint128 &operator+=(uint64_t rhs) {
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
  return {ac + (mid >> 32) + (ad >> 32) + (bc >> 32),
          (mid << 32) + (bd & mask)};
}

FMT_HD inline auto umul128_upper64(uint64_t x, uint64_t y) noexcept
    -> uint64_t {
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

FMT_HD inline auto umul96_lower64(uint32_t x, uint64_t y) noexcept -> uint64_t {
  return x * y;
}

FMT_HD inline auto rotr(uint32_t n, uint32_t r) noexcept -> uint32_t {
  r &= 31;
  return (n >> r) | (n << (32 - r));
}

FMT_HD inline auto rotr(uint64_t n, uint32_t r) noexcept -> uint64_t {
  r &= 63;
  return (n >> r) | (n << (64 - r));
}

FMT_HD inline auto floor_log10_pow2(int e) noexcept -> int {
  return (e * 315653) >> 20;
}

FMT_HD inline auto floor_log2_pow10(int e) noexcept -> int {
  return (e * 1741647) >> 19;
}

FMT_HD inline auto floor_log10_pow2_minus_log10_4_over_3(int e) noexcept
    -> int {
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

template <typename Float> FMT_HD constexpr auto num_significand_bits() -> int {
  return std::numeric_limits<Float>::digits - 1;
}

template <typename Float>
FMT_HD constexpr auto exponent_mask() ->
    typename float_info<Float>::carrier_uint {
  using uint = typename float_info<Float>::carrier_uint;
  return ((uint(1) << float_info<Float>::exponent_bits) - 1)
         << num_significand_bits<Float>();
}

template <typename Float> FMT_HD constexpr auto exponent_bias() -> int {
  return std::numeric_limits<Float>::max_exponent - 1;
}

struct div_info {
  uint32_t divisor;
  int shift;
};
static constexpr div_info div_infos[] = {{10, 16}, {100, 16}};

template <int N>
FMT_HD auto check_divisibility_and_divide_by_pow10(uint32_t &n) noexcept
    -> bool {
  constexpr auto info = div_infos[N - 1];
  constexpr uint32_t magic = (1u << info.shift) / info.divisor + 1;
  n *= magic;
  const uint32_t mask = (1u << info.shift) - 1;
  bool result = (n & mask) < magic;
  n >>= info.shift;
  return result;
}

FMT_HD inline auto divide_by_10_to_kappa_plus_1(uint32_t n) noexcept
    -> uint32_t {
  return static_cast<uint32_t>((static_cast<uint64_t>(n) * 1374389535) >> 37);
}

FMT_HD inline auto divide_by_10_to_kappa_plus_1(uint64_t n) noexcept
    -> uint64_t {
  return umul128_upper64(n, 2361183241434822607ull) >> 7;
}

template <typename T> struct cache_accessor;

FFMT_DEVICE_TABLE static constexpr uint64_t float_pow10_table[] = {
    0x81ceb32c4b43fcf5, 0xa2425ff75e14fc32, 0xcad2f7f5359a3b3f,
    0xfd87b5f28300ca0e, 0x9e74d1b791e07e49, 0xc612062576589ddb,
    0xf79687aed3eec552, 0x9abe14cd44753b53, 0xc16d9a0095928a28,
    0xf1c90080baf72cb2, 0x971da05074da7bef, 0xbce5086492111aeb,
    0xec1e4a7db69561a6, 0x9392ee8e921d5d08, 0xb877aa3236a4b44a,
    0xe69594bec44de15c, 0x901d7cf73ab0acda, 0xb424dc35095cd810,
    0xe12e13424bb40e14, 0x8cbccc096f5088cc, 0xafebff0bcb24aaff,
    0xdbe6fecebdedd5bf, 0x89705f4136b4a598, 0xabcc77118461cefd,
    0xd6bf94d5e57a42bd, 0x8637bd05af6c69b6, 0xa7c5ac471b478424,
    0xd1b71758e219652c, 0x83126e978d4fdf3c, 0xa3d70a3d70a3d70b,
    0xcccccccccccccccd, 0x8000000000000000, 0xa000000000000000,
    0xc800000000000000, 0xfa00000000000000, 0x9c40000000000000,
    0xc350000000000000, 0xf424000000000000, 0x9896800000000000,
    0xbebc200000000000, 0xee6b280000000000, 0x9502f90000000000,
    0xba43b74000000000, 0xe8d4a51000000000, 0x9184e72a00000000,
    0xb5e620f480000000, 0xe35fa931a0000000, 0x8e1bc9bf04000000,
    0xb1a2bc2ec5000000, 0xde0b6b3a76400000, 0x8ac7230489e80000,
    0xad78ebc5ac620000, 0xd8d726b7177a8000, 0x878678326eac9000,
    0xa968163f0a57b400, 0xd3c21bcecceda100, 0x84595161401484a0,
    0xa56fa5b99019a5c8, 0xcecb8f27f4200f3a, 0x813f3978f8940985,
    0xa18f07d736b90be6, 0xc9f2c9cd04674edf, 0xfc6f7c4045812297,
    0x9dc5ada82b70b59e, 0xc5371912364ce306, 0xf684df56c3e01bc7,
    0x9a130b963a6c115d, 0xc097ce7bc90715b4, 0xf0bdc21abb48db21,
    0x96769950b50d88f5, 0xbc143fa4e250eb32, 0xeb194f8e1ae525fe,
    0x92efd1b8d0cf37bf, 0xb7abc627050305ae, 0xe596b7b0c643c71a,
    0x8f7e32ce7bea5c70, 0xb35dbf821ae4f38c, 0xe0352f62a19e306f};

FFMT_DEVICE_TABLE static constexpr uint128 double_pow10_significands[] = {
    {0xff77b1fcbebcdc4f, 0x25e8e89c13bb0f7b},
    {0xce5d73ff402d98e3, 0xfb0a3d212dc81290},
    {0xa6b34ad8c9dfc06f, 0xf42faa48c0ea481f},
    {0x86a8d39ef77164bc, 0xae5dff9c02033198},
    {0xd98ddaee19068c76, 0x3badd624dd9b0958},
    {0xafbd2350644eeacf, 0xe5d1929ef90898fb},
    {0x8df5efabc5979c8f, 0xca8d3ffa1ef463c2},
    {0xe55990879ddcaabd, 0xcc420a6a101d0516},
    {0xb94470938fa89bce, 0xf808e40e8d5b3e6a},
    {0x95a8637627989aad, 0xdde7001379a44aa9},
    {0xf1c90080baf72cb1, 0x5324c68b12dd6339},
    {0xc350000000000000, 0x0000000000000000},
    {0x9dc5ada82b70b59d, 0xf020000000000000},
    {0xfee50b7025c36a08, 0x02f236d04753d5b5},
    {0xcde6fd5e09abcf26, 0xed4c0226b55e6f87},
    {0xa6539930bf6bff45, 0x84db8346b786151d},
    {0x865b86925b9bc5c2, 0x0b8a2392ba45a9b3},
    {0xd910f7ff28069da4, 0x1b2ba1518094da05},
    {0xaf58416654a6babb, 0x387ac8d1970027b3},
    {0x8da471a9de737e24, 0x5ceaecfed289e5d3},
    {0xe4d5e82392a40515, 0x0fabaf3feaa5334b},
    {0xb8da1662e7b00a17, 0x3d6a751f3b936244},
    {0x95527a5202df0ccb, 0x0f37801e0c43ebc9},
    {0xf13e34aabb430a15, 0x647726b9e7c68ff0},
};

FFMT_DEVICE_TABLE static constexpr uint64_t double_powers_of_5_64[] = {
    0x0000000000000001, 0x0000000000000005, 0x0000000000000019,
    0x000000000000007d, 0x0000000000000271, 0x0000000000000c35,
    0x0000000000003d09, 0x000000000001312d, 0x000000000005f5e1,
    0x00000000001dcd65, 0x00000000009502f9, 0x0000000002e90edd,
    0x000000000e8d4a51, 0x0000000048c27395, 0x000000016bcc41e9,
    0x000000071afd498d, 0x0000002386f26fc1, 0x000000b1a2bc2ec5,
    0x000003782dace9d9, 0x00001158e460913d, 0x000056bc75e2d631,
    0x0001b1ae4d6e2ef5, 0x000878678326eac9, 0x002a5a058fc295ed,
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
                                 const cache_entry_type &cache) noexcept
      -> compute_mul_result {
    auto r = umul96_upper64(u, cache);
    return {static_cast<carrier_uint>(r >> 32),
            static_cast<carrier_uint>(r) == 0};
  }

  static FMT_HD auto compute_delta(const cache_entry_type &cache,
                                   int beta) noexcept -> uint32_t {
    return static_cast<uint32_t>(cache >> (64 - 1 - beta));
  }

  static FMT_HD auto compute_mul_parity(carrier_uint two_f,
                                        const cache_entry_type &cache,
                                        int beta) noexcept
      -> compute_mul_parity_result {
    auto r = umul96_lower64(two_f, cache);
    return {((r >> (64 - beta)) & 1) != 0,
            static_cast<uint32_t>(r >> (32 - beta)) == 0};
  }

  static FMT_HD auto compute_left_endpoint_for_shorter_interval_case(
      const cache_entry_type &cache, int beta) noexcept -> carrier_uint {
    return static_cast<carrier_uint>(
        (cache - (cache >> (num_significand_bits<float>() + 2))) >>
        (64 - num_significand_bits<float>() - 1 - beta));
  }

  static FMT_HD auto compute_right_endpoint_for_shorter_interval_case(
      const cache_entry_type &cache, int beta) noexcept -> carrier_uint {
    return static_cast<carrier_uint>(
        (cache + (cache >> (num_significand_bits<float>() + 1))) >>
        (64 - num_significand_bits<float>() - 1 - beta));
  }

  static FMT_HD auto compute_round_up_for_shorter_interval_case(
      const cache_entry_type &cache, int beta) noexcept -> carrier_uint {
    return (static_cast<carrier_uint>(
                cache >> (64 - num_significand_bits<float>() - 2 - beta)) +
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
                                 const cache_entry_type &cache) noexcept
      -> compute_mul_result {
    auto r = umul192_upper128(u, cache);
    return {r.high(), r.low() == 0};
  }

  static FMT_HD auto compute_delta(const cache_entry_type &cache,
                                   int beta) noexcept -> uint32_t {
    return static_cast<uint32_t>(cache.high() >> (64 - 1 - beta));
  }

  static FMT_HD auto compute_mul_parity(carrier_uint two_f,
                                        const cache_entry_type &cache,
                                        int beta) noexcept
      -> compute_mul_parity_result {
    auto r = umul192_lower128(two_f, cache);
    return {((r.high() >> (64 - beta)) & 1) != 0,
            ((r.high() << beta) | (r.low() >> (64 - beta))) == 0};
  }

  static FMT_HD auto compute_left_endpoint_for_shorter_interval_case(
      const cache_entry_type &cache, int beta) noexcept -> carrier_uint {
    return (cache.high() -
            (cache.high() >> (num_significand_bits<double>() + 2))) >>
           (64 - num_significand_bits<double>() - 1 - beta);
  }

  static FMT_HD auto compute_right_endpoint_for_shorter_interval_case(
      const cache_entry_type &cache, int beta) noexcept -> carrier_uint {
    return (cache.high() +
            (cache.high() >> (num_significand_bits<double>() + 1))) >>
           (64 - num_significand_bits<double>() - 1 - beta);
  }

  static FMT_HD auto compute_round_up_for_shorter_interval_case(
      const cache_entry_type &cache, int beta) noexcept -> carrier_uint {
    return ((cache.high() >> (64 - num_significand_bits<double>() - 2 - beta)) +
            1) /
           2;
  }
};

FMT_HD inline auto remove_trailing_zeros(uint32_t &n, int s = 0) noexcept
    -> int {
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
FMT_HD auto is_left_endpoint_integer_shorter_interval(int exponent) noexcept
    -> bool {
  return exponent >= 2 && exponent <= 3;
}

template <typename T>
FMT_HD inline auto shorter_interval_case(int exponent) noexcept
    -> decimal_fp<T> {
  decimal_fp<T> ret;
  const int minus_k = floor_log10_pow2_minus_log10_4_over_3(exponent);
  const int beta = exponent + floor_log2_pow10(-minus_k);

  using cache_entry_type = typename cache_accessor<T>::cache_entry_type;
  const cache_entry_type cache = cache_accessor<T>::get_cached_power(-minus_k);

  auto xi = cache_accessor<T>::compute_left_endpoint_for_shorter_interval_case(
      cache, beta);
  auto zi = cache_accessor<T>::compute_right_endpoint_for_shorter_interval_case(
      cache, beta);

  if (!is_left_endpoint_integer_shorter_interval<T>(exponent))
    ++xi;

  ret.significand = zi / 10;
  if (ret.significand * 10 >= xi) {
    ret.exponent = minus_k + 1;
    ret.exponent += remove_trailing_zeros(ret.significand);
    return ret;
  }

  ret.significand =
      cache_accessor<T>::compute_round_up_for_shorter_interval_case(cache,
                                                                    beta);
  ret.exponent = minus_k;

  if (exponent >= float_info<T>::shorter_interval_tie_lower_threshold &&
      exponent <= float_info<T>::shorter_interval_tie_upper_threshold) {
    ret.significand =
        ret.significand % 2 == 0 ? ret.significand : ret.significand - 1;
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
  int exponent =
      static_cast<int>((br & exponent_mask<T>()) >> num_significand_bits<T>());

  if (exponent != 0) {
    exponent -= exponent_bias<T>() + num_significand_bits<T>();
    if (significand == 0)
      return shorter_interval_case<T>(exponent);
    significand |= (static_cast<carrier_uint>(1) << num_significand_bits<T>());
  } else {
    if (significand == 0)
      return {0, 0};
    exponent =
        std::numeric_limits<T>::min_exponent - num_significand_bits<T>() - 1;
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
  uint32_t r = static_cast<uint32_t>(z_mul.result - float_info<T>::big_divisor *
                                                        ret.significand);

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
  const bool approx_y_parity =
      ((dist ^ (float_info<T>::small_divisor / 2)) & 1) != 0;

  const bool divisible =
      check_divisibility_and_divide_by_pow10<float_info<T>::kappa>(dist);
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
  for (int i = 0; i < n; i++)
    p *= 10;
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

FMT_HD inline auto write_digits(char *buf, uint64_t n, int num_digits)
    -> char * {
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
template <typename T> FMT_HD constexpr auto exp_upper() -> int {
  return std::numeric_limits<T>::digits10 + 1 < 16
             ? std::numeric_limits<T>::digits10 + 1
             : 16;
}

template <typename T>
FMT_HD inline auto format_shortest(char *buf, T value) -> int {
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
}
#ifdef _OPENMP
#pragma omp end declare target
#endif
