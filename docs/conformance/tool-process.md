# Capture exact subprocess bytes and terminate timed-out children

Compiler drivers can spawn children that outlive a timeout; text decoding can erase output distinctions.

compare_bytes(b"value\n", b"value") reports a mismatch; an embedded NUL remains observable.

Keep byte capture, process lifetime and evidence-path handling in one small reusable module; save both streams before returning.

Timeout cleanup uses POSIX process groups; its regression is explicitly skipped where killpg is unavailable.

Process results preserve raw stdout/stderr, exit status, command arguments and elapsed time. Timeout cleanup kills the complete compiler process group.

Validation:

```sh
python3 -m unittest discover -s test/conformance -p test_process.py
python3 -m unittest discover -s test/conformance -p test_output.py
```
