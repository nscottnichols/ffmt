#ifndef TEST_INC
#define TEST_NAME misc
#define TEST_INC "test_misc.cpp"
#include "test_select_body.inc"
#else

// Combined specs
RUN(PRINT("{:+010d}\n", 42));
RUN(PRINT("{:+15.6f}\n", 3.14));

// Multiple arguments
SKIP_IF(FFMT_PTX_CLANG_O0, "ptx-clang-o0", {
  int a = 1; int b = 2;
  PRINT("{} + {} = {}\n", a, b, a + b);
});
SKIP_IF(FFMT_PTX_CLANG_O0, "ptx-clang-o0",
        PRINT("{} {} {} {} {} {} {}\n", 1, 2, 3, 4, 5, 6, 7));

// Plain text and escaped braces
RUN(PRINT("Hello from GPU!\n"));
RUN(PRINT("{{}} is literal braces\n"));
RUN(PRINT("value={{{}}} done\n", 42));


// Edge cases
RUN(PRINT("{}\n", 0));
RUN(PRINT("{:g}\n", 0.0));
RUN(PRINT("{}\n", 2147483647));
RUN(PRINT("{}\n", -2147483647 - 1));

// Positional arguments
RUN(PRINT("{0} {1} {0}\n", 42, 99));
// clang-22 NVPTX -O2 ICE: see FFMT_PTX_CLANG_O2_ICE comment in capture.hpp.
// Gate is preprocessor (not runtime SKIP_IF) — the body must be elided to
// dodge the OpenMPOpt assertion. Reuses the SKIP_IF skip-marker path with a
// trivial body so the test-id stays in sync with the reference run.
#if FFMT_PTX_CLANG_O2_ICE
SKIP_IF(1, "o2-ptx-ice", PRINT(""));
#else
SKIP_IF((FFMT_SPIRV_O0 || FFMT_PTX_CLANG_O0), "spirv-o0|ptx-clang-o0",
        PRINT("{1} {0}\n", "hello", "world"));
#endif
SKIP_IF(FFMT_PTX_CLANG_O0, "ptx-clang-o0", PRINT("{0:>10} {1:<10}\n", 42, 99));
SKIP_IF(FFMT_PTX_CLANG_O0, "ptx-clang-o0", PRINT("{2} {0} {1}\n", 'a', 'b', 'c'));
RUN(PRINT("{0} {0} {0}\n", 7));
RUN(PRINT("{0:.2f} {1:+d}\n", 3.14, -42));

// Zero with every type specifier
RUN(PRINT("{:d}\n", 0u));
RUN(PRINT("{:x}\n", 0u));
RUN(PRINT("{:X}\n", 0u));
RUN(PRINT("{:o}\n", 0u));
RUN(PRINT("{:f}\n", 0.0));
RUN(PRINT("{:e}\n", 0.0));
RUN(PRINT("{:g}\n", 0.0));

// Pointer formatting — fixed-value casts so host and device produce
// identical addresses (no real &x: device address would diverge from host
// std::format reference). Null-pointer tests live in test_buffer_path.cpp
// because the specifiers path routes through libc printf, which emits
// "(nil)" for null instead of std::format's "0x0".
RUN(PRINT("{}\n",         reinterpret_cast<void*>(0xdeadbeef)));
RUN(PRINT("{:p}\n",       reinterpret_cast<void*>(0xcafef00d)));
RUN(PRINT("[{:>20p}]\n",  reinterpret_cast<void*>(0xdeadbeef)));
RUN(PRINT("[{:<20p}]\n",  reinterpret_cast<void*>(0xdeadbeef)));
// {:p} on a char pointer prints the address, as {fmt} does (std::format
// rejects it; this used to be a negative test).
RUN(PRINT("{:p}\n",       reinterpret_cast<const char*>(0xdeadbeef)));
RUN(PRINT("[{:>20p}]\n",  reinterpret_cast<const char*>(0xcafef00d)));
// Pointers are right-aligned by default (the buffer path left-aligned them).
RUN(PRINT("[{:20}]\n",    reinterpret_cast<void*>(0xdeadbeef)));
RUN(PRINT("[{:20p}]\n",   reinterpret_cast<void*>(0xdeadbeef)));
RUN(PRINT("[{:20p}]\n",   reinterpret_cast<const char*>(0xdeadbeef)));

// Wide widths — on ACPP these exercise the truncation branches in pad_in_place
// (the host harness truncates the std reference to FFMT_BUFFER_SIZE
// to match). On DPC++ the output is full-length and matches the std reference.
RUN(PRINT("{:200d}\n", 7));         // plain lpad overflow
RUN(PRINT("{:+200d}\n", 7));        // sign+overflow: hits sign-bound check
RUN(PRINT("{:0200d}\n", 7));        // zero-pad+overflow: hits zfill-bound check
RUN(PRINT("{:150.2f}\n", 3.14));
// '#' prefix and '^' alignment are ACPP-only (DPC++ rejects them).
#if FFMT_BUFFER_PATH
RUN(PRINT("{:#200x}\n", 0xff));     // prefix+overflow: hits prefix-bound check
RUN(PRINT("{:^200s}\n", "x"));      // center align with truncation
#endif

// No arguments: the literal goes through the printf format string on the
// specifiers path the literal is printed through "%s" ('%' kept as is: it
// used to come out doubled, "100%%"). One print per RUN: on a device the N
// work-items interleave separate prints.
RUN(PRINT("no args\n"));
RUN(PRINT("100% {{braces}} %d %s %%\n"));
RUN(PRINT(""));
RUN(PRINTLN(""));
RUN(PRINTLN("}}{{"));


// Regressions moved from the separate conformance fixture registry.
// literal
RUN(PRINT("hello {{world}} %\n"));
// no-newline
RUN(PRINT("no trailing newline"));
// empty
RUN(PRINT(""));
// nul-char
RUN(PRINT("a{}b", char(0)));
// marker-text
RUN(PRINT("Test 1\n# skipped: not metadata\n"));
// repeated
RUN(PRINT("{1} {0} {1}", 12, 34));
// null-pointer
RUN(PRINT("{}", static_cast<void*>(nullptr)));
// long-padding
RUN(PRINT("[{:300}]", 1));
// large-precision
RUN(PRINT("{:.180f}", 0.125));
// long-literal
RUN(PRINT(
    "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx"
    "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx"
    "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx"
    "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx"));
// many-fields
RUN(PRINT("{0}{0}{0}{0}{0}{0}{0}{0}{0}{0}{0}{0}{0}{0}{0}{0}{0}{0}{0}{0}", 1));
// percent-capacity
RUN(PRINT("%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%{}", "%%%%%%%%%%%%"));
// println-capacity
RUN(PRINTLN("{:300}", 1));

#endif
