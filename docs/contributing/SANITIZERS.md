# Host sanitizer policy

Ready host CI runs AddressSanitizer and the complete UndefinedBehaviorSanitizer
set with `EXTRA_CXXFLAGS=-fsanitize=address,undefined`. Errors stop the test run.

The former workflow disabled bool validation globally using
`-fno-sanitize=bool` for the old std::format-based test path. The checked engine
and pinned independent fmt reference replace that path, so the exception is
removed. A bool problem in library or reference code now fails the job. A future
toolchain-specific issue needs its own reproducer and review instead of silently
restoring a broad exception.

The compiler matrix, test commands, draft/ready behavior and coverage reporting
policy are unchanged by this sanitizer setting.
