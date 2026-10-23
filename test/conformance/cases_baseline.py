"""Baseline cases supported by the original public implementation."""
from case_support import case

CASES = [
    case("literal", "literal", "hello {{world}} %\n"),
    case("int-min", "integer", "{}", "INT64_MIN"),
    case("uint-max", "integer", "{}", "UINT64_MAX"),
    case("ascii", "string", "[{:.3s}] [{:>10s}]", '"abcdef"', '"hello"'),
]
