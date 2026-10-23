// Test-only differential helpers. Each suite includes only its own family
// headers.
#pragma once
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <array>
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <ffmt/base.hpp>
#include <fmt/format.h>
#include <fmt/xchar.h>
#include <string>
#include <string_view>

namespace ffmt_test {
// Specialize only where the oracle exposes a different wrapper type, such as
// fmt::year versus std::chrono::year. Other suites need no calendar dependency.
template <typename T> struct oracle_argument {
  static const T &get(const T &value) { return value; }
};
template <typename T> decltype(auto) oracle_arg(const T &value) {
  return oracle_argument<T>::get(value);
}

class differential_suite {
  const char *name_;
  int comparisons_ = 0;

public:
  explicit differential_suite(const char *name) : name_(name) {}
  void record() { ++comparisons_; }
  void report() const {
    std::printf("%s: %d differential cases passed\n", name_, comparisons_);
  }
  template <typename... T>
  void equal_expected(std::string_view pattern, const std::string &expected,
                      const T &...args) {
    char buffer[16384];
    auto result = ffmt::format_to_n(buffer, sizeof(buffer),
                                    ffmt::runtime(pattern), args...);
    auto actual = std::string(buffer, result.written);
    if (result.error != ffmt::format_error::none || actual != expected ||
        result.required != expected.size()) {
      std::fprintf(stderr,
                   "%s mismatch format=%.*s error=%d actual=%s expected=%s\n",
                   name_, int(pattern.size()), pattern.data(),
                   int(result.error), actual.c_str(), expected.c_str());
      std::abort();
    }
    record();
  }
  template <typename... T>
  void check(std::string_view pattern, const T &...args) {
    std::string expected;
    try {
      expected = fmt::format(fmt::runtime(pattern), oracle_arg(args)...);
    } catch (const fmt::format_error &error) {
      std::fprintf(stderr, "%s oracle rejected fixture format=%.*s: %s\n",
                   name_, int(pattern.size()), pattern.data(), error.what());
      std::abort();
    }
    equal_expected(pattern, expected, args...);
  }
  template <typename... T>
  void check_wide(std::wstring_view pattern, const T &...args) {
    wchar_t buffer[2048];
    auto expected = fmt::format(fmt::runtime(pattern), oracle_arg(args)...);
    auto result = ffmt::format_to_n(buffer, std::size(buffer),
                                    ffmt::runtime(pattern), args...);
    assert(result.error == ffmt::format_error::none);
    assert(std::wstring(buffer, result.written) == expected);
    record();
  }
  template <typename... T>
  void reject(std::string_view pattern, const T &...args) {
    bool rejected = false;
    try {
      (void)fmt::format(fmt::runtime(pattern), oracle_arg(args)...);
    } catch (const fmt::format_error &) {
      rejected = true;
    }
    if (!rejected) {
      std::fprintf(stderr, "%s oracle accepted negative fixture format=%.*s\n",
                   name_, int(pattern.size()), pattern.data());
      std::abort();
    }
    char buffer[1024];
    assert(ffmt::format_to_n(buffer, sizeof(buffer), ffmt::runtime(pattern),
                             args...)
               .error != ffmt::format_error::none);
    record();
  }
};
} // namespace ffmt_test
