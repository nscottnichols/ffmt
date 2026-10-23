# Qualify immutable headers from an independent source checkout

Re-running a comparison against a moving worktree can silently change the source under test.

--source-root ../ffmt --revision COMMIT snapshots that commit even when its working headers have subsequent edits.

Snapshot include/ through git archive for pinned revisions, or copy current headers for WORKTREE; identify runner checkout separately from library checkout.

Only headers belong to the library snapshot. Exact runner/fixture hashes and compiler commands remain separate evidence, not an assertion of a clean worktree.

--revision COMMIT snapshots immutable headers. --source-root CHECKOUT separates the tested library checkout from the portable runner; manifests retain both revisions and all captured hashes.

Validation:

```sh
python3 -m unittest discover -s test/conformance -p test_snapshots.py
python3 tools/conformance.py --revision HEAD --case literal --route host-buffer --opt 2 --no-memory --no-negative
```
