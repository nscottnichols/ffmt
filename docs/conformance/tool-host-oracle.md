# Compare host output with an independently compiled fmt oracle

The legacy test harness can share assumptions with the implementation; expected bytes need an independent formatting implementation.

A standalone fmt process formats INT64_MIN; a separate ffmt process must produce the identical bytes.

Compile fmt only into host reference executables, and compile ffmt targets without fmt headers. Begin with baseline cases supported before engine migration.

Individual executables cost compile time; compiler and fmt source identities are recorded. Accelerator routes and future feature cases are absent.

The host differential runner compiles the pinned fmt oracle independently. The reference checkout must be clean, and exact format.h/format.cc hashes are retained; verification never changes the caller checkout. Feature fixture modules are introduced with implementations; every loaded fixture file receives a source hash.

Validation:

```sh
python3 -m unittest discover -s test/conformance -p test_*.py
python3 tools/conformance.py --route host-buffer --opt 2 --no-memory --no-negative --fmt-dir third_party/fmt
```
