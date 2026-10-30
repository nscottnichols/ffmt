// The build copies only base.hpp into an otherwise empty include directory.
#include <ffmt/base.hpp>

int main() {
  char output[96];
  for (char &unit : output)
    unit = '!';
  auto result = ffmt::format_to_n(output + 1, 4, "{value:06x}",
                                  ffmt::arg("value", 42));
  if (!result || result.required != 6 || result.written != 4 ||
      output[0] != '!' || output[1] != '0' || output[4] != '0' ||
      output[5] != '!')
    return 1;
  int values[] = {1, 2};
  auto range = ffmt::format_to_n(output, sizeof output, "{}", values);
  if (!range || range.required != 6 || output[0] != '[' || output[4] != '2' ||
      output[5] != ']')
    return 2;
  auto duration = ffmt::format_to_n(output, sizeof output, "{}",
                                    std::chrono::seconds{42});
  if (!duration || duration.required != 3 || output[0] != '4' ||
      output[1] != '2' || output[2] != 's')
    return 3;
  auto optional = ffmt::format_to_n(output, sizeof output, "{}",
                                    std::optional<int>{42});
  if (!optional || optional.required != 12 || output[0] != 'o' ||
      output[11] != ')')
    return 4;
  auto styled = ffmt::format_to_n(output, sizeof output, "{}",
                                  ffmt::styled(42, ffmt::emphasis::bold));
  constexpr char expected_style[] = "\x1b[1m42\x1b[0m";
  if (!styled || styled.required != sizeof(expected_style) - 1)
    return 5;
  for (std::size_t i = 0; i != styled.written; ++i)
    if (output[i] != expected_style[i])
      return 6;
  auto printf_result = ffmt::printf_to_n(output, sizeof output, "%04x", 42u);
  if (!printf_result || printf_result.required != 4 || output[0] != '0' ||
      output[1] != '0' || output[2] != '2' || output[3] != 'a')
    return 7;
  auto rejected = ffmt::format_to_n(output, sizeof output,
                                    ffmt::runtime("{:d}"), "text");
  return rejected.error == ffmt::format_error::incompatible_type ? 0 : 8;
}
