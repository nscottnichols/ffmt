#ifdef NDEBUG
#undef NDEBUG
#endif
#include <array>
#include <cassert>
#include <cstddef>
#include <string>
static std::string bytes;
static void capture(const char *data, std::size_t size) {
  bytes.append(data, size);
}
#define FFMT_EMIT_CHUNK(data, size) capture(data, size)
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

void const_elements_are_rejected() {
  static_assert(!ffmt::is_formattable<const std::array<counter, 2>>);
  char output[32];
  const std::array<counter, 2> values;
  auto result = ffmt::format_to(output, ffmt::runtime("{}"), values);
  assert(result.error == ffmt::format_error::incompatible_type &&
         result.written == 0 && values[0].value == 0);
}
void bounded_padding_invokes_each_formatter_once() {
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
}
void nested_padding_invokes_each_formatter_once() {
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
}
void stream_padding_documents_replay_limit() {
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
}
int main() {
  const_elements_are_rejected();
  bounded_padding_invokes_each_formatter_once();
  nested_padding_invokes_each_formatter_once();
  stream_padding_documents_replay_limit();
}
