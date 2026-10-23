# Apply counted Unicode decoding and display rules

Byte counts alone misalign multibyte text, and decoding must respect counted boundaries.

```cpp
format_to(buffer, "{:─^9}", "é");
```

Counted UTF-8 decoding, display-width tables and printability tables govern fill, alignment, precision and debug escapes. Wide output follows code-unit semantics.

These are qualified fmt semantics, not terminal-specific grapheme measurement. Unicode can still be disabled explicitly.
