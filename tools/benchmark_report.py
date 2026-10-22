#!/usr/bin/env python3
"""Export compact measured evidence from explicitly selected benchmark runs."""
import argparse
import hashlib
import json
from pathlib import Path


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("manifests", nargs="+", type=Path)
    p.add_argument("--output", type=Path)
    a = p.parse_args()
    result = {"schema_version": 1, "runs": []}
    for path in a.manifests:
        raw = path.read_bytes()
        run = json.loads(raw)
        compact = {k: run.get(k) for k in ["profile", "revision", "source_files", "benchmark_sources", "machine",
                                           "cobalt_jobid", "hostname", "started", "finished", "source_commit", "discovery", "compiler_versions",
                                           "fmt_commit", "timer_policy", "sycl_cache_persistent", "sycl_jit_policy", "warmups", "samples"]}
        compact.update(raw_manifest=str(path), raw_manifest_sha256=hashlib.sha256(raw).hexdigest())
        # Older campaign runs predate the inline discovery link. Attach the
        # matching immutable job discovery file without rewriting raw evidence.
        discovery = path.parent.parent / ("discovery-"+str(run.get("cobalt_jobid"))+".json")
        if not compact.get("discovery") and discovery.exists():
            compact["discovery"] = dict(path=str(discovery), sha256=hashlib.sha256(discovery.read_bytes()).hexdigest())
        compact["results"] = [{k: v for k, v in row.items() if k not in ("execution", "verification")}
                              for row in run["results"]]
        for row in compact["results"]:
            if "cold_ns" in row.get("metrics", {}):
                row["metrics"] = dict(row["metrics"])
                row["metrics"]["first_batch_ns"] = row["metrics"].pop("cold_ns")
                row["first_batch_note"] = "Historical cold_ns field measured the first batch after a separate validation process; persistent cache state was unspecified"
        compact["builds"] = [{k: v for k, v in row.items() if k not in ("events", "executable", "resources")}
                             for row in run["builds"]]
        for original, saved in zip(run["builds"], compact["builds"]):
            saved["build_commands"] = [event["command"] for event in original.get("events", []) if "command" in event]
        compact["resource_probes"] = run.get("resource_probes", [])
        result["runs"].append(compact)
    text = json.dumps(result, indent=2) + "\n"
    if a.output:
        a.output.parent.mkdir(parents=True, exist_ok=True)
        a.output.write_text(text)
    else:
        print(text, end="")


if __name__ == "__main__":
    main()
