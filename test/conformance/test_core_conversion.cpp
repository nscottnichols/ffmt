#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>
#include <cstdint>
#include <ffmt/detail/checked.hpp>
#include <fmt/format.h>
#include <limits>
#include <string>

template <typename T> void shortest_matches_oracle(T value) {
  const auto parts = ffmt::detail::split_binary(value);
  assert(parts.supported && parts.finite && !parts.negative);
  char output[64];
  const auto size = ffmt::detail::portable_shortest(output, value);
  assert(std::string(output, size) == fmt::format("{}", value));
}
void binary64_boundaries() {
  // Adjacent representations exercise asymmetric intervals at powers of two,
  // the largest subnormal, the first normals, and the largest finite values.
  constexpr std::uint64_t bits[] = {0,
                                    1,
                                    2,
                                    3,
                                    0x000fffffffffffffull,
                                    0x0010000000000000ull,
                                    0x0010000000000001ull,
                                    0x3fdfffffffffffffull,
                                    0x3fe0000000000000ull,
                                    0x3fe0000000000001ull,
                                    0x3fefffffffffffffull,
                                    0x3ff0000000000000ull,
                                    0x3ff0000000000001ull,
                                    0x3fffffffffffffffull,
                                    0x4000000000000000ull,
                                    0x4000000000000001ull,
                                    0x433fffffffffffffull,
                                    0x4340000000000000ull,
                                    0x4340000000000001ull,
                                    0x7feffffffffffffeull,
                                    0x7fefffffffffffffull};
  for (auto representation : bits)
    shortest_matches_oracle(__builtin_bit_cast(double, representation));
  for (double value : {9.999999999999998, 10.0, 10.000000000000002, 0.0001,
                       0.00001, 1e15, 1e16})
    shortest_matches_oracle(value);
}
void binary32_boundaries() {
  constexpr std::uint32_t bits[] = {
      0,           1,           2,           3,           0x007fffffu,
      0x00800000u, 0x00800001u, 0x3effffffu, 0x3f000000u, 0x3f000001u,
      0x3f7fffffu, 0x3f800000u, 0x3f800001u, 0x3fffffffu, 0x40000000u,
      0x40000001u, 0x4b7fffffu, 0x4b800000u, 0x4b800001u, 0x7f7ffffeu,
      0x7f7fffffu};
  for (auto representation : bits)
    shortest_matches_oracle(__builtin_bit_cast(float, representation));
  for (float value : {0.0001f, 0.00001f, 1e6f, 1e7f})
    shortest_matches_oracle(value);
}
void decimal_rounding_carries_and_ties() {
  struct rounding_case {
    double input;
    const char *digits;
    int exponent;
  };
  constexpr rounding_case cases[] = {
      {9.5, "1", 1}, {2.5, "2", 0}, {3.5, "4", 0}};
  for (const auto &sample : cases) {
    ffmt::detail::precise_decimal<1100> value(
        ffmt::detail::split_binary(sample.input));
    value.round(1);
    assert(std::string(value.data, value.size) == sample.digits &&
           value.exponent == sample.exponent);
  }
  ffmt::detail::precise_decimal<1100> exact(ffmt::detail::split_binary(0.125));
  assert(std::string(exact.data, exact.size) == "125" && exact.exponent == -1);
}
void classification_is_separate_from_magnitude_rendering() {
  const auto zero = ffmt::detail::split_binary(-0.0);
  assert(zero.zero() && zero.negative && zero.finite);
  const auto minimum =
      ffmt::detail::split_binary(std::numeric_limits<double>::denorm_min());
  assert(minimum.words[0] == 1 && minimum.exponent == -1074);
  const auto infinity =
      ffmt::detail::split_binary(std::numeric_limits<double>::infinity());
  const auto nan =
      ffmt::detail::split_binary(std::numeric_limits<double>::quiet_NaN());
  assert(!infinity.finite && !infinity.nan && !nan.finite && nan.nan);
}
int main() {
  binary64_boundaries();
  binary32_boundaries();
  decimal_rounding_carries_and_ties();
  classification_is_separate_from_magnitude_rendering();
}
