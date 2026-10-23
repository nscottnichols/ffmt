# Compare invalid literal rejection against the independent oracle

A source snapshot can reject valid formatting output correctly while silently accepting an invalid literal.

The unclosed "{" fixture must produce an intended format diagnostic in both the target compiler and the host fmt oracle.

Reuse the diagnostic classifier for revisioned negative fixture modules and preserve both compiler diagnostics separately.

This adds compile-time rejection parity, not runtime parser exception parity. The baseline fixture registry has no new negative cases until engine migration.

Revisioned negative fixtures accompany parser behavior and run through both the independent host fmt oracle and the selected target compiler.

Validation:

```sh
python3 -m unittest discover -s test/conformance -p test_negative_driver.py
```
