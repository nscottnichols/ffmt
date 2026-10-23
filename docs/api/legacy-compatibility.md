# Adapt the existing static formatter protocol

Existing ffmt formatters return a literal and a tuple from static format functions.

```cpp
static auto format(point p) { return formatted<"({}, {})", int, int>{{p.x, p.y}}; }
```

Legacy values expand through the counted engine. Narrow nested literals are converted for wide destinations without allocating.

Legacy custom formatters accept empty specs; modern parse/context formatters remain the spec-aware interface.
