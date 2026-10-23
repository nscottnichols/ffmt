#!/usr/bin/env python3
"""Reason-aware driver for the existing ffmt negative-compile corpus."""
import argparse
import datetime
import json
from pathlib import Path
import shlex
import sys
from process_evidence import ROOT, record, run
from compile_diagnostics import negative_status


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--cxx", default="c++")
    parser.add_argument("--flags", default="-std=c++20 -Iinclude -Ithird_party/fmt/include")
    parser.add_argument("--output")
    args = parser.parse_args()
    output = Path(args.output) if args.output else ROOT / "artifacts/negative" / datetime.datetime.now(datetime.timezone.utc).strftime("%Y%m%dT%H%M%SZ")
    output.mkdir(parents=True, exist_ok=False)
    command = shlex.split(args.cxx) + shlex.split(args.flags)
    variants = dict(specifiers=["-DFFMT_BUFFER_PATH=0"], buffer=["-DFFMT_BUFFER_PATH=1"], fmt=["-DNEG_CHECK_FMT"])
    results = []
    bad = False
    for variant, flags in variants.items():
        p = run(command + flags + ["-c", ROOT / "test/negative/control/accept_valid.cpp", "-o", output / "control.o"], output, "control-" + variant)
        results.append(dict(id="control", variant=variant, status="pass" if p["exit_code"] == 0 else "infrastructure_failure", process=record(p)))
        bad |= p["exit_code"] != 0
    if not bad:
        for source in sorted((ROOT / "test/negative").glob("*.cpp")):
            syntax = run(command + ["-DNEG_SYNTAX_CHECK", "-c", source, "-o", output / "syntax.o"], output, source.stem + "-syntax")
            if syntax["exit_code"]:
                results.append(dict(id=source.stem, status="infrastructure_failure", syntax=record(syntax)))
                bad = True
                continue
            for variant, flags in variants.items():
                p = run(command + flags + ["-c", source, "-o", output / "negative.o"], output, source.stem + "-" + variant)
                status = negative_status(p)
                results.append(dict(id=source.stem, variant=variant, status=status, syntax=record(syntax), process=record(p)))
                print("negative/%s [%s]: %s" % (source.stem, variant, status), flush=True)
                bad |= status != "expected_rejection"
    (output / "manifest.json").write_text(json.dumps(dict(schema_version=1, cases=results), indent=2) + "\n")
    print("Negative evidence: " + str(output / "manifest.json"))
    return int(bad)


if __name__ == "__main__":
    sys.exit(main())
