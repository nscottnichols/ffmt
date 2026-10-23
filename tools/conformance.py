#!/usr/bin/env python3
"""Compare ffmt against an independently compiled, pinned host fmt oracle."""
import argparse
import datetime
import hashlib
import importlib.util
import json
import os
from pathlib import Path
import platform
import shlex
import shutil
import subprocess
import sys
from process_evidence import ROOT, compare_bytes, path_label, record, run
from oracle import FMT_COMMIT, oracle_identity
import backends
from backends import ROUTES, compiler, flags, link_flags

WORKSPACE = Path(os.environ.get("FFMT_WORKSPACE", ROOT)).resolve()
SOURCE_ROOT = Path(os.environ.get("FFMT_SOURCE_ROOT", ROOT)).resolve()

def load_profiles(path=None):
    """Load optional execution defaults without depending on a site campaign."""
    if path is None:
        return dict(fmt_commit=FMT_COMMIT, profiles={"local": {
            "routes": ["host-buffer", "host-specifiers"]}})
    data = json.loads(Path(path).read_text())
    if data.get("fmt_commit", FMT_COMMIT) != FMT_COMMIT:
        raise ValueError("profile fmt oracle differs from the portable suite pin")
    profiles = data.get("profiles", {})
    if not isinstance(profiles, dict) or not profiles:
        raise ValueError("profiles must contain a nonempty profile mapping")
    for name, profile in profiles.items():
        if not isinstance(profile, dict):
            raise ValueError("profile %s must contain a mapping" % name)
        routes = profile.get("routes", [])
        if not routes or any(route not in ROUTES for route in routes):
            raise ValueError("profile %s contains missing or unknown routes" % name)
    return dict(data, fmt_commit=FMT_COMMIT)

CONFIG = load_profiles(os.environ.get("FFMT_PROFILES_FILE"))
_spec = importlib.util.spec_from_file_location("ffmt_cases", str(ROOT / "test/conformance/cases.py"))
fixtures = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(fixtures)

def git(*args):
    return subprocess.check_output(["git"] + list(args), cwd=str(ROOT)).decode().strip()

def source_git(*args):
    return subprocess.check_output(["git"] + list(args), cwd=str(SOURCE_ROOT)).decode().strip()

def snapshot(destination, revision):
    if revision != "WORKTREE":
        raise ValueError("this runner revision supports only --revision WORKTREE")
    shutil.copytree(str(SOURCE_ROOT / "include"), str(destination / "include"))
    return {str(path.relative_to(destination)): hashlib.sha256(path.read_bytes()).hexdigest()
            for path in sorted((destination / "include").rglob("*")) if path.is_file()}


def expression(item, reference=False):
    special = item.get("oracle_expr" if reference else "ffmt_expr")
    if special:
        return special
    args = ", ".join([json.dumps(item["pattern"], ensure_ascii=False)] + item["args"])
    if reference:
        return "fmt::format(" + args + ")" + (' + "\\n"' if item.get("println") else "")
    return ("FFMT_PRINTLN(" if item.get("println") else "FFMT_PRINT(") + args + ")"

def extra_headers(items, reference=False):
    keys = ("oracle_includes", "oracle_preamble") if reference else ("includes", "preamble")
    headers = sorted({p for item in items for p in item.get(keys[0], [])})
    return "\n".join("#include <" + p + ">" for p in headers) + "\n" + "\n".join(dict.fromkeys(item.get(keys[1], "") for item in items))

PRELUDE = '\n#include <cstdint>\n#include <cstdio>\n#include <cstdlib>\n#include <limits>\ntemplate<class T, class U> HD T bits(U v) {return __builtin_bit_cast(T, v);}\n'

def oracle_source(items):
    body = "\n".join("case %d: {auto s=%s; std::fwrite(s.data(),1,s.size(),stdout); return 0;}" %
                     (i, expression(item, True)) for i, item in enumerate(items))
    return "#define HD\n#include <fmt/format.h>\n" + PRELUDE + extra_headers(items, True) + """
int main(int argc, char** argv) {
  if(argc!=2) return 2;
  switch(std::atoi(argv[1])) {""" + body + "}return 2;}\n"

def target_source(items, indices, route):
    # Reject unknown adapters before generating any host fallback.
    prefix = backends.prelude(route)
    body = "\n".join("case %d: {%s; break;}" % (i, expression(items[i])) for i in indices)
    source = prefix + "#include <ffmt/base.hpp>\n" + PRELUDE
    source += extra_headers([items[i] for i in indices])
    source += "\nHD void execute_case(int index) {\nswitch (index) {\n" + body + "\n}\n}\n"
    return source + backends.launcher(route)


class Campaign:
    def __init__(self, args):
        self.args = args
        self.fmt = Path(args.fmt_dir).resolve()
        oracle = oracle_identity(self.fmt)
        stamp = datetime.datetime.now(datetime.timezone.utc).strftime("%Y%m%dT%H%M%S%fZ")
        self.output = (WORKSPACE / args.output if args.output else WORKSPACE / "artifacts" / args.profile / (args.label + "-" + stamp)).resolve()
        self.output.mkdir(parents=True, exist_ok=False)
        hashes = snapshot(self.output / "source", args.revision)
        self.host_cxx = shlex.split(args.host_cxx or os.environ.get("FFMT_HOST_CXX", "g++"))
        self.fmt = Path(args.fmt_dir).resolve()
        self.manifest = dict(schema_version=1, profile=args.profile, label=args.label, started=stamp,
            source=dict(checkout=str(ROOT), commit=source_git("rev-parse", "HEAD" if args.revision == "WORKTREE" else args.revision),
                        worktree=args.revision == "WORKTREE", dirty=bool(source_git("status", "--porcelain")), files=hashes),
            runner=dict(checkout=str(ROOT), commit=git("rev-parse", "HEAD")),
            oracle=oracle,
            host=dict(name=platform.node(), architecture=platform.machine(), os=platform.platform()),
            environment={k: v for k, v in os.environ.items() if k in ("LC_ALL", "OMP_TARGET_OFFLOAD", "ONEAPI_DEVICE_SELECTOR", "ACPP_VISIBILITY_MASK", "CHIP_BE", "CHIP_DEVICE_TYPE", "CHIP_SKIP_UNINIT")},
            configurations=[])
        self.manifest["fixture_files"] = fixtures.source_hashes()
        self.manifest["corpus_sha256"] = hashlib.sha256(json.dumps(self.manifest["fixture_files"], sort_keys=True).encode()).hexdigest()
        self.manifest["selection"] = dict(
            case_ids=[x["id"] for x in fixtures.CASES if not args.case or x["id"] in args.case],
            routes=args.route or args.profiles_config["profiles"][args.profile]["routes"],
            optimizations=args.opt or [0, 2], numeric_samples=0,
            memory=False, negative=False, shard_size=args.shard_size)
        self.manifest["runner_files"] = {str(path.relative_to(ROOT)): hashlib.sha256(path.read_bytes()).hexdigest()
            for path in sorted((ROOT / "tools").glob("*.py"))}
        self.manifest["timeouts"] = dict(compile=args.compile_timeout, execution=args.run_timeout, cold_jit=args.jit_timeout)
        self.manifest["invocation"] = [sys.executable, *sys.argv]
        self.manifest["workspace"] = str(WORKSPACE)
        if args.profiles:
            profiles = Path(args.profiles).resolve()
            self.manifest["profiles"] = dict(path=str(profiles), sha256=hashlib.sha256(profiles.read_bytes()).hexdigest())
        self.reference = self.output / "oracle"
        self.reference.mkdir()
        self.save()

    def save(self):
        counts = {}
        for config in self.manifest["configurations"]:
            for item in config.get("cases", []) + config.get("negative_cases", []):
                counts[item["status"]] = counts.get(item["status"], 0) + 1
        self.manifest["counts"] = counts
        path = self.output / "manifest.json"
        temporary = path.with_suffix(".tmp")
        temporary.write_text(json.dumps(self.manifest, indent=2, sort_keys=True) + "\n")
        temporary.replace(path)

    def build_oracle(self, items):
        common = self.host_cxx + ["-std=c++20", "-O2", "-fno-fast-math", "-I" + str(self.fmt / "include")]
        obj = self.reference / "fmt.o"
        p = run(common + ["-c", self.fmt / "src/format.cc", "-o", obj], self.reference, "fmt-build", self.args.compile_timeout)
        self.manifest["oracle"]["build"] = record(p)
        if p["exit_code"]:
            raise RuntimeError("oracle object failed: " + p["stderr_file"])
        source = self.reference / "cases.cpp"
        source.write_text(oracle_source(items))
        self.manifest["oracle"]["cases_source_sha256"] = hashlib.sha256(source.read_bytes()).hexdigest()
        p = run(common + [source, obj, "-o", self.reference / "cases"], self.reference, "oracle-build", self.args.compile_timeout)
        self.manifest["oracle"]["cases_build"] = record(p)
        if p["exit_code"]:
            raise RuntimeError("oracle cases failed: " + p["stderr_file"])
        expected = []
        for i, item in enumerate(items):
            p = run([self.reference / "cases", i], self.reference, item["id"], self.args.run_timeout)
            if p["exit_code"]:
                raise RuntimeError("oracle execution failed: " + item["id"])
            expected.append(p["stdout"])
        self.save()
        return expected

    def configuration(self, route, opt, items, expected):
        directory = self.output / (route + "-O" + str(opt))
        directory.mkdir()
        command = compiler(route) + flags(route, opt) + ["-I" + str(self.output / "source/include")]
        config = dict(route=route, optimization=opt, command_prefix=command,
                      cases=[], negative_cases=[])
        self.manifest["configurations"].append(config)
        config["compiler_version"] = record(run(compiler(route) + ["--version"], directory, "compiler-version"))
        for index, item in enumerate(items):
            source = directory / (item["id"] + ".cpp")
            source.write_text(target_source(items, [index], route))
            executable = directory / item["id"]
            build = run(command + [source] + link_flags(route) + ["-o", executable],
                        directory, item["id"] + "-build", self.args.compile_timeout)
            result = dict(id=item["id"], family=item["family"], build=record(build),
                          source_sha256=hashlib.sha256(source.read_bytes()).hexdigest())
            if build["exit_code"]:
                result["status"] = "compile_timeout" if build.get("timeout") else "compile_failure"
            else:
                execution = run([executable, index], directory, item["id"], self.args.run_timeout)
                result["execution"] = record(execution)
                if execution["exit_code"]:
                    result["status"] = "execution_timeout" if execution.get("timeout") else "execution_failure"
                else:
                    result.update(compare_bytes(expected[index], execution["stdout"]))
            config["cases"].append(result)
            self.save()
            print("%s -O%s %s: %s" % (route, opt, item["id"], result["status"]), flush=True)
        config["stdout_status"] = "completed"
        config["status"] = "completed"
        self.save()


def main():
    global SOURCE_ROOT
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--profiles", default=os.environ.get("FFMT_PROFILES_FILE"), help="Optional JSON profile defaults; no site profiles are required")
    parser.add_argument("--profile", default="local", help="Profile label/default routes from --profiles; built-in profile: local")

    parser.add_argument("--route", action="append", choices=ROUTES)
    parser.add_argument("--opt", type=int, choices=(0, 1, 2, 3), action="append")
    parser.add_argument("--revision", default="WORKTREE", choices=("WORKTREE",))
    parser.add_argument("--label", default="head")
    parser.add_argument("--output")
    parser.add_argument("--fmt-dir", default=os.environ.get("FFMT_FMT_DIR", str(WORKSPACE / "third_party/fmt")))
    parser.add_argument("--host-cxx")
    parser.add_argument("--case", action="append", choices=sorted(x["id"] for x in fixtures.CASES))
    parser.add_argument("--shard-size", type=int, default=16)
    parser.add_argument("--compile-timeout", type=int, default=180)
    parser.add_argument("--run-timeout", type=int, default=30)
    parser.add_argument("--jit-timeout", type=int, default=600, help="execution cap for routes that compile device images at launch")
    parser.add_argument("--no-negative", action="store_true", help=argparse.SUPPRESS)
    parser.add_argument("--no-memory", action="store_true", help=argparse.SUPPRESS)
    parser.add_argument("--numeric-samples", type=int, default=0, help=argparse.SUPPRESS)
    args = parser.parse_args()
    try:
        args.profiles_config = load_profiles(args.profiles)
    except (OSError, ValueError) as error:
        parser.error(str(error))
    if args.profile not in args.profiles_config["profiles"]:
        parser.error("unknown profile %s; supply --profiles or use local" % args.profile)
    SOURCE_ROOT = ROOT
    if args.shard_size < 1:
        parser.error("--shard-size must be positive")
    if args.numeric_samples < 0:
        parser.error("--numeric-samples must be nonnegative")
    os.environ["LC_ALL"] = "C"
    os.environ["OMP_TARGET_OFFLOAD"] = "MANDATORY"
    os.environ["FFMT_PROFILE"] = args.profile
    items = [x for x in fixtures.CASES if not args.case or x["id"] in args.case]
    if not items:
        parser.error("no cases selected")
    campaign = Campaign(args)
    try:
        expected = campaign.build_oracle(items)
        for route in args.route or args.profiles_config["profiles"][args.profile]["routes"]:
            for opt in args.opt or (0, 2):
                campaign.configuration(route, opt, items, expected)
        campaign.manifest["finished"] = datetime.datetime.now(datetime.timezone.utc).isoformat()
    except Exception as e:
        campaign.manifest["fatal_error"] = str(e)
        raise
    finally:
        campaign.save()
        print("Manifest: " + str(campaign.output / "manifest.json"), flush=True)
    bad = any(status not in ("pass", "expected_rejection") for status in campaign.manifest["counts"])
    bad |= any(c.get("numeric_memory", {}).get("status", "pass") != "pass" for c in campaign.manifest["configurations"])
    bad |= any(x["status"] != "expected_rejection" for x in campaign.manifest["oracle"].get("negative_cases", []))
    return int(bad)

if __name__ == "__main__":
    sys.exit(main())
