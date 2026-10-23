#ifdef NDEBUG
#undef NDEBUG
#endif
#include <array>
#include <cassert>
#include <ffmt/args.hpp>
#include <fmt/format.h>
#include <string>

struct counter {
  int value = 0;
  counter() = default;
  counter(const counter &) = delete;
  counter(counter &&) = delete;
};
template <> struct ffmt::formatter<counter> : ffmt::formatter<int> {
  template <typename Context> auto format(counter &value, Context &ctx) const {
    return ffmt::formatter<int>::format(++value.value, ctx);
  }
};
void names_resolve_fields_and_widths() {
  char output[128];
  auto result =
      ffmt::format_to_n(output, sizeof(output), "{value:04} {width}",
                        ffmt::arg("value", 42), ffmt::arg("width", 7));
  auto expected = fmt::format("{value:04} {width}", fmt::arg("value", 42),
                              fmt::arg("width", 7));
  assert(result && result.required == expected.size());
  assert(std::string(output, result.written) == expected);
  auto padded =
      ffmt::format_to_n(output, sizeof(output), "{value:>{width}}",
                        ffmt::arg("value", 42), ffmt::arg("width", 7));
  expected = fmt::format("{value:>{width}}", fmt::arg("value", 42),
                         fmt::arg("width", 7));
  assert(padded && std::string(output, padded.written) == expected);
}
void named_values_preserve_mutability_without_copying() {
  char output[32];
  counter value;
  auto named = ffmt::format_to(output, "{item}", ffmt::arg("item", value));
  assert(named && std::string(output, named.written) == "1" &&
         value.value == 1);
  auto temporary =
      ffmt::format_to(output, "{item}", ffmt::arg("item", counter{}));
  assert(temporary && std::string(output, temporary.written) == "1");
}
template <typename... Args>
void reject_named(std::string_view pattern, ffmt::format_error expected,
                  Args &&...args) {
  std::array<char, 32> output;
  output.fill('!');
  auto result =
      ffmt::format_to_n(output.data(), output.size(), ffmt::runtime(pattern),
                        std::forward<Args>(args)...);
  assert(result.error == expected && result.written == 0);
  assert(result.out == output.data());
  for (char unit : output)
    assert(unit == '!');
}
void missing_names_and_incompatible_specs_report_errors() {
  reject_named("{missing}", ffmt::format_error::argument_out_of_range,
               ffmt::arg("value", 42));
  reject_named("{value:d}", ffmt::format_error::incompatible_type,
               ffmt::arg("value", "text"));
}
void named_width_requires_a_nonnegative_integer() {
  reject_named("{value:>{width}}", ffmt::format_error::invalid_dynamic_spec,
               ffmt::arg("value", 42), ffmt::arg("width", -1));
  reject_named("{value:>{width}}", ffmt::format_error::invalid_dynamic_spec,
               ffmt::arg("value", 42), ffmt::arg("width", 1.25));
}
int main() {
  names_resolve_fields_and_widths();
  named_values_preserve_mutability_without_copying();
  missing_names_and_incompatible_specs_report_errors();
  named_width_requires_a_nonnegative_integer();
}
