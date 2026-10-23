# Build and install

ffmt is a C++20 header-only library. Use a C++20-capable compiler; CMake
integration requires CMake 3.20 or newer. Library consumers need neither fmt,
Python nor a GPU runtime for host formatting.

For a source checkout:

```cmake
add_subdirectory(ffmt)
target_link_libraries(my_program PRIVATE ffmt::ffmt)
```

For an installed package:

```sh
cmake -S . -B build/package -DCMAKE_INSTALL_PREFIX="$PWD/build/install"
cmake --install build/package
cmake -S examples/cmake-consumer -B build/consumer -DCMAKE_PREFIX_PATH="$PWD/build/install"
cmake --build build/consumer -j2
./build/consumer/ffmt_consumer
```

The consumer uses `find_package(ffmt CONFIG REQUIRED)` and links `ffmt::ffmt`.
The target supplies include directories and the C++20 language requirement.
Compiler backend flags, runtime linking and kernel launch/synchronization remain
the application's responsibility; see [transport contract](api/transport.md).

The package installs headers, CMake target metadata and third-party notices. It
does not fetch software or discover test dependencies while configuring an
ordinary consumer. Select `CMAKE_INSTALL_INCLUDEDIR` and `CMAKE_INSTALL_LIBDIR`
through standard GNUInstallDirs options when packaging for another layout.

## Portable CMake tests

Tests are opt-in. They require Python 3, Git and a clean independent fmt checkout
at revision `0fc31633e7d53ac895ae0a688b83b7d44f0b0c1f`. Configuration verifies
that checkout and never clones, switches or modifies it.

```sh
git clone https://github.com/fmtlib/fmt.git third_party/fmt
git -C third_party/fmt checkout 0fc31633e7d53ac895ae0a688b83b7d44f0b0c1f
cmake -S . -B build/cmake -DFFMT_BUILD_TESTS=ON -DFFMT_FMT_DIR="$PWD/third_party/fmt"
cmake --build build/cmake -j2
ctest --test-dir build/cmake --output-on-failure
```

Each declared C++ suite has an explicit source and CTest entry. Missing sources
fail configuration; they are never silently omitted. Assertions stay enabled in
Release builds. The floating suite also runs with `FFMT_PORTABLE_SHORTEST=1`;
the fast-math target has a separately compiled strict oracle. Python discovery
runs every `test_*.py` capability test, rather than only the original runner test.

Native int128, BitInt and binary128 tests report CTest skips via exit 77 when the
compiler lacks the tested extension. Optional C++23 expected/unexpected tests use
`-DFFMT_BUILD_CXX23_TESTS=ON`; that option requires `FFMT_BUILD_TESTS=ON` and a
compiler accepting C++23 mode. A standard library lacking expected reports 77.
A skip is not evidence that the corresponding feature conforms.

The full host differential test compares both compatibility paths, including
the independent numeric memory corpus. Compiler/runtime investigations and
accelerator campaign results remain separate; see [the independent runner guide](conformance/tool-host-oracle.md).
