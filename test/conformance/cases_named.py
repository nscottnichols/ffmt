"""Handwritten named fixtures introduced with their formatter implementation."""
from case_support import case

CASES = [
    case("named-fields", "named", "", oracle_expr='fmt::format("{value:04x}", fmt::arg("value", 42))',
         ffmt_expr='ffmt::print("{value:04x}", ffmt::arg("value", 42))', includes=["ffmt/args.hpp"], requires=["ffmt/args.hpp"]),
]
