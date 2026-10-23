# Verify benchmark output before collecting timing samples

Fast timing numbers are meaningless if the candidate truncates output or executes a different workload.

Before measuring integer memory formatting, compare its emitted verification bytes with a separate pinned fmt executable.

Snapshot sources and benchmark primitives, compare output before timing, validate sample counts and checksums, and retain exact commands and source identities.

Host wall timing includes launch and synchronization for accelerators. Verification process time and first batch are recorded separately; neither is a cold-JIT claim.

Output verification precedes timing. Manifests retain source snapshots, runner files, exact compiler commands and timer policy; campaign conclusions belong in ffmt-evaluation.

Validation:

```sh
python3 -m unittest discover -s test/conformance -p test_benchmark.py
python3 tools/benchmark.py --route host-buffer --family integer --mode memory --capacity 128 --memory-iterations 128
```
