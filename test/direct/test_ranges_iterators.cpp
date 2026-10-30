#ifdef NDEBUG
#undef NDEBUG
#endif
#include <array>
#include <cassert>
#include <cstdio>
#include <ffmt/ranges.hpp>
#include <fmt/ranges.h>
#include <fmt/xchar.h>
#include <string>
#include <string_view>

struct mutable_numbers {
  int *first;
  int *last;
  FMT_HD int *begin() { return first; }
  FMT_HD int *end() { return last; }
};
struct move_iterator {
  using value_type = int;
  using difference_type = std::ptrdiff_t;
  int *current;
  FMT_HD explicit move_iterator(int *value) : current(value) {}
  FMT_HD move_iterator(move_iterator &&other) : current(other.current) {}
  move_iterator(const move_iterator &) = delete;
  FMT_HD int &operator*() const { return *current; }
  FMT_HD move_iterator &operator++() {
    ++current;
    return *this;
  }
  FMT_HD friend bool operator==(const move_iterator &it, const int *end) {
    return it.current == end;
  }
  FMT_HD friend bool operator!=(const move_iterator &it, const int *end) {
    return it.current != end;
  }
};
template <typename T> struct consuming_range {
  T *current;
  T *finish;
  int begins = 0;
  struct sentinel {
    T *value;
  };
  struct iterator {
    using iterator_concept = std::input_iterator_tag;
    using value_type = T;
    using difference_type = std::ptrdiff_t;
    T **current;
    FMT_HD T &operator*() const { return **current; }
    FMT_HD iterator &operator++() {
      ++*current;
      return *this;
    }
    FMT_HD void operator++(int) { ++*this; }
    FMT_HD friend bool operator==(const iterator &it, sentinel end) {
      return *it.current == end.value;
    }
    FMT_HD friend bool operator!=(const iterator &it, sentinel end) {
      return *it.current != end.value;
    }
  };
  FMT_HD iterator begin() {
    ++begins;
    return {&current};
  }
  FMT_HD sentinel end() { return {finish}; }
};

// This view deliberately has no data()/size() interface: each logical code unit
// occupies every other storage element. It is directly usable with device data.
template <typename Char, bool Debug> struct strided_text {
  struct iterator {
    using iterator_category = std::forward_iterator_tag;
    using value_type = Char;
    using difference_type = std::ptrdiff_t;
    using pointer = const Char *;
    using reference = const Char &;
    const Char *value;
    FMT_HD const Char &operator*() const { return *value; }
    FMT_HD iterator &operator++() {
      value += 2;
      return *this;
    }
    FMT_HD iterator operator++(int) {
      auto old = *this;
      ++*this;
      return old;
    }
    FMT_HD bool operator==(const iterator &) const = default;
  };
  const Char *data;
  std::size_t count;
  FMT_HD iterator begin() const { return {data}; }
  FMT_HD iterator end() const { return {data + 2 * count}; }
};
template <typename Char, bool Debug>
struct ffmt::range_format_kind<strided_text<Char, Debug>, Char>
    : std::integral_constant<ffmt::range_format,
                             Debug ? ffmt::range_format::debug_string
                                   : ffmt::range_format::string> {};
template <typename Char, bool Debug>
struct fmt::range_format_kind<strided_text<Char, Debug>, Char>
    : std::integral_constant<fmt::range_format,
                             Debug ? fmt::range_format::debug_string
                                   : fmt::range_format::string> {};

int comparisons = 0;
template <typename Char, typename T>
void check(std::basic_string_view<Char> pattern, T &&value) {
  auto expected = fmt::format(fmt::runtime(fmt::basic_string_view<Char>(
                                  pattern.data(), pattern.size())),
                              value);
  std::array<Char, 1024> buffer;
  for (std::size_t capacity : {std::size_t(0), std::size_t(1), std::size_t(5),
                               std::size_t(32), std::size_t(1000)}) {
    buffer.fill(Char('!'));
    auto result = ffmt::format_to_n(buffer.data() + 1, capacity,
                                    ffmt::runtime(pattern), value);
    if (result.error != ffmt::format_error::none ||
        result.required != expected.size() ||
        std::basic_string<Char>(buffer.data() + 1, result.written) !=
            expected.substr(0, capacity)) {
      std::string displayed_pattern;
      for (Char c : pattern)
        displayed_pattern.push_back(c < 128 ? static_cast<char>(c) : '?');
      std::fprintf(stderr,
                   "noncontiguous range mismatch at comparison %d format=%s, "
                   "error=%d required=%zu expected=%zu\n",
                   comparisons, displayed_pattern.c_str(), int(result.error),
                   result.required, expected.size());
      std::abort();
    }
    assert(result.required == expected.size());
    assert(result.written == std::min(capacity, expected.size()));
    assert(buffer.front() == Char('!') && buffer[capacity + 1] == Char('!'));
    ++comparisons;
  }
}
void check_move_join(std::string_view pattern) {
  int values[] = {1, -2, 33};
  auto expected =
      fmt::format(fmt::runtime(pattern),
                  fmt::join(move_iterator(values), values + 3, " | "));
  for (std::size_t capacity :
       {std::size_t(0), std::size_t(1), std::size_t(5), std::size_t(1000)}) {
    std::array<char, 1024> output;
    output.fill('!');
    auto result =
        ffmt::format_to_n(output.data() + 1, capacity, ffmt::runtime(pattern),
                          ffmt::join(move_iterator(values), values + 3, " | "));
    assert(result.error == ffmt::format_error::none &&
           result.required == expected.size());
    assert(result.written == std::min(capacity, expected.size()));
    assert(std::string(output.data() + 1, result.written) ==
           expected.substr(0, capacity));
    assert(output.front() == '!' && output[capacity + 1] == '!');
    ++comparisons;
  }
  auto actual_view = ffmt::join(move_iterator(values), values + 3, " | ");
  auto expected_view = fmt::join(move_iterator(values), values + 3, " | ");
  for (int pass = 0; pass != 2; ++pass) {
    std::array<char, 256> output;
    auto reference = fmt::format(fmt::runtime(pattern), expected_view);
    auto result = ffmt::format_to_n(output.data(), output.size(),
                                    ffmt::runtime(pattern), actual_view);
    assert(result.error == ffmt::format_error::none &&
           result.required == reference.size());
    assert(std::string(output.data(), result.written) == reference);
    ++comparisons;
  }
}
void check_consuming_alignment() {
  static_assert(std::input_iterator<consuming_range<int>::iterator>);
  static_assert(!std::forward_iterator<consuming_range<int>::iterator>);
  int values[] = {1, -2, 33};
  for (auto pattern : {"prefix{:*>20:03}end", "prefix{:*^20:03}end",
                       "prefix{:*<20:03}end", "prefix{:─^20:03}end"}) {
    consuming_range<int> reference{values, values + 3};
    auto expected = fmt::format(fmt::runtime(pattern), reference);
    assert(reference.current == values + 3 && reference.begins == 1);
    for (std::size_t capacity = 0; capacity != 64; ++capacity) {
      std::array<char, 128> buffer;
      buffer.fill('!');
      consuming_range<int> actual{values, values + 3};
      auto result = ffmt::format_to_n(buffer.data() + 1, capacity,
                                      ffmt::runtime(pattern), actual);
      assert(result.error == ffmt::format_error::none &&
             result.required == expected.size());
      assert(std::string(buffer.data() + 1, result.written) ==
             expected.substr(0, capacity));
      assert(actual.current == values + 3 && actual.begins == 1);
      assert(buffer.front() == '!' && buffer[capacity + 1] == '!');
      ++comparisons;
    }
  }
  consuming_range<int> actual{values, values + 3},
      reference{values, values + 3};
  auto expected = fmt::format("{:*<20:03}", reference);
  std::string output;
  auto result =
      ffmt::format_to(std::back_inserter(output), "{:*<20:03}", actual);
  assert(result.error == ffmt::format_error::none && output == expected &&
         actual.begins == 1);
  ++comparisons;
  for (auto pattern : {"{:*>20:03}", "{:*^20:03}"}) {
    consuming_range<int> input{values, values + 3};
    std::string rejected;
    auto error = ffmt::format_to(std::back_inserter(rejected),
                                 ffmt::runtime(pattern), input);
    assert(error.error == ffmt::format_error::unsupported_type &&
           rejected.empty());
    assert(input.current == values && input.begins == 0);
    ++comparisons;
  }
  char nothing = '!';
  consuming_range<int> large{values, values + 3};
  auto measured = ffmt::format_to_n(&nothing, 0, "{:>2147483647:03}", large);
  assert(measured.error == ffmt::format_error::none &&
         measured.required == INT_MAX && measured.written == 0);
  assert(large.begins == 1 && nothing == '!');
  ++comparisons;
}
template <typename Char, bool Debug>
void sample(std::basic_string_view<Char> text) {
  std::array<Char, 1024> storage;
  storage.fill(Char('#'));
  for (std::size_t i = 0; i < text.size(); ++i)
    storage[2 * i] = text[i];
  strided_text<Char, Debug> value{storage.data(), text.size()};
  if constexpr (std::is_same_v<Char, char>) {
    for (auto pattern :
         {"{}", "{:s}", "{:?}", "{:.0}", "{:.1}", "{:.2}", "{:.3}", "{:.4}",
          "{:.5}", "{:.10}", "{:*>20.4}", "{:─^20.6}"})
      check(std::string_view(pattern), value);
  } else {
    for (auto pattern :
         {L"{}", L"{:s}", L"{:?}", L"{:.0}", L"{:.1}", L"{:.2}", L"{:.3}",
          L"{:.4}", L"{:.5}", L"{:.10}", L"{:*>20.4}", L"{:─^20.6}"})
      check(std::wstring_view(pattern), value);
  }
}
int main() {
  static_assert(ffmt::is_formattable<mutable_numbers>);
  static_assert(!ffmt::is_formattable<const mutable_numbers>);
  int numbers[] = {1, -2, 33};
  mutable_numbers mutable_range{numbers, numbers + 3};
  for (auto pattern : {"{}", "{:n}", "{:*>20:03}", "{::#06x}"})
    check(std::string_view(pattern), mutable_range);
  std::array<char, 256> output;
  const auto &immutable = mutable_range;
  assert(ffmt::format_to_n(output.data(), output.size(), ffmt::runtime("{}"),
                           immutable)
             .error == ffmt::format_error::incompatible_type);
  ++comparisons;
  for (auto pattern : {"{}", "{:04}", "{:+#08x}"})
    check_move_join(pattern);
  check_consuming_alignment();
  for (std::string_view text :
       {std::string_view{}, std::string_view("hello"),
        std::string_view("a\n\t\"\\"), std::string_view("静夜思🦊é"),
        std::string_view("a\0b", 3),
        std::string_view("a\xff\xe0\x80\x80z", 6)}) {
    sample<char, false>(text);
    sample<char, true>(text);
  }
  for (std::wstring_view text :
       {std::wstring_view{}, std::wstring_view(L"hello"),
        std::wstring_view(L"a\n\t\"\\"), std::wstring_view(L"静夜思🦊é"),
        std::wstring_view(L"a\0b", 3)}) {
    sample<wchar_t, false>(text);
    sample<wchar_t, true>(text);
  }
  check(std::wstring_view(L"{:*>25}"), std::tuple{42, L"静夜思"});
  std::printf("ranges: %d mutable/move-only/noncontiguous/wide/bounded "
              "comparisons passed\n",
              comparisons);
}
