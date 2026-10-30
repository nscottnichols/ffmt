// Host-only checks for APIs that return sizes/status or write bounded buffers.
// Text printed by shared host/device cases uses capture.hpp instead.
#pragma once
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <algorithm>
#include <array>
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <ffmt/base.hpp>
#include <fmt/format.h>
#include <fmt/xchar.h>
#include <iostream>
#include <string>
#include <string_view>

namespace ffmt_test {
template <typename... Args>
void check_output(ffmt::string_view pattern, const std::string &expected,
                  bool check_bounds, const Args &...args) {
  std::string actual(expected.size() + 8, '~');
  auto result = ffmt::format_to_n(actual.data(), expected.size(),
                                  ffmt::runtime(pattern), args...);
  if (!result || result.required != expected.size() ||
      result.written != expected.size() ||
      actual.substr(0, result.written) != expected) {
    std::cerr << "pattern=" << pattern << " expected=" << expected
              << " actual=" << actual.substr(0, result.written) << '\n';
    std::abort();
  }
  assert(actual.substr(expected.size()) == std::string(8, '~'));
  if (!check_bounds)
    return;
  for (std::size_t bound :
       {std::size_t(0), std::size_t(1), expected.size() / 2}) {
    actual.assign(expected.size() + 8, '~');
    auto bounded = ffmt::format_to_n(actual.data(), bound,
                                     ffmt::runtime(pattern), args...);
    const auto retained = std::min(bound, expected.size());
    assert(bounded && bounded.required == expected.size() &&
           bounded.written == retained);
    assert(actual.substr(0, retained) == expected.substr(0, retained));
    assert(actual.substr(retained) ==
           std::string(actual.size() - retained, '~'));
  }
}

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
    check_output(pattern, expected, false, args...);
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

// Primitive/float sweeps retain zero, one, and half-length truncation checks.
template <typename... Args>
void compare(ffmt::string_view pattern, const Args &...args) {
  auto expected = fmt::format(
      fmt::runtime(fmt::string_view{pattern.data(), pattern.size()}), args...);
  ffmt_test::check_output(pattern, expected, true, args...);
}
