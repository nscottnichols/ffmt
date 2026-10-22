#!/usr/bin/env python3
"""Publish compact qualification evidence from completed differential runs."""
import argparse
from collections import Counter
import hashlib
import json
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def read_log(config, key, stream="stdout"):
    name = config.get(key, {}).get(stream + "_file")
    return (ROOT / name).read_text(errors="replace") if name and (ROOT / name).is_file() else None


def summarize(path):
    raw = path.read_bytes()
    m = json.loads(raw)
    configurations = []
    for c in m["configurations"]:
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
            status=c.get("status", "incomplete"), device=c.get("device"),
            compiler_version=read_log(c, "compiler_version"),
            command_prefix=c.get("command_prefix"), control_build=c.get("control_build"),
            control_run=c.get("control_run"),
            positive_counts=dict(Counter(x["status"] for x in cases)),
            positive_executions=sum("execution" in x and x["execution"]["exit_code"] == 0 for x in cases) if c.get("status") == "completed" else 0,
            unique_logical_cases=len({x["id"] for x in cases}),
            negative_counts=dict(Counter(x["status"] for x in c.get("negative_cases", []))),
            numeric_memory=c.get("numeric_memory"),
            cases=compact, negative_cases=c.get("negative_cases", [])))
    return dict(profile=m["profile"], label=m["label"], started=m["started"], finished=m.get("finished"),
                source=m["source"], oracle=m["oracle"], host=m["host"], environment=m.get("environment"),
                input_manifest=str(path.relative_to(ROOT)), input_sha256=hashlib.sha256(raw).hexdigest(),
                configurations=configurations)


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("--label", required=True)
    p.add_argument("--output", required=True, help="output basename within repository")
    p.add_argument("--include-incomplete", action="store_true")
    a = p.parse_args()
    runs = []
    for path in sorted((ROOT / "artifacts").glob("*/" + a.label + "-*/manifest.json")):
        m = json.loads(path.read_text())
        if m.get("finished") or a.include_incomplete:
            runs.append(summarize(path))
    target = ROOT / a.output
    target.parent.mkdir(parents=True, exist_ok=True)
    target.with_suffix(".json").write_text(json.dumps(dict(schema_version=1, runs=runs), indent=2, sort_keys=True) + "\n")
    lines = ["# " + a.label + " qualification", "", "Exact byte comparisons against the pinned host fmt oracle. Counts are executions per configuration; logical case IDs remain stable across configurations.", "",
        "A failed positive control means cases were not exercised. Unsupported or compile-failing cases are not counted as executions. Hardware availability and compiler failures are separate from output agreement.", "",
        "| Profile | Route | Opt | Executed | Exact | Mismatch | Unsupported | Other failures | Negative rejection |", "|---|---|---:|---:|---:|---:|---:|---:|---:|"]
    for run in runs:
        for c in run["configurations"]:
            counts = c["positive_counts"]
            other = sum(v for k, v in counts.items() if k not in ("pass", "mismatch", "unsupported"))
            negatives = c["negative_counts"]
            lines.append("| %s | %s | O%d | %d | %d | %d | %d | %d | %d/%d |" %
                (run["profile"], c["route"], c["optimization"], c["positive_executions"], counts.get("pass", 0), counts.get("mismatch", 0), counts.get("unsupported", 0), other, negatives.get("expected_rejection", 0), sum(negatives.values())))
    lines += ["", "## Observed gaps", ""]
    for run in runs:
        for c in run["configurations"]:
            failures = [x for x in c["cases"] if x["status"] != "pass"]
            if not failures:
                continue
            label = "%s / %s / O%d" % (run["profile"], c["route"], c["optimization"])
            if c["status"] == "infrastructure_failure":
                lines.append("- **%s:** positive control failed; no conformance claim. See control diagnostics in the JSON record." % label)
            else:
                grouped = {}
                for item in failures:
                    grouped.setdefault(item["status"], []).append(item["id"])
                for status, ids in grouped.items():
                    lines.append("- **%s, %s:** %s." % (label, status, ", ".join(ids)))
    lines += ["", "The companion JSON contains compiler/device identity, source file hashes, commands, per-case outcomes, and hashes/paths for raw evidence retained under artifacts/. Code coverage and performance are measured separately.", ""]
    target.with_suffix(".md").write_text("\n".join(lines))
    print("Published %d runs to %s.{json,md}" % (len(runs), target))


if __name__ == "__main__":
    main()
