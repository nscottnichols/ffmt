"""Handwritten ranges fixtures introduced with their formatter implementation."""
from case_support import case

_PADDING_PROBE_TYPES = r"""
struct family_padding_value {int mutations;};
struct family_padding_range {
  family_padding_value values[2];
  HD family_padding_value* begin() {return values;}
  HD family_padding_value* end() {return values + 2;}
};
"""

_PADDING_ORACLE_PREAMBLE = _PADDING_PROBE_TYPES + r"""
template<> struct fmt::formatter<family_padding_value> : fmt::formatter<int> {
  template<class Context>
  HD auto format(family_padding_value& value, Context& ctx) const -> decltype(ctx.out()) {
    return fmt::formatter<int>::format(++value.mutations, ctx);
  }
};
static std::string oracle_bounded_family_padding() {
  family_padding_range value{{{0}, {0}}};
  char stored[16];
  auto result = fmt::format_to_n(stored, sizeof(stored), "{:*>20:03}", value);
  auto written = static_cast<std::size_t>(result.out - stored);
  return fmt::format("buffer=<{}>|mutations={},{}|written={}|required={}|error={}",
                     std::string_view(stored, written), value.values[0].mutations,
                     value.values[1].mutations, written, result.size, 0);
}
static std::string oracle_stream_left_family_padding() {
  family_padding_range value{{{0}, {0}}};
  auto result = fmt::format("{:*<20:03}", value);
  return result + fmt::format("|mutations={},{}|written={}|required={}|error={}",
                              value.values[0].mutations, value.values[1].mutations,
                              result.size(), result.size(), 0);
}
"""

_PADDING_TARGET_PREAMBLE = _PADDING_PROBE_TYPES + r"""
template<> struct ffmt::formatter<family_padding_value> : ffmt::formatter<int> {
  template<class Context>
  HD auto format(family_padding_value& value, Context& ctx) const -> decltype(ctx.out()) {
    return ffmt::formatter<int>::format(++value.mutations, ctx);
  }
};
HD void bounded_family_padding() {
  family_padding_range value{{{0}, {0}}};
  char stored[16];
  auto result = ffmt::format_to_n(stored, sizeof(stored), "{:*>20:03}", value);
  ffmt::print("buffer=<{}>|mutations={},{}|written={}|required={}|error={}",
              ffmt::string_view(stored, result.written), value.values[0].mutations,
              value.values[1].mutations, result.written, result.required, int(result.error));
}
HD void stream_left_family_padding() {
  family_padding_range value{{{0}, {0}}};
  auto result = ffmt::try_print("{:*<20:03}", value);
  ffmt::print("|mutations={},{}|written={}|required={}|error={}",
              value.values[0].mutations, value.values[1].mutations,
              result.written, result.required, int(result.error));
}
"""

CASES = [
    case("range-nested-spec", "range", "", oracle_expr='fmt::format("{::03}", std::array<int,3>{1,-2,33})',
         ffmt_expr='ffmt::print("{::03}", std::array<int,3>{1,-2,33})', oracle_includes=["fmt/ranges.h", "array"],
         includes=["ffmt/ranges.hpp", "array"], requires=["ffmt/ranges.hpp"]),
    case("tuple", "tuple", "", oracle_expr='fmt::format("{}", std::tuple{42, true, "a"})',
         ffmt_expr='ffmt::print("{}", std::tuple<int, bool, const char*>{42, true, "a"})', oracle_includes=["fmt/ranges.h", "tuple"],
         includes=["ffmt/ranges.hpp", "tuple"], requires=["ffmt/ranges.hpp"]),
    case("join", "range", "", oracle_expr='fmt::format("{:02x}", fmt::join(std::array<int,3>{1,2,33}, ","))',
         ffmt_expr='ffmt::print("{:02x}", ffmt::join(std::array<int,3>{1,2,33}, ","))', oracle_includes=["fmt/ranges.h", "array"],
         includes=["ffmt/ranges.hpp", "array"], requires=["ffmt/ranges.hpp"]),
    case("bounded-family-padding", "range", "", oracle_expr="oracle_bounded_family_padding()",
         ffmt_expr="bounded_family_padding()", oracle_includes=["fmt/ranges.h", "string", "string_view"],
         oracle_preamble=_PADDING_ORACLE_PREAMBLE, includes=["ffmt/ranges.hpp"],
         preamble=_PADDING_TARGET_PREAMBLE, requires=["ffmt/ranges.hpp"]),
    case("stream-left-family-padding", "range", "", oracle_expr="oracle_stream_left_family_padding()",
         ffmt_expr="stream_left_family_padding()", oracle_includes=["fmt/ranges.h", "string", "string_view"],
         oracle_preamble=_PADDING_ORACLE_PREAMBLE, includes=["ffmt/ranges.hpp"],
         preamble=_PADDING_TARGET_PREAMBLE, requires=["ffmt/ranges.hpp"]),
]
