# Header dependencies

The original host/device object rules and direct test rules depend on the library
headers and the same `test/direct/tests.mk` inventory. Editing `base.hpp`, a
compatibility wrapper, or a test registration invalidates cached objects/binaries.
`test/test_infrastructure.py` checks these contracts using real Make timestamps
and dry-run compile plans, without invoking a compiler.
