#pragma once
#include <algorithm>
#include <cassert>
#include <cstdint>
#include <cstdlib>
#include <ffmt/detail/checked.hpp>
#include <fmt/format.h>
#include <fmt/xchar.h>
#include <iostream>
#include <string>
template <typename... Args>
void compare(ffmt::string_view pattern, const Args &...args) {
  auto expected = fmt::format(
      fmt::runtime(fmt::string_view{pattern.data(), pattern.size()}), args...);
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
  for (std::size_t bound :
       {std::size_t(0), std::size_t(1), expected.size() / 2}) {
    actual.assign(expected.size() + 8, '~');
    auto bounded = ffmt::format_to_n(actual.data(), bound,
                                     ffmt::runtime(pattern), args...);
    const auto retained = std::min(bound, expected.size());
    assert(bounded && bounded.required == expected.size() &&
           bounded.written == retained);
    assert(actual.substr(0, retained) == expected.substr(0, retained));
    assert(actual[retained] == '~' && actual[bound] == '~');
  }
}
