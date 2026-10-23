#pragma once
#ifdef _OPENMP
#pragma omp declare target
#endif
namespace ffmt::detail {
struct exact_decimal {
  static constexpr int LIMBS = 35;
  static constexpr uint32_t BASE = 1000000000u;
  uint32_t a[LIMBS];
  int n = 0;          // limbs in use
  int k = 0;          // fraction width in bits; 0 when the value is an integer
  uint64_t ipart = 0; // integer part when k > 0
  int int_digits = 0;
  int top_digits = 0; // decimal digits in a[n-1] when k == 0
  int pos = 0;        // digits consumed by next()

  FMT_HD explicit exact_decimal(double v) {
    uint64_t bits = __builtin_bit_cast(uint64_t, v);
    int be = static_cast<int>((bits >> 52) & 0x7FF);
    uint64_t m = bits & 0x000FFFFFFFFFFFFFULL;
    int e;
    if (be == 0)
      e = -1074;
    else {
      m |= 1ULL << 52;
      e = be - 1075;
    }
    if (e >= 0) {
      for (uint64_t t = m; t; t /= BASE)
        a[n++] = static_cast<uint32_t>(t % BASE);
      while (e > 0) { // multiply by 2^e, at most 2^32 per pass
        int s = e > 32 ? 32 : e;
        e -= s;
        uint64_t carry = 0;
        for (int i = 0; i < n; i++) {
          uint64_t x = (static_cast<uint64_t>(a[i]) << s) + carry;
          a[i] = static_cast<uint32_t>(x % BASE);
          carry = x / BASE;
        }
        while (carry) {
          a[n++] = static_cast<uint32_t>(carry % BASE);
          carry /= BASE;
        }
      }
      top_digits = dragonbox::count_digits(a[n - 1]);
      int_digits = 9 * (n - 1) + top_digits;
    } else {
      k = -e;
      uint64_t r = m;
      if (k < 64) {
        ipart = m >> k;
        r = m & ((1ULL << k) - 1);
      }
      n = (k + 4 + 31) / 32;
      for (int i = 0; i < n; i++)
        a[i] = 0;
      a[0] = static_cast<uint32_t>(r);
      if (n > 1)
        a[1] = static_cast<uint32_t>(r >> 32);
      int_digits = ipart ? dragonbox::count_digits(ipart) : 0;
    }
  }

  // j-th integer digit from the top (k == 0 only).
  FMT_HD int int_digit(int j) const {
    if (j < top_digits)
      return static_cast<int>(a[n - 1] /
                              dragonbox::pow10_u64(top_digits - 1 - j) % 10);
    j -= top_digits;
    return static_cast<int>(a[n - 2 - j / 9] / dragonbox::pow10_u64(8 - j % 9) %
                            10);
  }

  FMT_HD int next() {
    int j = pos++;
    if (j < int_digits) {
      if (k == 0)
        return int_digit(j);
      return static_cast<int>(ipart / dragonbox::pow10_u64(int_digits - 1 - j) %
                              10);
    }
    if (k == 0)
      return 0;
    uint64_t carry = 0;
    for (int i = 0; i < n; i++) {
      uint64_t x = static_cast<uint64_t>(a[i]) * 10 + carry;
      a[i] = static_cast<uint32_t>(x);
      carry = x >> 32;
    }
    // The digit is bits [k, k+4); clear them to keep r < 2^k.
    int li = k / 32, sh = k % 32;
    uint64_t w = a[li];
    if (li + 1 < n)
      w |= static_cast<uint64_t>(a[li + 1]) << 32;
    a[li] &= (1u << sh) - 1;
    if (li + 1 < n)
      a[li + 1] = 0;
    return static_cast<int>((w >> sh) & 0xF);
  }

  // Is any digit at or after the current position nonzero?
  FMT_HD bool rest_nonzero() const {
    if (k == 0) {
      for (int j = pos; j < int_digits; j++)
        if (int_digit(j))
          return true;
      return false;
    }
    if (pos < int_digits && ipart % dragonbox::pow10_u64(int_digits - pos) != 0)
      return true;
    for (int i = 0; i < n; i++)
      if (a[i])
        return true;
    return false;
  }
};

}
#ifdef _OPENMP
#pragma omp end declare target
#endif
