---
name: ffmt-jlse-development
description: Implement, test, and qualify the ffmt working fork on JLSE Cobalt resources. Use for ffmt engine, conformance, toolchain, benchmark, or evidence tasks in this project; not for unrelated clusters.
---

# ffmt on JLSE

The checkout is `/vast/projects/datascience/nnichols/ffmt_project/ffmt`.
Read `docs/MILESTONES.md` there before work; it is the source of truth for scope,
accepted API choices, milestone status, ownership, and external prerequisites.
The guidance input lives in the sibling `ffmt_outline` directory.

Use `platforms/profiles.json` and `tools/jlse/` for system selection and job
submission. JLSE is Cobalt (`qsub --mode script -q QUEUE -n 1 -t MINUTES --cwd /home/nnichols /bin/bash SCRIPT ARGS`),
not PBS. Read `qsub --help` if changing submission arguments. Obtain allocations
before compute-node execution. Store installations in ignored
`software/<system-id>/{downloads,src,build,install}`, never system directories.

Run `python3 tools/conformance.py --help` for the independent differential runner.
Its pinned fmt is the oracle; preserve raw bytes and complete expectations.
Compile errors, skips and unavailable hardware must never become passing cases.
Record the actual device and execution errors; prevent host fallback.
Use separate artifact paths per worker and configuration. Test defects with
minimized regressions and test harness changes with malformed-output fixtures.
Use `--revision COMMIT` for final qualification. A WORKTREE run is development
evidence; match its captured header hashes to an immutable commit before linking
it to a release claim. Keep a fallback node's actual device name visible even
when the queue profile retains the primary system's name.

`tools/jlse/host_tests.py` runs strict/sanitized focused suites and records source
hashes. `tools/jlse/build_checks.sh PROFILE COMMIT --cxx COMPILER --coverage`
checks an isolated committed tree with CMake/CTest, Make, invalid-format controls
and source coverage. Submit it through Cobalt, as with other compute work.
Publish explicitly selected completed runs with `tools/report.py`; use
`tools/benchmark_report.py` for measured groups and `tools/paper_tables.py` for
tables plus their selection ledger. Retain earlier failures when a retry passes.

Workers follow their assigned file ownership. Send the orchestrator interface
changes and validation commands. Do not change branches, commit, reset, or include
another worker's changes unless explicitly assigned integration. PRs and pushes
belong to the orchestrator and target `nscottnichols/ffmt`.

For GitHub CLI invocations set `GOMAXPROCS=2` on JLSE to avoid Go thread pressure.
Inspect only selected environment fields in evidence, never credential values.
