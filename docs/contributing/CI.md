# Ready review waves and portable CI

Draft PRs are published for dependency navigation and local validation evidence.
The expensive host and device compile jobs start when a PR becomes ready, using
the `ready_for_review` event. Pushes to main keep running CI.

A skipped draft job is not a passing conformance result. Every published
intermediate must already have local portable validation evidence; marking a
wave ready adds the normal hosted compiler/OS checks. Later review waves can
remain draft without spending runner time on every dependency simultaneously.

The ready host matrix runs `make test-portable`: original shared-case host
comparisons, direct assertions, negative controls, infrastructure checks and the
isolated single-header consumer. The CMake job reads the same test inventory.
Device jobs compile the original shared cases. Snapshot campaigns and evidence
export run in ffmt-evaluation rather than a second library CI runner.
