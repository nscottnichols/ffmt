#ifdef NDEBUG
#undef NDEBUG
#endif
#include <array>
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <ffmt/printf.hpp>
#include <fmt/printf.h>
#include <fmt/xchar.h>
#include <limits>
#include <string>
#include <string_view>

namespace {
int comparisons = 0;
template <typename... Args>
void check(std::string_view pattern, const Args &...args) {
  auto expected =
      fmt::sprintf(fmt::string_view(pattern.data(), pattern.size()), args...);
  char output[4096];
  auto result = ffmt::printf_to_n(output, sizeof(output),
                                  ffmt::runtime(pattern), args...);
  if (result.error != ffmt::format_error::none ||
      std::string(output, result.written) != expected ||
      result.required != expected.size()) {
    std::fprintf(
        stderr,
        "printf mismatch format=%.*s error=%d actual=%.*s expected=%s\n",
        int(pattern.size()), pattern.data(), int(result.error),
        int(result.written), output, expected.c_str());
    std::abort();
  }
  for (std::size_t capacity : {std::size_t(0), std::size_t(1), std::size_t(3),
                               std::size_t(7), std::size_t(31)}) {
    std::array<char, 64> bounded;
    bounded.fill('!');
    auto truncated = ffmt::printf_to_n(bounded.data() + 1, capacity,
                                       ffmt::runtime(pattern), args...);
    assert(truncated.error == ffmt::format_error::none);
    assert(truncated.required == expected.size());
    assert(truncated.written == std::min(capacity, expected.size()));
    assert(std::string(bounded.data() + 1, truncated.written) ==
           expected.substr(0, capacity));
    assert(bounded[0] == '!' && bounded[capacity + 1] == '!');
    assert(truncated.out == bounded.data() + 1 + truncated.written);
    ++comparisons;
  }
  ++comparisons;
}
template <typename... Args>
void reject(std::string_view pattern, const Args &...args) {
  bool oracle_rejected = false;
  try {
    (void)fmt::sprintf(fmt::string_view(pattern.data(), pattern.size()),
                       args...);
  } catch (const fmt::format_error &) {
    oracle_rejected = true;
  }
  assert(oracle_rejected);
  char output[1024];
  auto result = ffmt::printf_to_n(output, sizeof(output),
                                  ffmt::runtime(pattern), args...);
  assert(result.error != ffmt::format_error::none);
  ++comparisons;
}
template <typename... Args>
void wide(std::wstring_view pattern, const Args &...args) {
  auto expected = fmt::vsprintf(
      fmt::basic_string_view<wchar_t>(pattern.data(), pattern.size()),
      fmt::make_printf_args<wchar_t>(args...));
  wchar_t output[4096];
  auto result = ffmt::printf_to_n(output, std::size(output),
                                  ffmt::runtime(pattern), args...);
  assert(result.error == ffmt::format_error::none);
  assert(std::wstring(output, result.written) == expected);
  ++comparisons;
}
} // namespace

namespace {
void literal_and_integer_conversions() {
  check("plain %% %s %c %c", "100%", 'x', char(0));
  for (const char *flag : {"", "+", " ", "#", "0", "-", "+#0", "-#"})
    for (const char *width : {"", "7", "20"})
      for (const char *precision : {"", ".0", ".1", ".5", ".20"})
        for (const char *type : {"d", "i", "u", "o", "x", "X"}) {
          auto pattern = std::string("%") + flag + width + precision + type;
          for (int value :
               {0, 1, -1, 42, -42, (std::numeric_limits<int>::min)(),
                (std::numeric_limits<int>::max)()})
            check(pattern, value);
        }
  for (const char *length : {"hh", "h", "l", "ll", "j", "z", "t"})
    for (const char *type : {"d", "i", "u", "o", "x", "X"}) {
      auto pattern = std::string("%#20.10") + length + type;
      check(pattern, -42);
      check(pattern, 256);
      check(pattern, (std::numeric_limits<unsigned long long>::max)());
      check(pattern, (std::numeric_limits<long long>::min)());
    }
  check("%d %u %d %d", true, false, 'x', static_cast<signed char>(-42));
}

void floating_conversions() {
  for (const char *flag : {"", "+", " ", "#", "0", "-", "+#0"})
    for (const char *type : {"f", "e", "g", "a", "F", "E", "G", "A"}) {
      auto pattern = std::string("%") + flag + "20.5" + type;
      for (double value : {0.0, -0.0, 1.125, -42.125, 1e100, 1e-100,
                           std::numeric_limits<double>::infinity(),
                           -std::numeric_limits<double>::infinity(),
                           std::numeric_limits<double>::quiet_NaN()})
        check(pattern, value);
      check(pattern, 0.1f);
    }
  check("%f %e %g %a", 1.25, 1.25, 1.25, 1.25);
}

void dynamic_and_positional_fields() {
  check("%*.*f %d", 12, 4, 1.25, 42);
  check("%*.*d %s", -12, 5, -42, "last");
  check("%*.*d", 7, -1, 0);
  check("%2$*1$d", 9, -42);
  check("%3$*2$.*1$f", 3, 12, 1.25);
  check("%2$s %1$d %2$s", 42, "reordered");
}

void strings_pointers_and_characters() {
  check("%.4s", "abcde");
  const char raw[4] = {'a', 'b', 'c', 'd'};
  check("%.4s", raw);
  check("%.4s", std::string("静夜思"));
  check("%.4s", "静夜思");
  check("%10s %10p", static_cast<const char *>(nullptr),
        static_cast<void *>(nullptr));
  int integer = 1;
  check("%p %p", static_cast<void *>(&integer), "string address");
  check("%05c %-5c", -1, 0xff);
}

void wide_conversions() {
  wide(L"%d %s %.3f %c", -42, L"wide", 1.25, L'文');
  wide(L"%#10.5x %p", 42, static_cast<void *>(nullptr));
}

void invalid_conversions_do_not_write() {
  int integer = 1;
  reject("%");
  reject("%q", 42);
  reject("%n", static_cast<void *>(&integer));
  reject("%d", 1.25);
  reject("%s", 42);
  reject("%f", 42);
  reject("%d %d", 1);
  reject("%1$d %d", 1, 2);
  reject("%0$d", 1);
  reject("%*d", 1.2, 42);
  reject("%.*d", 1.2, 42);
  reject("%1$*0$d", 1, 2);
  reject("%2147483648d", 1);
  assert(integer == 1);
}

} // namespace
int main() {
  literal_and_integer_conversions();
  floating_conversions();
  dynamic_and_positional_fields();
  strings_pointers_and_characters();
  wide_conversions();
  invalid_conversions_do_not_write();
  std::printf("printf: %d differential/bounded comparisons passed\n",
              comparisons);
}
