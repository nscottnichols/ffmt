"""Handwritten style fixtures introduced with their formatter implementation."""
from case_support import case

CASES = [
    case("styled", "style", "", oracle_expr='fmt::format("{}", fmt::styled(42, fmt::fg(fmt::rgb(255,20,30)) | fmt::emphasis::bold))',
         ffmt_expr='ffmt::print("{}", ffmt::styled(42, ffmt::fg(ffmt::rgb(255,20,30)) | ffmt::emphasis::bold))',
         oracle_includes=["fmt/color.h"], includes=["ffmt/color.hpp"], requires=["ffmt/color.hpp"]),
]
