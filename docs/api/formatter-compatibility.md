# Support parse/context formatters and correct forwarding

User-defined values need to consume specs and write into the caller context.

```cpp
struct formatter<point> : formatter<int> { /* parse and format */ };
```

Eligibility checks inspect the cv-qualified reference accepted by format. Shared sink iterators preserve nested counts without copying user objects.

The protocol is template based with checked statuses; it does not provide exception services or type-erased stores.
