# Format native 128-bit and bit-precise integers

Standard type traits do not consistently recognize native extended integers.

```cpp
using U = unsigned __int128;
format_to(buffer, "{:#x}", U(1) << 100);
```

Explicit signed and unsigned traits preserve the native width. _BitInt support follows the oracle surface through 64 bits.

Compiler extensions are conditional. Wider _BitInt values report unsupported rather than narrowing.
