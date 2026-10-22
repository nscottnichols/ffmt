#!/usr/bin/env python3
"""Exercise CMake/CTest and Make from a detached, immutable source checkout."""
import argparse
import json
import os
from pathlib import Path
import shlex
import sys

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools"))
import conformance


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--profile", required=True)
    parser.add_argument("--revision", required=True)
    parser.add_argument("--cxx", default=os.environ.get("FFMT_HOST_CXX", os.environ.get("CXX", "g++")))
    parser.add_argument("--coverage", action="store_true", help="Also profile all instantiated headers; requires Clang")
    args = parser.parse_args()
    if not os.environ.get("COBALT_JOBID"):
        parser.error("Build integration checks require a Cobalt allocation")
    if len(shlex.split(args.cxx)) != 1:
        parser.error("--cxx must name a compiler executable")
    output = ROOT / "artifacts" / args.profile / ("build-checks-" + os.environ["COBALT_JOBID"])
    output.mkdir(parents=True, exist_ok=False)
    source, build = output / "source", output / "cmake"
    manifest = dict(schema_version=1, revision=conformance.git("rev-parse", args.revision),
        profile=args.profile, cobalt_jobid=os.environ["COBALT_JOBID"], steps=[], status="running")
    def step(name, command, timeout=1800):
        result = conformance.run(command, output, name, timeout)
        manifest["steps"].append(dict(name=name, **conformance.record(result)))
        (output / "manifest.json").write_text(json.dumps(manifest, indent=2) + "\n")
        print(name, result["exit_code"], flush=True)
        if result["exit_code"]:
            raise RuntimeError(name + " failed; see " + result["stderr_file"])
        return result
    try:
        step("checkout", ["git", "worktree", "add", "--detach", str(source), manifest["revision"]])
        step("compiler", [args.cxx, "--version"])
        oracle = ROOT / "third_party/fmt"
        step("configure", ["cmake", "-S", str(source), "-B", str(build), "-DFFMT_BUILD_TESTS=ON",
            "-DFFMT_BUILD_CXX23_TESTS=ON", "-DCMAKE_BUILD_TYPE=Release",
            "-DCMAKE_CXX_COMPILER=" + args.cxx, "-DFFMT_FMT_DIR=" + str(oracle)])
        step("build", ["cmake", "--build", str(build), "-j2"])
        step("ctest", ["ctest", "--test-dir", str(build), "--output-on-failure", "-j2"])
        step("make-tests", ["make", "-C", str(source), "-j2", "test-host", "test-fast-math", "test-negative",
            "CXX=" + args.cxx, "FMT_DIR=" + str(oracle)])
        if args.coverage:
            result = step("coverage", ["make", "-C", str(source), "-j2", "coverage", "CXX=" + args.cxx,
                "FMT_DIR=" + str(oracle)])
            rows = [line for line in result["stdout"].decode().splitlines() if line.startswith('{"branches":')]
            if len(rows) != 1:
                raise RuntimeError("Missing or duplicate source-coverage record")
            coverage = json.loads(rows[0])
            coverage.update(source_revision=manifest["revision"], profile=args.profile,
                cobalt_jobid=manifest["cobalt_jobid"])
            (output / "coverage.json").write_text(json.dumps(coverage, indent=2) + "\n")
        manifest["status"] = "pass"
    except Exception as error:
        manifest.update(status="failure", reason=str(error))
    (output / "manifest.json").write_text(json.dumps(manifest, indent=2) + "\n")
    print(output / "manifest.json")
    return int(manifest["status"] != "pass")


if __name__ == "__main__":
    raise SystemExit(main())
