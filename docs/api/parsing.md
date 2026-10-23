# Parse and validate counted format strings

Runtime execution and literal validation need a single grammar.

```cpp
walk_format("{{{:04x}}}", handler); // literal braces and one field
```

A handler receives parsed fields; parse context tracks index modes and checks dynamic references without producing output.

This prefix supports ASCII fill; counted Unicode decoding is introduced with its tests.
