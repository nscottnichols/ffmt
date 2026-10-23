"""Handwritten chrono fixtures introduced with their formatter implementation."""
from case_support import case

CASES = [
    case("chrono-duration", "chrono", "", oracle_expr='fmt::format("{:%T}", std::chrono::milliseconds{-123456})',
         ffmt_expr='ffmt::print("{:%T}", std::chrono::milliseconds{-123456})', oracle_includes=["fmt/chrono.h", "chrono"],
         includes=["ffmt/chrono.hpp", "chrono"], requires=["ffmt/chrono.hpp"]),
    case("chrono-calendar", "chrono", "", oracle_expr='fmt::format("{:%G-W%V-%u}", fmt::year_month_day{fmt::year{2021},fmt::month{1},fmt::day{1}})',
         ffmt_expr='ffmt::print("{:%G-W%V-%u}", std::chrono::year{2021}/1/1)', oracle_includes=["fmt/chrono.h", "chrono"],
         includes=["ffmt/chrono.hpp", "chrono"], requires=["ffmt/chrono.hpp"]),
]
