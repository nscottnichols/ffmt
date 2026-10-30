#ifdef NDEBUG
#undef NDEBUG
#endif
#include <array>
#include <cassert>
#include <ffmt/detail/family.hpp>
#include <iterator>
#include <string>

struct counted_body {
  int calls = 0;
};
template <> struct ffmt::formatter<counted_body> {
  ffmt::detail::basic_format_spec<char> spec;
  constexpr auto parse(ffmt::format_parse_context &ctx) {
    return ffmt::detail::family_outer_specs(ctx, spec);
  }
  template <typename Context>
  auto format(counted_body &value, Context &ctx) const {
    return ffmt::detail::family_padded(
        ctx, spec,
        [&](auto &body) {
          ++value.calls;
          ffmt::detail::family_ascii(body, "body");
        },
        false);
  }
};

void bounded_alignment_renders_once() {
  struct alignment_case {
    const char *pattern;
    const char *expected;
  };
  constexpr alignment_case cases[] = {
      {"{:*>8}", "****body"},
      {"{:*^9}", "**body***"},
      {"{:*<8}", "body****"},
  };
  for (const auto &sample : cases) {
    const std::string expected(sample.expected);
    for (std::size_t capacity = 0; capacity <= expected.size() + 1;
         ++capacity) {
      counted_body value;
      std::array<char, 32> output;
      output.fill('!');
      auto result = ffmt::format_to_n(output.data() + 1, capacity,
                                      ffmt::runtime(sample.pattern), value);
      assert(result && result.required == expected.size());
      assert(result.written == std::min(capacity, expected.size()));
      assert(std::string(output.data() + 1, result.written) ==
             expected.substr(0, capacity));
      assert(value.calls == 1);
      assert(output[0] == '!' && output[capacity + 1] == '!');
    }
  }
}
void single_pass_iterators_have_an_explicit_limit() {
  counted_body left;
  std::string output;
  auto result = ffmt::format_to(std::back_inserter(output), "{:*<8}", left);
  assert(result && output == "body****" && left.calls == 1);
  for (const char *pattern : {"{:*>8}", "{:*^9}"}) {
    counted_body value;
    output.clear();
    auto rejected = ffmt::format_to(std::back_inserter(output),
                                    ffmt::runtime(pattern), value);
    assert(rejected.error == ffmt::format_error::unsupported_type);
    assert(output.empty() && value.calls == 0);
  }
}
struct replayable_body {
  int *calls;
};
template <>
struct ffmt::formatter<replayable_body> : ffmt::formatter<counted_body> {
  template <typename Context>
  auto format(const replayable_body &value, Context &ctx) const {
    return ffmt::detail::family_padded(
        ctx, spec,
        [&](auto &body) {
          ++*value.calls;
          ffmt::detail::family_ascii(body, "body");
        },
        true);
  }
};
void replayable_streaming_alignment_measures_then_renders() {
  struct alignment_case {
    const char *pattern;
    const char *expected;
  };
  constexpr alignment_case cases[] = {{"{:*>8}", "****body"},
                                      {"{:*^9}", "**body***"}};
  for (const auto &sample : cases) {
    int calls = 0;
    std::string output;
    auto result =
        ffmt::format_to(std::back_inserter(output),
                        ffmt::runtime(sample.pattern), replayable_body{&calls});
    assert(result && output == sample.expected);
    assert(result.written == output.size() && result.required == output.size());
    assert(calls == 2); // One measurement pass followed by one output pass.
  }
}
void display_column_counting() {
  ffmt::detail::family_counter<char> counter;
  for (char c : std::string("静é"))
    counter.push(c);
  assert(counter.size() == 3);
  counter.push(char(0xff));
  assert(counter.size() == 4);
  ffmt::detail::family_counter<wchar_t> wide;
  wide.push(L'静');
  assert(wide.size() == 1);
}
int main() {
  bounded_alignment_renders_once();
  single_pass_iterators_have_an_explicit_limit();
  replayable_streaming_alignment_measures_then_renders();
  display_column_counting();
}
