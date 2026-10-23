# Independent host conformance in ready CI

The sanitizer matrix retains its focused C++ and Python checks. It also runs the
portable differential driver for both host compatibility routes at the matrix's
optimization level. The independent oracle uses the immutable fmt checkout;
programs using ffmt compare complete stdout bytes and framed numeric memory
records, including required/written counts and errors.

```sh
python3 tools/conformance.py --route host-buffer --route host-specifiers \
  --opt 2 --host-cxx g++ --fmt-dir third_party/fmt
```

The corpus includes the feature modules present in this complete checkout.
Cases are selected with their implementations; earlier feature PRs keep their
own focused portable regressions. Missing/failed/unsupported records make the
host conformance step fail rather than count as passes. The corpus is a useful
sample of fmt behavior, not exhaustive compatibility.

The driver keeps commands, source snapshots, reference identity, diagnostics and
results under `artifacts/ci-conformance/`. CI uploads that directory after success
or failure when the job has not been cancelled. A cancelled run is incomplete
evidence. Focused Make tests use ASan/UBSan; this separate driver uses its own
explicit compiler flags and is not implicitly sanitized by EXTRA_CXXFLAGS.

Draft PR compiler jobs remain skipped. `ready_for_review` starts the normal
host/device jobs for a ready wave; this change does not mark later waves ready.
No scheduler or evaluation-repository profile is required.
