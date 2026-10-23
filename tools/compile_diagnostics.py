"""Classify negative-compile diagnostics conservatively."""
import re

def negative_status(p):
    if p["exit_code"] == 0:
        return "unexpected_acceptance"
    text = p["stderr"].decode("utf-8", "replace")
    if p["exit_code"] < 0 or re.search(
        r"internal compiler error|PLEASE submit a bug|LLVM ERROR|Segmentation fault|"
        r"No such file or directory|file not found|unable to execute|cannot find -l|"
        r"No available targets|unsupported.*(gpu|architecture|target)|failed to create.*thread",
        text, re.I):
        return "infrastructure_failure"
    if re.search(r"constant expression|consteval|constexpr|invalid.format|invalid.*(spec|type)|"
                 r"format.*(error|invalid|reject)|precision|sign_not_allowed|"
                 r"unmatched|argument_index|cannot_mix|not.allowed", text, re.I):
        return "expected_rejection"
    return "wrong_diagnostic"
