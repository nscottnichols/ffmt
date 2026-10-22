#!/usr/bin/env python3
"""Publish compact qualification evidence from completed differential runs."""
import argparse
from collections import Counter
import hashlib
import json
from pathlib import Path
from benchmark_report import hardware_identity

ROOT = Path(__file__).resolve().parents[1]


def device_label(run, config):
    """Keep actual hardware visible when a queue profile uses a fallback node."""
    device = config.get("device") or "unavailable"
    if " name=" in device:
        device = device.split(" name=", 1)[1].split(" arch=", 1)[0].split(" fp64=", 1)[0]
    elif device == "host" or device == "kokkos space=Serial":
        device = "host " + run["host"]["architecture"]
    elif device != "unavailable":
        api = "cuda" if "cuda" in config["route"] else "hip" if "hip" in config["route"] else "sycl"
        names = {x["name"] for x in run.get("hardware", {}).get("discovered_accelerators", []) if x["api"] == api}
        if len(names) == 1:
            device = next(iter(names)) + " [" + device + "]"
    return device.replace("|", "\\|").replace("\n", " ")


def source_label(run):
    source = run["source"]
    return (source.get("commit") or "unknown")[:8] + ("+W" if source.get("worktree") else "")


def read_log(config, key, stream="stdout"):
    name = config.get(key, {}).get(stream + "_file")
    return (ROOT / name).read_text(errors="replace") if name and (ROOT / name).is_file() else None


def summarize(path, terminal_only=False):
    raw = path.read_bytes()
    m = json.loads(raw)
    configurations = []
    unfinished = []
    for c in m["configurations"]:
        interrupted = bool(m.get("fatal_error")) and c is m["configurations"][-1]
        if terminal_only and (not c.get("status") or interrupted):
            unfinished.append(dict(route=c["route"], optimization=c["optimization"],
                recorded_cases=len(c.get("cases", [])), status="not_completed",
                reported_status=c.get("status"), stdout_status=c.get("stdout_status"),
                positive_counts=dict(Counter(x["status"] for x in c.get("cases", []))),
                numeric_memory=c.get("numeric_memory"), fatal_error=m.get("fatal_error") if interrupted else None))
            continue
        cases = c.get("cases", [])
        compact = []
        for case in cases:
            entry = {k: v for k, v in case.items() if k not in ("build", "execution")}
            for stage in ("build", "execution"):
                if stage not in case:
                    continue
                entry[stage] = {k: case[stage][k] for k in ("exit_code", "stdout_file", "stderr_file") if k in case[stage]}
                for stream in ("stdout", "stderr"):
                    file = case[stage].get(stream + "_file")
                    if file and (ROOT / file).is_file():
                        entry[stage][stream + "_sha256"] = hashlib.sha256((ROOT / file).read_bytes()).hexdigest()
            compact.append(entry)
        configurations.append(dict(route=c["route"], optimization=c["optimization"],
            status=c.get("status", "incomplete"), stdout_status=c.get("stdout_status"), device=c.get("device"),
            compiler_version=read_log(c, "compiler_version"),
            command_prefix=c.get("command_prefix"), control_build=c.get("control_build"),
            control_run=c.get("control_run"),
            shard_failures=c.get("shard_failures", []),
            positive_counts=dict(Counter(x["status"] for x in cases)),
            positive_execution_attempts=sum("execution" in x for x in cases),
            positive_executions=sum("execution" in x and x["execution"]["exit_code"] == 0 for x in cases) if c.get("status") == "completed" else 0,
            unique_logical_cases=len({x["id"] for x in cases}),
            negative_counts=dict(Counter(x["status"] for x in c.get("negative_cases", []))),
            numeric_memory=c.get("numeric_memory"),
            cases=compact, negative_cases=c.get("negative_cases", [])))
    termination = path.parent / "termination.json"
    termination_record = dict(evidence=json.loads(termination.read_text()),
        sha256=hashlib.sha256(termination.read_bytes()).hexdigest()) if termination.is_file() else None
    return dict(profile=m["profile"], label=m["label"], started=m["started"], finished=m.get("finished"),
                source=m["source"], oracle=m["oracle"], host=m["host"], environment=m.get("environment"),
                selection=m.get("selection"), termination=termination_record,
                fatal_error=m.get("fatal_error"),
                unfinished_configurations=unfinished,
                input_manifest=str(path.relative_to(ROOT)), input_sha256=hashlib.sha256(raw).hexdigest(),
                corpus_sha256=m.get("corpus_sha256"), timeouts=m.get("timeouts"),
                runner_files=m.get("runner_files"),
                cobalt_jobid=m.get("cobalt_jobid"), discovery=m.get("discovery"),
                hardware=hardware_identity(dict(hostname=m["host"].get("name"), machine=m["host"].get("architecture")), m.get("discovery")),
                configurations=configurations)


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("--label", required=True)
    p.add_argument("--output", required=True, help="output basename within repository")
    p.add_argument("--include-incomplete", action="store_true")
    p.add_argument("--terminal-configurations-only", action="store_true", help="Include terminal configurations from partial campaigns, retaining failed controls and explicit unfinished scope")
    p.add_argument("--manifest", type=Path, action="append", help="Select exact runs instead of scanning the label")
    a = p.parse_args()
    runs = []
    for path in a.manifest or sorted((ROOT / "artifacts").glob("*/" + a.label + "-*/manifest.json")):
        path = path.resolve()
        m = json.loads(path.read_text())
        if m.get("finished") or a.include_incomplete or a.terminal_configurations_only:
            runs.append(summarize(path, a.terminal_configurations_only))
    target = ROOT / a.output
    target.parent.mkdir(parents=True, exist_ok=True)
    publisher = dict(tool="tools/report.py", sha256=hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),
        hardware_helper_sha256=hashlib.sha256((ROOT / "tools/benchmark_report.py").read_bytes()).hexdigest(),
        terminal_configurations_only=a.terminal_configurations_only, include_incomplete=a.include_incomplete,
        label=a.label)
    target.with_suffix(".json").write_text(json.dumps(dict(schema_version=1, publisher=publisher, runs=runs), indent=2, sort_keys=True) + "\n")
    lines = ["# " + a.label + " qualification", "", "Exact byte comparisons against the pinned host fmt oracle. Completed/attempted counts summarize recorded per-case stdout executions; shared-shard failures are retained separately. Logical case IDs remain stable across configurations. Compile failures and unsupported cases are not execution attempts.", "",
        "A failed positive control means cases were not exercised. Unsupported or compile-failing cases are not counted as executions. Hardware availability and compiler failures are separate from output agreement.", "",
        "Source identifies the captured library commit; +W means captured working-tree headers, whose hashes are retained in JSON.", "",
        "| Profile | Actual device | Route | Opt | Source | Completed/attempted | Exact | Mismatch | Unsupported | Other failures | Negative rejection |", "|---|---|---|---:|---|---:|---:|---:|---:|---:|---:|"]
    for run in runs:
        for c in run["configurations"]:
            counts = c["positive_counts"]
            other = sum(v for k, v in counts.items() if k not in ("pass", "mismatch", "unsupported"))
            negatives = c["negative_counts"]
            lines.append("| %s | %s | %s | O%d | %s | %d/%d | %d | %d | %d | %d | %d/%d |" %
                (run["profile"], device_label(run, c), c["route"], c["optimization"], source_label(run), c["positive_executions"], c["positive_execution_attempts"], counts.get("pass", 0), counts.get("mismatch", 0), counts.get("unsupported", 0), other, negatives.get("expected_rejection", 0), sum(negatives.values())))
    lines += ["", "## Observed gaps", ""]
    for run in runs:
        if not run["finished"]:
            lines.append("- **%s partial campaign:** only the selected terminal configurations are shown; the original run did not finish. Unfinished attempts and termination scope remain in the JSON." % run["profile"])
        for c in run["configurations"]:
            failures = [x for x in c["cases"] if x["status"] != "pass"]
            if not failures:
                continue
            label = "%s / %s / O%d / %s" % (run["profile"], c["route"], c["optimization"], source_label(run))
            if c["status"] in ("infrastructure_failure", "control_compile_failure", "control_execution_failure"):
                lines.append("- **%s:** positive control failed; no conformance claim. See control diagnostics in the JSON record." % label)
            else:
                grouped = {}
                for item in failures:
                    grouped.setdefault(item["status"], []).append(item["id"])
                for status, ids in grouped.items():
                    lines.append("- **%s, %s:** %s." % (label, status, ", ".join(ids)))
    lines += ["", "## Device-memory numeric comparisons", "",
              "Each input record combines five independently referenced formats. Counts are input records, not stdout cases.", "",
              "| Profile | Actual device | Route | Opt | Source | Status | Exact/input records |", "|---|---|---|---:|---|---|---:|"]
    for run in runs:
        for c in run["configurations"]:
            memory = c.get("numeric_memory")
            if memory:
                lines.append("| %s | %s | %s | O%d | %s | %s | %s/%s |" %
                    (run["profile"], device_label(run, c), c["route"], c["optimization"], source_label(run), memory["status"],
                     memory.get("passed", "—"), memory.get("input_cases", "—")))
    lines += ["", "The companion JSON contains compiler/device identity, source file hashes, commands, per-case outcomes, and hashes/paths for raw evidence retained under artifacts/. Code coverage and performance are measured separately.", ""]
    target.with_suffix(".md").write_text("\n".join(lines))
    print("Published %d runs to %s.{json,md}" % (len(runs), target))


if __name__ == "__main__":
    main()
