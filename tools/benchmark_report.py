#!/usr/bin/env python3
"""Export compact measured evidence from explicitly selected benchmark runs."""
import argparse
import csv
import hashlib
import io
import json
from pathlib import Path
import re


def hardware_identity(run, discovery):
    """Use recorded hardware evidence, never the scheduler profile's nickname."""
    evidence = {}
    identity = {"hostname": run.get("hostname"), "architecture": run.get("machine"),
                "cpu_model": None, "discovered_accelerators": []}
    if discovery and discovery.get("path"):
        path = Path(discovery["path"])
        if not path.is_absolute():
            path = Path(__file__).resolve().parents[1] / path
        if path.exists():
            raw = path.read_bytes()
            if discovery.get("sha256") == hashlib.sha256(raw).hexdigest():
                evidence = json.loads(raw)
            else:
                identity["discovery_error"] = "Discovery file SHA-256 does not match the recorded evidence"
    identity["hostname"] = identity["hostname"] or evidence.get("hostname")
    identity["architecture"] = identity["architecture"] or evidence.get("machine")
    commands = evidence.get("commands", {})
    cpu = commands.get("lscpu", {}).get("stdout", "")
    model = re.search(r"^Model name:\s*(.+)$", cpu, re.M)
    if model:
        identity["cpu_model"] = model.group(1).strip()
    accelerators = identity["discovered_accelerators"]
    nvidia = commands.get("nvidia-query", {})
    if nvidia.get("returncode") == 0:
        for item in csv.DictReader(io.StringIO(nvidia.get("stdout", ""))):
            item = {key.strip(): value.strip() for key, value in item.items() if key and value}
            if item.get("name"):
                accelerators.append(dict(api="cuda", name=item["name"], driver=item.get("driver_version"),
                                         architecture=item.get("compute_cap"), pci_bus_id=item.get("pci.bus_id")))
    rocminfo = commands.get("rocminfo", {})
    if rocminfo.get("returncode") == 0:
        for block in re.split(r"^Agent\s+\d+.*$", rocminfo.get("stdout", ""), flags=re.M):
            arch = re.search(r"^\s+Name:\s+(gfx\w+)\s*$", block, re.M)
            name = re.search(r"^\s+Marketing Name:\s+(.+?)\s*$", block, re.M)
            if arch and name:
                accelerators.append(dict(api="hip", name=name.group(1), architecture=arch.group(1)))
    sycl = commands.get("sycl-ls", {})
    if sycl.get("returncode") == 0:
        for line in sycl.get("stdout", "").splitlines():
            if ":gpu]" in line:
                description = line.split("]", 1)[1].strip()
                name = description.split(", ", 1)[-1]
                name = re.sub(r" \d+\.\d+\.\d+ \[.*\]$", "", name)
                accelerators.append(dict(api="sycl", name=name, description=description))
    # Multiple equivalent agents may describe one model; retain model identity
    # without claiming an execution used every enumerated device.
    unique = {json.dumps(x, sort_keys=True): x for x in accelerators}
    identity["discovered_accelerators"] = list(unique.values())
    return identity


def device_label(names, hardware):
    labels = []
    for name in names:
        if name == "host":
            name = hardware.get("cpu_model") or "host"
            if hardware.get("architecture"):
                name += " (" + hardware["architecture"] + ")"
        else:
            architectures = {x["architecture"] for x in hardware["discovered_accelerators"]
                             if x["name"] == name and x.get("api") == "hip" and x.get("architecture")}
            if architectures:
                name += " (" + ", ".join(sorted(architectures)) + ")"
        if name not in labels:
            labels.append(name)
    return ", ".join(labels) or None


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
                                           "fmt_commit", "timer_policy", "sycl_cache_persistent", "sycl_jit_policy", "warmups", "samples", "selection",
                                           "source_worktree", "runner_commit", "runner_files", "environment"]}
        if "source_worktree" not in run:
            compact["source_identity_note"] = "Historical source_commit recorded the runner checkout HEAD; use revision and source_files for the captured library identity"
        compact.update(raw_manifest=str(path), raw_manifest_sha256=hashlib.sha256(raw).hexdigest())
        compact["source_fingerprint_sha256"] = hashlib.sha256(
            json.dumps(run.get("source_files", {}), sort_keys=True, separators=(",", ":")).encode()).hexdigest()
        # Older campaign runs predate the inline discovery link. Attach the
        # matching immutable job discovery file without rewriting raw evidence.
        discovery = path.parent.parent / ("discovery-"+str(run.get("cobalt_jobid"))+".json")
        if not compact.get("discovery") and discovery.exists():
            compact["discovery"] = dict(path=str(discovery), sha256=hashlib.sha256(discovery.read_bytes()).hexdigest())
        compact["hardware"] = hardware_identity(run, compact.get("discovery"))
        compact["results"] = [{k: v for k, v in row.items() if k not in ("execution", "verification")}
                              for row in run["results"]]
        for row in compact["results"]:
            row.setdefault("presentation", "explicit_e8" if row.get("family") == "float" else "integer_d")
            row["actual_device"] = device_label(row.get("metrics", {}).get("device", []), compact["hardware"])
            if "cold_ns" in row.get("metrics", {}):
                row["metrics"] = dict(row["metrics"])
                row["metrics"]["first_batch_ns"] = row["metrics"].pop("cold_ns")
                row["first_batch_note"] = "Historical cold_ns field measured the first batch after a separate validation process; persistent cache state was unspecified"
        compact["builds"] = [{k: v for k, v in row.items() if k not in ("events", "executable", "resources")}
                             for row in run["builds"]]
        for original, saved in zip(run["builds"], compact["builds"]):
            saved["build_commands"] = [event["command"] for event in original.get("events", []) if "command" in event]
        compact["resource_probes"] = run.get("resource_probes", [])
        for row in compact["resource_probes"]:
            row.setdefault("presentation", "explicit_e8" if row.get("family") == "float" else "integer_d")
            row["execution_scope"] = "compile_only"
            row["target_architectures"] = sorted(set(re.findall(
                r"\b(?:sm_\d+|gfx[0-9a-z]+|spirv?64)\b", " ".join(row.get("compile", {}).get("command", [])))))
        compact["actual_devices"] = sorted({row["actual_device"] for row in compact["results"] if row["actual_device"]})
        result["runs"].append(compact)
    text = json.dumps(result, indent=2) + "\n"
    if a.output:
        a.output.parent.mkdir(parents=True, exist_ok=True)
        a.output.write_text(text)
    else:
        print(text, end="")


if __name__ == "__main__":
    main()
