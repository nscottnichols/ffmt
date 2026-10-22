// A binary-counted transport fixture independent of libc stdout buffering.
#include <cstddef>
#include <string>
#include <cassert>
static std::string bytes;
static std::size_t chunks;
static void capture_chunk(const char *p, std::size_t n) {
  bytes.append(p, n);
  ++chunks;
}
#define FFMT_EMIT_CHUNK(p, n) capture_chunk(p, n)
#include <ffmt/base.hpp>
int main() {
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
