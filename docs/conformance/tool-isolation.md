# Preserve failed batches before isolating individual cases

A combined switch can fail although individual cases succeed, and retrying without saving it erases the failure.

When a two-case binary crashes, save its source hash and diagnostic before retrying one-case binaries.

Require a positive execution control, recursively split failed build batches, and isolate remaining cases after runtime failures. Save each checkpoint atomically.

Retries increase compile time and isolated passes do not erase batch failures. A failed control produces not_run outcomes, not a formatting judgment.

Failed batch builds and executions remain in manifest shard_failures before per-case isolation. Positive controls separate missing execution capability from formatting outcomes.

Validation:

```sh
python3 -m unittest discover -s test/conformance -p test_runner.py
python3 tools/conformance.py --route host-buffer --opt 2 --no-memory --no-negative --shard-size 2
```
