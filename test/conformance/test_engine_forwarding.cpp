// Preserve the caller's cv qualification without copying formatter values.
#include <cassert>
#include <array>
#include <cstddef>
#include <string>
static std::string bytes;
static void capture(const char *data, std::size_t size) {
  bytes.append(data, size);
}
#define FFMT_EMIT_CHUNK(data, size) capture(data, size)
#include <ffmt/args.hpp>
#include <ffmt/ranges.hpp>
struct counter {
  int value = 0;
  counter() = default;
  counter(const counter &) = delete;
  counter(counter &&) = delete;
};
template <typename Char>
struct ffmt::formatter<counter, Char> : ffmt::formatter<int, Char> {
  template <class Context> auto format(counter &item, Context &ctx) const {
    return ffmt::formatter<int, Char>::format(++item.value, ctx);
  }
};
struct mutable_counter_range {
  counter items[2];
  counter *begin() { return items; }
  counter *end() { return items + 2; }
};
struct nested_range {
  mutable_counter_range range;
};
template <> struct ffmt::formatter<nested_range> : ffmt::formatter<int> {
  template <class Context>
  auto format(nested_range &value, Context &ctx) const {
    auto result =
        ffmt::format_to(ctx.out(), ffmt::runtime("{:*>20:03}"), value.range);
    ctx.on_error(result.error);
    return result.out;
  }
};
struct legacy_counter {
  int value = 0;
};
template <> struct ffmt::formatter<legacy_counter> {
  static auto format(legacy_counter &item) {
    return ffmt::formatted<ffmt::detail::fixed_string{"{}"}, int>{
        {++item.value}};
  }
};
static_assert(ffmt::is_formattable<counter>);
static_assert(!ffmt::is_formattable<const counter>);
static_assert(ffmt::is_formattable<legacy_counter>);
static_assert(!ffmt::is_formattable<const legacy_counter>);
static_assert(!ffmt::is_formattable<const std::array<counter, 2>>);
template <typename Char>
void check_padding(ffmt::basic_string_view<Char> pattern,
                   const std::basic_string<Char> &expected) {
  for (std::size_t capacity = 0; capacity < expected.size() + 3; ++capacity) {
    mutable_counter_range value;
    std::basic_string<Char> buffer(expected.size() + 8, Char('~'));
    auto result = ffmt::format_to_n(buffer.data() + 1, capacity,
                                    ffmt::runtime(pattern), value);
    assert(result && result.required == expected.size() &&
           result.written == std::min(capacity, expected.size()));
    assert(buffer.substr(1, result.written) == expected.substr(0, capacity));
    assert(buffer[0] == Char('~') && buffer[capacity + 1] == Char('~'));
    assert(value.items[0].value == 1 && value.items[1].value == 1);
  }
}
int main() {
  char output[32];
  counter value;
  auto first = ffmt::format_to(output, "{}", value);
  assert(first && std::string(output, first.written) == "1" &&
         value.value == 1);
  auto second =
      ffmt::format_to_n(output, sizeof output, ffmt::runtime("{}"), value);
  assert(second && std::string(output, second.written) == "2" &&
         value.value == 2);
  auto size = ffmt::formatted_size("{}", value);
  assert(size && size.required == 1 && value.value == 3);
  auto named = ffmt::format_to(output, "{item}", ffmt::arg("item", value));
  assert(named && std::string(output, named.written) == "4" &&
         value.value == 4);
  ffmt::print("{}", value);
  assert(bytes == "5" && value.value == 5);
  bytes.clear();
  auto checked = ffmt::try_print<"{}">(value);
  assert(checked && bytes == "6" && value.value == 6);
  bytes.clear();
  ffmt::println("{}", value);
  assert(bytes == "7\n" && value.value == 7);
  auto temporary = ffmt::format_to(output, "{}", counter{});
  assert(temporary && std::string(output, temporary.written) == "1");
  auto temporary_named =
      ffmt::format_to(output, "{item}", ffmt::arg("item", counter{}));
  assert(temporary_named &&
         std::string(output, temporary_named.written) == "1");
  const counter immutable;
  auto invalid = ffmt::format_to(output, ffmt::runtime("{}"), immutable);
  assert(invalid.error == ffmt::format_error::incompatible_type &&
         invalid.written == 0);
  legacy_counter old;
  auto legacy = ffmt::format_to(output, "{}", old);
  assert(legacy && old.value == 1 &&
         std::string(output, legacy.written) == "1");
  const legacy_counter old_const;
  auto invalid_legacy = ffmt::format_to(output, ffmt::runtime("{}"), old_const);
  assert(invalid_legacy.error == ffmt::format_error::incompatible_type);
  const std::array<counter, 2> const_elements;
  auto invalid_elements =
      ffmt::format_to(output, ffmt::runtime("{}"), const_elements);
  assert(invalid_elements.error == ffmt::format_error::incompatible_type &&
         invalid_elements.written == 0 && const_elements[0].value == 0);
  check_padding<char>("p{:*>20:03}q", "p**********[001, 001]q");
  check_padding<char>("p{:*<20:03}q", "p[001, 001]**********q");
  check_padding<char>("p{:*^21:03}q", "p*****[001, 001]******q");
  check_padding<char>("p{:─>15:03}q", "p─────[001, 001]q");
  check_padding<wchar_t>(L"p{:─>20:03}q", L"p──────────[001, 001]q");
  mutable_counter_range count_only;
  auto no_storage = ffmt::format_to_n(static_cast<char *>(nullptr), 0,
                                      "{:>2147483647:03}", count_only);
  assert(no_storage && no_storage.required == 2147483647 &&
         no_storage.written == 0 && count_only.items[0].value == 1 &&
         count_only.items[1].value == 1);
  for (std::size_t capacity : {0, 1, 9, 12, 20, 22}) {
    nested_range value;
    std::string buffer(32, '~');
    auto result = ffmt::format_to_n(buffer.data(), capacity, "p{}q", value);
    const std::string expected = "p**********[001, 001]q";
    assert(result && result.required == expected.size() &&
           buffer.substr(0, result.written) == expected.substr(0, capacity) &&
           buffer[capacity] == '~' && value.range.items[0].value == 1 &&
           value.range.items[1].value == 1);
  }
  bytes.clear();
  mutable_counter_range streamed;
  auto left = ffmt::try_print("{:*<20:03}", streamed);
  assert(left && bytes == "[001, 001]**********" &&
         streamed.items[0].value == 1 && streamed.items[1].value == 1);
  bytes.clear();
  auto unsupported_padding = ffmt::try_print("{:*>20:03}", streamed);
  assert(unsupported_padding.error == ffmt::format_error::unsupported_type &&
         bytes.empty() && streamed.items[0].value == 1 &&
         streamed.items[1].value == 1);
#ifdef FFMT_TEST_CONST_REJECT
  ffmt::print("{}", immutable);
#endif
}
