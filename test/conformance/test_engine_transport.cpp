// A binary-counted transport fixture independent of libc stdout buffering.
#include <cassert>
#include <cstddef>
#include <string>
static std::string bytes;
static std::size_t chunks;
static void capture_chunk(const char *p, std::size_t n) {
  bytes.append(p, n);
  ++chunks;
}
#define FFMT_EMIT_CHUNK(p, n) capture_chunk(p, n)
#include <ffmt/detail/checked.hpp>
#include <fmt/format.h>
#include <fmt/xchar.h>

// Template parameter object addresses exposed a GCC ASan constexpr defect in
// both the outer walker and a standalone formatter parse context.
template <ffmt::detail::fixed_string Text> consteval bool parse_fixed_specs() {
  using Char = std::remove_cvref_t<decltype(Text[0])>;
  ffmt::basic_format_parse_context<Char> ctx{
      {Text.data, ffmt::detail::flen(Text)}};
  ffmt::formatter<int, Char> formatter;
  auto end = formatter.parse(ctx);
  return ctx.error() == ffmt::format_error::none && end == ctx.end() - 1;
}
static_assert(parse_fixed_specs<"03}">());
static_assert(parse_fixed_specs<L"03}">());
static_assert([] {
  ffmt::format_parse_context ctx{ffmt::string_view{}};
  ffmt::wformat_parse_context wide{ffmt::wstring_view{}};
  return ctx.begin() == ctx.end() && wide.begin() == wide.end() &&
         ctx.error() == ffmt::format_error::none &&
         wide.error() == ffmt::format_error::none;
}());

int main() {
  const auto empty_oracle = fmt::format(fmt::runtime(fmt::string_view{}));
  const auto wide_empty_oracle = fmt::format(fmt::runtime(fmt::wstring_view{}));
  assert(empty_oracle.empty() && wide_empty_oracle.empty());
  auto empty = ffmt::try_print(ffmt::runtime(ffmt::string_view{}));
  auto wide_empty = ffmt::try_print(ffmt::runtime(ffmt::wstring_view{}));
  auto empty_literal = ffmt::try_print<"">();
  auto wide_empty_literal = ffmt::try_print<L"">();
  assert(empty && wide_empty && empty_literal && wide_empty_literal &&
         empty.written == 0 && empty.required == 0 && wide_empty.written == 0 &&
         wide_empty.required == 0 && empty_literal.written == 0 &&
         empty_literal.required == 0 && wide_empty_literal.written == 0 &&
         wide_empty_literal.required == 0 && bytes.empty() && chunks == 0);
  // Null nonempty views are invalid input. This defensive status is ffmt's
  // checked API behavior; the oracle is only called with valid empty views.
  auto invalid = ffmt::try_print(ffmt::runtime(ffmt::string_view{nullptr, 1}));
  auto wide_invalid =
      ffmt::try_print(ffmt::runtime(ffmt::wstring_view{nullptr, 1}));
  assert(invalid.error == ffmt::format_error::invalid_format &&
         wide_invalid.error == ffmt::format_error::invalid_format &&
         invalid.written == 0 && wide_invalid.written == 0 && bytes.empty());
  ffmt::format_parse_context invalid_ctx{ffmt::string_view{nullptr, 1}};
  ffmt::wformat_parse_context wide_invalid_ctx{ffmt::wstring_view{nullptr, 1}};
  assert(invalid_ctx.error() == ffmt::format_error::invalid_format &&
         wide_invalid_ctx.error() == ffmt::format_error::invalid_format &&
         invalid_ctx.begin() == invalid_ctx.end() &&
         wide_invalid_ctx.begin() == wide_invalid_ctx.end());
  auto result = ffmt::try_print("a{}{:300}{}", char(0), 42, "%%");
  assert(result && result.written == 304 && result.required == 304 &&
         bytes.size() == 304);
  assert(bytes[0] == 'a' && bytes[1] == 0 && bytes.substr(300) == "42%%");
  assert(chunks >= 304 / FFMT_BUFFER_SIZE);
  bytes.clear();
  chunks = 0;
  auto wide = ffmt::try_println(L"{} {}", L"静夜", L'𐐀');
  assert(wide && bytes == "静夜 𐐀\n" && wide.written == bytes.size() &&
         wide.required == bytes.size());
  bytes.clear();
  auto prefix = ffmt::try_print(ffmt::runtime("prefix {:q}"), 1);
  assert(prefix.error == ffmt::format_error::incompatible_type &&
         bytes == "prefix ");
  bytes.clear();
  auto bad = ffmt::try_print(L"{}", wchar_t(0xd800));
  assert(bad.error == ffmt::format_error::invalid_code_point);
  bytes.clear();
  ffmt::print<"{}\n">(7);
  assert(bytes == "7\n");
  bytes.clear();
  FFMT_PRINTF("{}:", 2);
  FFMT_PRINTLNF("{}", 3);
  assert(bytes == "2:3\n");
}
