"""Handwritten std fixtures introduced with their formatter implementation."""
from case_support import case

CASES = [
    case("optional", "std", "", oracle_expr='fmt::format("{}", std::optional<int>{42})',
         ffmt_expr='ffmt::print("{}", std::optional<int>{42})', oracle_includes=["fmt/std.h", "optional"],
         includes=["ffmt/std.hpp", "optional"], requires=["ffmt/std.hpp"]),
    case("complex", "std", "", oracle_expr='fmt::format("{:.3f}", std::complex<double>{1,-2.2})',
         ffmt_expr='ffmt::print("{:.3f}", std::complex<double>{1,-2.2})', oracle_includes=["fmt/std.h", "complex"],
         includes=["ffmt/std.hpp", "complex"], requires=["ffmt/std.hpp"]),
]
