# Coverage is reported, not a percentage regression gate

This policy **replaces the blocking region-coverage regression gate with
reporting**. Previously CI failed if the head percentage fell more than
0.01 percentage points below its base. CI now checks that measurement is valid
and nonempty, reports base/head region, executable-line and branch-edge metrics,
and uploads per-file JSON for review. A percentage drop no longer blocks merging.

Moving code between headers and changing template instantiation can change the
instrumented denominator even when test behavior is unchanged. A single aggregate
percentage cannot reliably distinguish this from a lost regression test. The
tradeoff is real: a genuine coverage regression can pass CI and requires a human
to inspect the per-file reports. Correctness, diagnostic and sanitizer checks
remain blocking. A future stable per-file baseline could restore a meaningful
regression gate; this change does not claim equivalent automatic protection.

Run `make CXX=clang++ coverage` to produce the current report. Device-only code,
compile-time evaluation and uninstantiated templates are excluded.

Coverage also executes explicitly registered mandatory runtime suites. Optional
compiler-extension checks and fast-math variants retain separate qualification
results and do not make report collection depend on optional compiler support.
