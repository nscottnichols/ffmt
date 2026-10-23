# Ready review waves and portable CI

Draft PRs are published for dependency navigation and local validation evidence.
The expensive host and device compile jobs start when a PR becomes ready, using
the `ready_for_review` event. Pushes to main keep running CI.

A skipped draft job is not a passing conformance result. Every published
intermediate must already have local portable validation evidence; marking a
wave ready adds the normal hosted compiler/OS checks. Later review waves can
remain draft without spending runner time on every dependency simultaneously.
