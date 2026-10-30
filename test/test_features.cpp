// Host-only standard-library adapters use the original stdout/oracle harness.
#ifndef TEST_INC
#define TEST_NAME features
#define TEST_INC "test_features.cpp"
#include "capture.hpp"
#include <array>
#include <chrono>
#include <codecvt>
#include <complex>
#include <fmt/chrono.h>
#include <fmt/color.h>
#include <fmt/printf.h>
#include <fmt/ranges.h>
#include <fmt/std.h>
#include <fmt/xchar.h>
#include <locale>
#include <optional>
#include <tuple>

struct family_padding_value { int mutations = 0; };
struct family_padding_range {
  family_padding_value values[2];
  family_padding_value *begin() { return values; }
  family_padding_value *end() { return values + 2; }
};
template <>
struct fmt::formatter<family_padding_value> : fmt::formatter<int> {
  template <class Context>
  auto format(family_padding_value &value, Context &ctx) const
      -> decltype(ctx.out()) {
    return fmt::formatter<int>::format(++value.mutations, ctx);
  }
};
template <>
struct ffmt::formatter<family_padding_value> : ffmt::formatter<int> {
  template <class Context>
  auto format(family_padding_value &value, Context &ctx) const
      -> decltype(ctx.out()) {
    return ffmt::formatter<int>::format(++value.mutations, ctx);
  }
};
struct stream_sizes {
  std::size_t written, required;
  int error;
};
static stream_sizes oracle_stream_padding(family_padding_range &value) {
  auto output = fmt::format("{:*<20:03}", value);
  std::cout << output;
  return {output.size(), output.size(), 0};
}
static stream_sizes target_stream_padding(family_padding_range &value) {
  auto result = ffmt::try_print("{:*<20:03}", value);
  return {result.written, result.required, int(result.error)};
}
static std::string wide_utf8(const std::wstring &text) {
  std::wstring_convert<std::codecvt_utf8<wchar_t>> converter;
  return converter.to_bytes(text);
}
#include "test_body_host.inc"
#else

#ifdef FFMT_STD_PATH
#define TEST_FORMAT fmt
#define TEST_WRITTEN(result, begin) static_cast<std::size_t>((result).out - (begin))
#define TEST_REQUIRED(result) (result).size
#define TEST_ERROR(result) 0
#define TEST_STREAM_PADDING(value) oracle_stream_padding(value)
#else
#define TEST_FORMAT ffmt
#define TEST_WRITTEN(result, begin) (result).written
#define TEST_REQUIRED(result) (result).required
#define TEST_ERROR(result) int((result).error)
#define TEST_STREAM_PADDING(value) target_stream_padding(value)
#endif

// chrono-duration
RUN(PRINT("{:%T}", std::chrono::milliseconds{-123456}));
// chrono-calendar: fmt's calendar wrapper predates the standard type.
#ifdef FFMT_STD_PATH
RUN(PRINT("{:%G-W%V-%u}",
          fmt::year_month_day{fmt::year{2021}, fmt::month{1}, fmt::day{1}}));
#else
RUN(PRINT("{:%G-W%V-%u}", std::chrono::year{2021} / 1 / 1));
#endif

// runtime-format
RUN(PRINT(TEST_FORMAT::runtime("{:04x}"), 42));
// wide-stdout: the host oracle's wide result is compared as UTF-8 bytes.
#ifdef FFMT_STD_PATH
RUN(std::cout << wide_utf8(fmt::format(L"[{:>8}] {:.3f}", L"静夜", 1.25)));
#else
RUN(ffmt::print(L"[{:>8}] {:.3f}", L"静夜", 1.25));
#endif

// named-fields
RUN(PRINT("{value:04x}", TEST_FORMAT::arg("value", 42)));
// printf-style
#ifdef FFMT_STD_PATH
RUN(std::cout << fmt::sprintf("%#08x %.3f", 42u, 1.25));
#else
RUN((void)ffmt::try_printf("%#08x %.3f", 42u, 1.25));
#endif

// range-nested-spec
RUN(PRINT("{::03}", std::array<int, 3>{1, -2, 33}));
// tuple
RUN(PRINT("{}", std::tuple<int, bool, const char *>{42, true, "a"}));
// join
RUN(PRINT("{:02x}", TEST_FORMAT::join(std::array<int, 3>{1, 2, 33}, ",")));

// bounded-family-padding: compare bytes, mutation counts, and required size.
RUN({
  family_padding_range value;
  char stored[16];
  auto result = TEST_FORMAT::format_to_n(stored, sizeof(stored),
                                       "{:*>20:03}", value);
  auto written = TEST_WRITTEN(result, stored);
  auto required = TEST_REQUIRED(result);
  int error = TEST_ERROR(result);
  PRINT("buffer=<{}>|mutations={},{}|written={}|required={}|error={}",
        std::string_view(stored, written), value.values[0].mutations,
        value.values[1].mutations, written, required, error);
});

// stream-left-family-padding: streaming must not measure by consuming twice.
RUN({
  family_padding_range value;
  auto result = TEST_STREAM_PADDING(value);
  auto written = result.written;
  auto required = result.required;
  int error = result.error;
  PRINT("|mutations={},{}|written={}|required={}|error={}",
        value.values[0].mutations, value.values[1].mutations, written,
        required, error);
});

// optional
RUN(PRINT("{}", std::optional<int>{42}));
// complex
RUN(PRINT("{:.3f}", std::complex<double>{1, -2.2}));
// styled
RUN(PRINT("{}", TEST_FORMAT::styled(
                    42, TEST_FORMAT::fg(TEST_FORMAT::rgb(255, 20, 30)) |
                            TEST_FORMAT::emphasis::bold)));

#undef TEST_FORMAT
#undef TEST_WRITTEN
#undef TEST_REQUIRED
#undef TEST_ERROR
#undef TEST_STREAM_PADDING
#endif
