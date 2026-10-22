#!/usr/bin/env python3
"""Submit isolated one-node JLSE work and retain the exact Cobalt command."""
import argparse
import datetime
import json
from pathlib import Path
import shlex
import subprocess

ROOT = Path(__file__).resolve().parents[2]


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("stage", choices=["discovery", "diagnostics", "baseline", "head", "provision", "benchmark"])
    p.add_argument("profiles", nargs="+")
    p.add_argument("--minutes", type=int)
    p.add_argument("--revision", default="3fe258a87675c997667c899c2b0f0cad405aad5a")
    p.add_argument("--queue", help="Override queue; requires exactly one profile")
    p.add_argument("--dry-run", action="store_true")
    p.add_argument("--extra", nargs=argparse.REMAINDER, default=[])
    a = p.parse_args()
    profiles = json.loads((ROOT / "platforms/profiles.json").read_text())["profiles"]
    if a.queue and len(a.profiles) != 1:
        p.error("--queue requires one profile")
    records = ROOT / "artifacts/submissions"
    if not a.dry_run:
        records.mkdir(parents=True, exist_ok=True)
    for profile in a.profiles:
        if profile not in profiles or not profiles[profile]["queue"]:
            p.error(f"Unknown schedulable profile: {profile}")
        minutes = a.minutes or (30 if a.stage == "discovery" else 120)
        # The Cobalt service host cannot open /vast output files even though
        # allocated compute nodes can. Keep launcher I/O on shared /home.
        scheduler_dir = Path.home() / ".cache/ffmt-cobalt" / profile
        prefix = scheduler_dir / f"cobalt-{a.stage}-{datetime.datetime.now(datetime.timezone.utc):%Y%m%dT%H%M%S}"
        cmd = ["qsub", "-q", a.queue or profiles[profile]["queue"], "-n", "1", "-t", str(minutes), "--mode", "script",
               "--cwd", str(Path.home()), "-O", str(prefix), "/bin/bash", str(ROOT / "tools/jlse/job.sh"),
               profile, a.stage, a.revision, *a.extra]
        print(shlex.join(cmd), flush=True)
        if a.dry_run:
            continue
        prefix.parent.mkdir(parents=True, exist_ok=True)
        r = subprocess.run(cmd, capture_output=True, text=True)
        record = {"profile": profile, "stage": a.stage, "command": cmd, "scheduler_output_prefix": str(prefix),
                  "returncode": r.returncode, "stdout": r.stdout, "stderr": r.stderr,
                  "timestamp_utc": datetime.datetime.now(datetime.timezone.utc).isoformat()}
        jobid = r.stdout.strip().splitlines()[-1] if r.stdout.strip() else "failed"
        (records / f"{profile}-{a.stage}-{jobid}.json").write_text(json.dumps(record, indent=2) + "\n")
        print(r.stdout, end="")
        print(r.stderr, end="")
        if r.returncode:
            raise SystemExit(r.returncode)


if __name__ == "__main__":
    main()
