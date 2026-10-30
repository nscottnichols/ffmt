# Maintaining tests

Use the [portable test guide](../CONFORMANCE.md) for commands and the
[maintenance inventory](../MAINTENANCE.md) for ownership and retained components.

Add shared text cases to the original `test/test_*.cpp` bodies. Put direct state,
iterator, numeric or error assertions under `test/direct/` and register them once
in `test/direct/tests.mk`. Make and CMake consume that same inventory; a missing
source must fail. Use exit 77 only for explicitly optional compiler/library types.

Keep the host fmt object independent of offload compilation. Keep literal
rejections in the existing negative corpus with valid and syntax controls.
Campaign source selection, execution profiles and captured evidence belong in
ffmt-evaluation, not another library runner.
