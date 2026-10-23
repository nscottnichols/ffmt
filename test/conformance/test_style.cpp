#include "family_test_support.hpp"
#include <ffmt/color.hpp>
#include <fmt/color.h>
namespace {
ffmt_test::differential_suite suite("style");
void explicit_ansi_styles() {
  auto style = ffmt::fg(ffmt::rgb(255, 20, 30)) |
               ffmt::bg(ffmt::terminal_color::bright_blue) |
               ffmt::emphasis::bold | ffmt::emphasis::underline;
  auto oracle_style = fmt::fg(fmt::rgb(255, 20, 30)) |
                      fmt::bg(fmt::terminal_color::bright_blue) |
                      fmt::emphasis::bold | fmt::emphasis::underline;
  suite.equal_expected(
      "{:>12}", fmt::format("{:>12}", fmt::styled("color", oracle_style)),
      ffmt::styled("color", style));
  suite.equal_expected("{}",
                       fmt::format("{}", fmt::styled(42, fmt::text_style{})),
                       ffmt::styled(42, ffmt::text_style{}));
  suite.equal_expected(
      "{}",
      fmt::format("{}",
                  fmt::styled("palette", fmt::fg(fmt::color::rebecca_purple))),
      ffmt::styled("palette", ffmt::fg(ffmt::color::rebecca_purple)));
}

} // namespace
int main() {
  explicit_ansi_styles();
  suite.report();
}
