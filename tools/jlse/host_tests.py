#!/usr/bin/env python3
"""Run standalone host regressions against an isolated header snapshot and fmt."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import shlex
import sys

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools"))
import conformance

DEFAULT_TESTS = [
    "test_engine.cpp", "test_engine_transport.cpp", "test_engine_legacy_hook.cpp",
    "test_engine_extended.cpp", "test_engine_bitint.cpp", "test_engine_quad.cpp",
    "test_engine_portable.cpp", "test_families.cpp", "test_families_printf.cpp",
    "test_families_std23.cpp",
]


def test_options(filename):
    """Describe synthetic and optional suites without changing their fixtures."""
    source = "test_engine.cpp" if filename == "test_engine_portable.cpp" else filename
    standard = "c++23" if filename == "test_families_std23.cpp" else "c++20"
    defines = []
    if filename == "test_engine_portable.cpp":
        defines.append("-DFFMT_PORTABLE_SHORTEST=1")
    if filename == "test_engine_quad.cpp":
        defines.append("-DFMT_USE_FLOAT128=1")
    return source, standard, defines


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("--profile", required=True)
    p.add_argument("--revision", default="WORKTREE")
    p.add_argument("--test", action="append", choices=sorted(set(DEFAULT_TESTS) | {
        x.name for x in (ROOT / "test/conformance").glob("test_*.cpp")}))
    p.add_argument("--opt", type=int, action="append", choices=[0, 2])
    a = p.parse_args()
    if not os.environ.get("COBALT_JOBID"):
        p.error("Representative host tests require an allocation")
    output = ROOT / "artifacts" / a.profile / ("host-tests-" + os.environ["COBALT_JOBID"])
    output.mkdir(parents=True, exist_ok=True)
    hashes = conformance.snapshot(output / "source", a.revision)
    compiler = shlex.split(os.environ.get("FFMT_HOST_CXX", os.environ.get("CXX", "g++")))
    oracle = ROOT / "third_party/fmt"
    fmt_commit = conformance.git("-C", str(oracle), "rev-parse", "HEAD")
    if fmt_commit != conformance.CONFIG["fmt_commit"]:
        p.error("fmt oracle does not match the campaign pin")
    manifest = dict(schema_version=1, profile=a.profile, revision=a.revision,
                    cobalt_jobid=os.environ["COBALT_JOBID"], source_files=hashes, fmt_commit=fmt_commit, tests=[])
    version = conformance.run([*compiler, "--version"], output, "compiler-version")
    manifest["compiler"] = conformance.record(version)
    macros = conformance.run([*compiler, "-std=c++20", "-dM", "-E", "-x", "c++", "/dev/null"], output, "compiler-macros")
    manifest["compiler_macros"] = conformance.record(macros)
    has_quad = b"#define __SIZEOF_FLOAT128__ " in macros["stdout"]
    build = conformance.run([*compiler, "-std=c++20", "-O2", "-fno-fast-math", "-I"+str(oracle/"include"),
                             "-c", str(oracle/"src/format.cc"), "-o", str(output/"fmt.o")], output, "fmt-build")
    manifest["fmt_build"] = conformance.record(build)
    failed = bool(build["exit_code"])
    if not failed:
        for filename in a.test or DEFAULT_TESTS:
            fixture, standard, defines = test_options(filename)
            # Native Arm long double is tested separately. Do not force fmt's
            # __float128 extension on a compiler that does not expose that type.
            if filename == "test_engine_quad.cpp" and not has_quad:
                defines = []
            source = output / filename
            source.write_bytes((ROOT / "test/conformance" / fixture).read_bytes())
            for opt in a.opt or [0, 2]:
                label = source.stem + "-O"+str(opt)
                executable = output / label
                process = conformance.run([*compiler, "-std="+standard, "-O"+str(opt), "-fno-fast-math", "-UNDEBUG", *defines,
                                          "-I"+str(output/"source/include"), "-I"+str(oracle/"include"),
                                          str(source), str(output/"fmt.o"), "-o", str(executable)], output, label+"-build", timeout=300)
                row = dict(test=filename, fixture=fixture, standard=standard, defines=defines,
                           opt=opt,source_sha256=hashlib.sha256(source.read_bytes()).hexdigest(),build=conformance.record(process))
                if process["exit_code"]:
                    row["status"] = "compile_failure"
                else:
                    execution = conformance.run([str(executable)], output, label+"-run", timeout=300)
                    status = {0: "pass", 77: "unsupported"}.get(execution["exit_code"], "execution_failure")
                    row.update(status=status, execution=conformance.record(execution))
                manifest["tests"].append(row)
                # Exit 77 is a recorded optional capability gap, never a pass.
                failed |= row["status"] not in ("pass", "unsupported")
                (output/"manifest.json").write_text(json.dumps(manifest,indent=2)+"\n")
                print(filename, opt, row["status"], flush=True)
    (output/"manifest.json").write_text(json.dumps(manifest,indent=2)+"\n")
    print(output/"manifest.json")
    raise SystemExit(1 if failed else 0)


if __name__ == "__main__":
    main()
