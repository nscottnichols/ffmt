# Activate public engine differential cases at migration

The new public engine needs transport, capacity, Unicode and floating-point cases beyond the old implementation baseline.

A 300-character padded field must compare against all 300 oracle bytes; a missing newline or embedded NUL is significant.

Introduce these public-entry-point fixtures with engine migration. Earlier internal engine steps already carry focused C++ comparisons against fmt.

This corpus is representative, not exhaustive; extension-specific integer and floating-point coverage remains in focused C++ tests.

Public engine cases are introduced at migration, after focused C++ oracle tests qualify individual internal engine steps.

Validation:

```sh
python3 tools/conformance.py --route host-buffer --route host-specifiers --opt 2 --no-memory
```
