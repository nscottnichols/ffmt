#!/usr/bin/env python3
"""Run the original compile-rejection corpus with positive and syntax controls."""
import argparse
from pathlib import Path
import shlex
import subprocess
import sys

from compile_diagnostics import negative_status

ROOT = Path(__file__).resolve().parents[1]


def compile_case(command, source, flags, output, name):
    """Keep compiler diagnostics in the build tree; a crash is never a rejection."""
    argv = command + flags + ["-c", str(source), "-o", str(output / "negative.o")]
    try:
        process = subprocess.run(argv, cwd=ROOT, capture_output=True, timeout=180)
        result = dict(exit_code=process.returncode, stderr=process.stderr)
    except (OSError, subprocess.TimeoutExpired) as error:
        result = dict(exit_code=-1, stderr=str(error).encode())
    (output / (name + ".stderr")).write_bytes(result["stderr"])
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--cxx", default="c++")
    parser.add_argument("--flags", default="-std=c++20 -Iinclude -Ithird_party/fmt/include")
    parser.add_argument("--output", type=Path, default=ROOT / "build/negative")
    args = parser.parse_args()
    args.output.mkdir(parents=True, exist_ok=True)
    command = shlex.split(args.cxx) + shlex.split(args.flags)
    variants = {"specifiers": ["-DFFMT_BUFFER_PATH=0"],
                "buffer": ["-DFFMT_BUFFER_PATH=1"], "fmt": ["-DNEG_CHECK_FMT"]}
    failed = False
    for name, flags in variants.items():
        result = compile_case(command, ROOT / "test/negative/control/accept_valid.cpp",
                              flags, args.output, "control-" + name)
        if result["exit_code"]:
            print("negative/control [%s]: FAIL (valid format did not compile)" % name)
            return 1
    for source in sorted((ROOT / "test/negative").glob("*.cpp")):
        result = compile_case(command, source, ["-DNEG_SYNTAX_CHECK"],
                              args.output, source.stem + "-syntax")
        if result["exit_code"]:
            print("negative/%s: FAIL (syntax control)" % source.stem)
            failed = True
            continue
        for name, flags in variants.items():
            result = compile_case(command, source, flags, args.output, source.stem + "-" + name)
            status = negative_status(result)
            print("negative/%s [%s]: %s" % (source.stem, name, status), flush=True)
            failed |= status != "expected_rejection"
    print("Negative diagnostics: " + str(args.output))
    return int(failed)


if __name__ == "__main__":
    sys.exit(main())
