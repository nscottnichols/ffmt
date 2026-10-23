"""Handwritten printf fixtures introduced with their formatter implementation."""
from case_support import case

CASES = [
    case("printf-style", "printf", "", oracle_expr='fmt::sprintf("%#08x %.3f", 42u, 1.25)',
         ffmt_expr='(void)ffmt::try_printf("%#08x %.3f", 42u, 1.25)', oracle_includes=["fmt/printf.h"],
         includes=["ffmt/printf.hpp"], requires=["ffmt/printf.hpp"]),
]
