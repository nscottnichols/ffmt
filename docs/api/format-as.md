# Map user values through ADL format_as

Small wrappers should reuse primitive formatting without a full formatter specialization.

```cpp
constexpr auto format_as(status value) { return value.code; }
```

ADL mapping participates in parsing, output and dynamic width validation. Values retain references until mapping occurs.

Mappings terminate at primitive representations; they are not a recursive serialization interface.
