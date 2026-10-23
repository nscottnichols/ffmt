// Representation-aware floating point conversion using bounded integer limbs.
// Exact decimal interval comparisons provide a portable shortest conversion.
#pragma once
namespace ffmt::detail {
struct binary_parts {
  std::uint32_t words[4]{};
  int exponent = 0, precision = 0, min_exponent = 0;
  bool negative = false, finite = true, nan = false, supported = true;
  FMT_HD bool zero() const {
    return !(words[0] | words[1] | words[2] | words[3]);
  }
  FMT_HD void shift_left(unsigned amount) {
    std::uint32_t carry = 0;
    for (unsigned i = 0; i < 4; ++i) {
      auto next = words[i] >> (32 - amount);
      words[i] = (words[i] << amount) | carry;
      carry = next;
    }
  }
  FMT_HD void shift_right(unsigned amount) {
    std::uint32_t carry = 0;
    for (unsigned i = 4; i > 0; --i) {
      auto next = words[i - 1] << (32 - amount);
      words[i - 1] = (words[i - 1] >> amount) | carry;
      carry = next;
    }
  }
  FMT_HD void add_one() {
    for (unsigned i = 0; i < 4; ++i)
      if (++words[i])
        break;
  }
  FMT_HD void subtract_one() {
    for (unsigned i = 0; i < 4; ++i) {
      auto before = words[i]--;
      if (before)
        break;
    }
  }
  FMT_HD bool power_of_two() const {
    unsigned count = 0;
    for (auto word : words) {
      if (word && (word & (word - 1)))
        return false;
      count += word ? 1 : 0;
    }
    return count == 1;
  }
  FMT_HD unsigned nibble(unsigned index) const {
    return (words[index / 8] >> ((index % 8) * 4)) & 15;
  }
};
template <typename T> FMT_HD binary_parts split_binary(T value) {
  binary_parts p;
  if constexpr (std::is_same_v<T, float>) {
    auto b = __builtin_bit_cast(std::uint32_t, value);
    unsigned e = (b >> 23) & 255;
    p.words[0] = b & 0x7fffff;
    p.nan = e == 255 && p.words[0];
    p.finite = e != 255;
    p.negative = bool(b >> 31);
    if (e && p.finite)
      p.words[0] |= 1u << 23;
    p.precision = 24;
    p.min_exponent = -149;
    p.exponent = int(e ? e : 1) - 127 - 23;
  } else if constexpr (std::is_same_v<T, double>) {
    auto b = __builtin_bit_cast(std::uint64_t, value);
    unsigned e = unsigned((b >> 52) & 2047);
    auto m = b & 0xfffffffffffffull;
    p.nan = e == 2047 && m;
    p.finite = e != 2047;
    p.negative = bool(b >> 63);
    if (e && p.finite)
      m |= 1ull << 52;
    p.words[0] = std::uint32_t(m);
    p.words[1] = std::uint32_t(m >> 32);
    p.precision = 53;
    p.min_exponent = -1074;
    p.exponent = int(e ? e : 1) - 1023 - 52;
  } else
    p.supported = false;
  return p;
}

// Capacity counts decimal digits, not requested output precision. Callers
// provide enough storage for the complete exact finite magnitude and interval
// endpoints; the supported IEEE layouts have bounded exponent/significand sizes.
template <unsigned Capacity> struct precise_decimal {
  char data[Capacity];
  int size = 0, exponent = 0;
  FMT_HD explicit precise_decimal(binary_parts p) {
    if (p.zero()) {
      data[0] = '0';
      size = 1;
      return;
    }
    while (p.exponent < 0 && (p.words[0] & 1) == 0) {
      p.shift_right(1);
      ++p.exponent;
    }
    constexpr unsigned limbs = (Capacity + 8) / 9;
    std::uint32_t values[limbs]{};
    unsigned count = 1;
    auto multiply = [&](std::uint32_t factor, std::uint32_t add = 0) {
      std::uint64_t carry = add;
      for (unsigned i = 0; i < count; ++i) {
        std::uint64_t product = std::uint64_t(values[i]) * factor + carry;
        values[i] = std::uint32_t(product % 1000000000u);
        carry = product / 1000000000u;
      }
      while (carry) {
        values[count++] = std::uint32_t(carry % 1000000000u);
        carry /= 1000000000u;
      }
    };
    // Two 16-bit multiplications avoid an unrepresentable 32-bit factor2^32.
    for (unsigned i = 4; i > 0; --i) {
      multiply(65536);
      multiply(65536, p.words[i - 1]);
    }
    int decimal_scale = p.exponent < 0 ? p.exponent : 0;
    if (p.exponent >= 0) {
      for (int left = p.exponent; left > 0;) {
        unsigned amount = left > 29 ? 29 : unsigned(left);
        multiply(std::uint32_t(1) << amount);
        left -= int(amount);
      }
    } else {
      for (int left = -p.exponent; left > 0;) {
        unsigned amount = left > 12 ? 12 : unsigned(left);
        std::uint32_t factor = 1;
        for (unsigned i = 0; i < amount; ++i)
          factor *= 5;
        multiply(factor);
        left -= int(amount);
      }
    }
    std::uint32_t high = values[count - 1];
    char reverse[10];
    unsigned digits = 0;
    do {
      reverse[digits++] = char('0' + high % 10);
      high /= 10;
    } while (high);
    for (unsigned i = digits; i > 0; --i)
      data[size++] = reverse[i - 1];
    for (unsigned i = count - 1; i > 0; --i) {
      auto word = values[i - 1];
      for (unsigned j = 9; j > 0; --j) {
        data[size + int(j) - 1] = char('0' + word % 10);
        word /= 10;
      }
      size += 9;
    }
    exponent = size - 1 + decimal_scale;
    trim();
  }
  FMT_HD void trim() {
    while (size > 1 && data[size - 1] == '0')
      --size;
  }
  FMT_HD char at(long long i) const {
    return i >= 0 && i < size ? data[i] : '0';
  }
  FMT_HD void round(long long keep) {
    if (keep >= size)
      return;
    if (keep < 0) {
      data[0] = '0';
      size = 1;
      exponent = 0;
      return;
    }
    int n = int(keep);
    int last = n ? data[n - 1] - '0' : 0;
    bool up = data[n] > '5' || (data[n] == '5' && (size > n + 1 || (last & 1)));
    if (!n) {
      if (up) {
        data[0] = '1';
        size = 1;
        ++exponent;
      } else {
        data[0] = '0';
        size = 1;
        exponent = 0;
      }
      return;
    }
    size = n;
    if (!up)
      return;
    int i = size - 1;
    while (i >= 0 && data[i] == '9')
      data[i--] = '0';
    if (i >= 0)
      ++data[i];
    else {
      data[0] = '1';
      size = 1;
      ++exponent;
    }
  }
};
struct shortest_decimal {
  // Returned candidates are copied as complete objects, including unused digits.
  char data[40]{};
  int size = 0, exponent = 0;
  template <unsigned C>
  FMT_HD shortest_decimal(const precise_decimal<C> &value, int keep, bool up)
      : size(keep), exponent(value.exponent) {
    if (size > value.size)
      size = value.size;
    for (int i = 0; i < size; ++i)
      data[i] = value.data[i];
    if (up) {
      int i = size - 1;
      while (i >= 0 && data[i] == '9')
        data[i--] = '0';
      if (i >= 0)
        ++data[i];
      else {
        data[0] = '1';
        size = 1;
        ++exponent;
      }
    }
    while (size > 1 && data[size - 1] == '0')
      --size;
  }
  template <unsigned C>
  FMT_HD int compare(const precise_decimal<C> &other) const {
    if (exponent != other.exponent)
      return exponent < other.exponent ? -1 : 1;
    int n = size > other.size ? size : other.size;
    for (int i = 0; i < n; ++i) {
      char a = i < size ? data[i] : '0',
           b = i < other.size ? other.data[i] : '0';
      if (a != b)
        return a < b ? -1 : 1;
    }
    return 0;
  }
};
template <unsigned Capacity>
FMT_HD auto interval_shortest(binary_parts p) -> shortest_decimal {
  precise_decimal<Capacity> value(p);
  if (p.zero())
    return shortest_decimal(value, 1, false);
  bool inclusive = (p.words[0] & 1) == 0;
  auto lower = p, upper = p;
  unsigned shift = p.power_of_two() && p.exponent > p.min_exponent ? 2 : 1;
  lower.shift_left(shift);
  lower.subtract_one();
  lower.exponent -= int(shift);
  upper.shift_left(1);
  upper.add_one();
  --upper.exponent;
  precise_decimal<Capacity> low(lower), high(upper);
  auto inside = [&](const shortest_decimal &candidate) {
    int lo = candidate.compare(low), hi = candidate.compare(high);
    return inclusive ? lo >= 0 && hi <= 0 : lo > 0 && hi < 0;
  };
  int max_digits = p.precision * 30103 / 100000 + 2;
  for (int keep = 1; keep <= max_digits; ++keep) {
    bool has_tail = keep < value.size;
    bool up = has_tail &&
              (value.data[keep] > '5' ||
               (value.data[keep] == '5' &&
                (value.size > keep + 1 || ((value.data[keep - 1] - '0') & 1))));
    shortest_decimal closest(value, keep, up);
    if (inside(closest))
      return closest;
    if (has_tail) {
      shortest_decimal alternate(value, keep, !up);
      if (inside(alternate))
        return alternate;
    }
  }
  // IEEE round-trip guarantees a candidate by max_digits10.
  return shortest_decimal(value, max_digits, false);
}
FMT_HD inline int render_shortest(char *out, const shortest_decimal &d,
                                  int upper) {
  char *p = out;
  if (d.exponent >= -4 && d.exponent < upper) {
    if (d.exponent >= 0) {
      int integer = d.exponent + 1;
      for (int i = 0; i < integer; ++i)
        *p++ = i < d.size ? d.data[i] : '0';
      if (integer < d.size) {
        *p++ = '.';
        for (int i = integer; i < d.size; ++i)
          *p++ = d.data[i];
      }
    } else {
      *p++ = '0';
      *p++ = '.';
      for (int i = -d.exponent - 1; i > 0; --i)
        *p++ = '0';
      for (int i = 0; i < d.size; ++i)
        *p++ = d.data[i];
    }
  } else {
    *p++ = d.data[0];
    if (d.size > 1) {
      *p++ = '.';
      for (int i = 1; i < d.size; ++i)
        *p++ = d.data[i];
    }
    *p++ = 'e';
    int exp = d.exponent;
    *p++ = exp < 0 ? '-' : '+';
    unsigned n = exp < 0 ? unsigned(-exp) : unsigned(exp);
    char digits[8];
    unsigned size = 0;
    do {
      digits[size++] = char('0' + n % 10);
      n /= 10;
    } while (n);
    if (size < 2)
      *p++ = '0';
    for (unsigned i = size; i > 0; --i)
      *p++ = digits[i - 1];
  }
  return int(p - out);
}
// Internal precondition: value has a supported finite representation. This
// renders the magnitude only; callers classify nonfinite values and emit signs.
// The output buffer must hold a shortest representation (64 bytes suffices for
// all supported formats). Decimal capacities conservatively cover exact binary64
// expansions and endpoints (1100), and native80/binary128 endpoints (16520).
// For binary128 the worst negative exponent is -16494. Midpoint endpoints
// need at most 115 significand bits and powers of five through 5^16496,
// requiring fewer than 11570 decimal digits. These bounds do not limit requested precision.
template <typename T> FMT_HD int portable_shortest(char *out, T value) {
  auto parts = split_binary(value);
  constexpr unsigned capacity =
      floating_traits<T>::precision > 53 ? 16520 : 1100;
  auto decimal = interval_shortest<capacity>(parts);
  int upper = floating_traits<T>::decimal_digits + 1;
  return render_shortest(out, decimal, upper < 16 ? upper : 16);
}
} // namespace ffmt::detail
