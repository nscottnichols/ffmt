#!/usr/bin/env python3
"""Source-position coverage of a file/directory, unioned over several binaries.

The header is compiled twice (FFMT_BUFFER_PATH=0 and =1), so the same
function exists in both coverage binaries, often with a different body.
`llvm-cov export A -object B` keeps only one record per function, and
which one depends on object order: a function the specifiers binary never
calls (zero counts) hides the buffer binary's real counts, and functions
whose body differs report "mismatched data". The total then depends on
the order the binaries are listed in.

Instead, run and export each binary on its own with its own profile, and
count a code region (file position) as covered if any binary covers it.

usage: coverage_union.py LLVM_COV LLVM_PROFDATA SOURCE BINARY...
Prints {"covered": N, "count": M, "percent": P} as JSON.
"""
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile


def regions(cov, profdata, source, binary, tmp):
    root = Path(source).resolve()
    sources = sorted(str(p.resolve()) for p in root.rglob("*.hpp")) if root.is_dir() else [str(root)]
    allowed = set(sources)
    name = os.path.basename(binary)
    raw = os.path.join(tmp, name + ".profraw")
    prof = os.path.join(tmp, name + ".profdata")
    env = dict(os.environ, LLVM_PROFILE_FILE=raw)
    subprocess.run([str(Path(binary).resolve())], env=env, stdout=subprocess.DEVNULL,
                   stderr=subprocess.DEVNULL, check=True)
    subprocess.run([profdata, "merge", "-o", prof, raw], check=True)
    command = [cov, "export", binary, "-instr-profile=" + prof, "-sources"] + sources
    out = subprocess.check_output(command).decode()
    found = {}
    data = json.loads(out)["data"][0]
    for fn in data["functions"]:
        filenames = [os.path.realpath(p) for p in fn["filenames"]]
        for ls, cs, le, ce, count, fid, _, kind, *_ in fn["regions"]:
            if kind != 0 or filenames[fid] not in allowed:
                continue  # code regions of SOURCE only
            key = (filenames[fid], ls, cs, le, ce)
            found[key] = found.get(key, 0) or count  # any instantiation
    branches = {}
    for file in data["files"]:
        name = os.path.realpath(file["filename"])
        if name not in allowed:
            continue
        for branch in file.get("branches", []):
            if len(branch) >= 9 and branch[8] != 4:
                continue
            ls, cs, le, ce, yes, no = branch[:6]
            for side, count in enumerate((yes, no)):
                key = (name, ls, cs, le, ce, side)
                branches[key] = branches.get(key, 0) or count
    # LCOV provides executable-line counts directly; do not approximate them
    # from multiline region spans or normalized source text.
    lcov = subprocess.check_output(command[:2] + ["-format=lcov"] + command[2:]).decode()
    lines, name = {}, None
    for line in lcov.splitlines():
        if line.startswith("SF:"):
            name = os.path.realpath(line[3:])
        elif line.startswith("DA:") and name in allowed:
            number, count = line[3:].split(",")[:2]
            key = (name, int(number))
            lines[key] = lines.get(key, 0) or int(count)
    return dict(regions=found, branches=branches, lines=lines)


def metrics(values):
    covered, count = sum(bool(c) for c in values.values()), len(values)
    return dict(covered=covered, count=count, percent=100.0 * covered / count if count else None)


def main():
    cov, profdata, source, *binaries = sys.argv[1:]
    union = dict(regions={}, branches={}, lines={})
    with tempfile.TemporaryDirectory() as tmp:
        for b in binaries:
            for category, values in regions(cov, profdata, source, b, tmp).items():
                for key, count in values.items():
                    union[category][key] = union[category].get(key, 0) or count
    result = metrics(union["regions"])
    result.update(lines=metrics(union["lines"]), branches=metrics(union["branches"]),
                  instrumented_scope=source,
                  exclusions=["Device-only code and uninstantiated template bodies are not instrumented.",
                              "Compile-time evaluation is not runtime coverage."], files={})
    filenames = {key[0] for values in union.values() for key in values}
    for filename in sorted(filenames):
        result["files"][os.path.relpath(filename)] = {
            category: metrics({k: v for k, v in values.items() if k[0] == filename})
            for category, values in union.items()}
    print(json.dumps(result, sort_keys=True))


if __name__ == "__main__":
    main()
