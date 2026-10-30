# Build and install

ffmt requires C++20. The complete implementation is maintained directly in
`include/ffmt/base.hpp`; copying that file alone is sufficient. The other header
paths are compatibility includes. There is no amalgamation or generation step.
C++23 expected adapters are guarded and do not raise the C++20 requirement.
A host consumer needs neither fmt, Python nor a GPU runtime.

```sh
mkdir -p /tmp/my-include/ffmt
cp include/ffmt/base.hpp /tmp/my-include/ffmt/
c++ -std=c++20 -I/tmp/my-include my_program.cpp -o my_program
```

CMake 3.20 or newer provides a header-only interface target:

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

The package installs the implementation, compatibility wrappers, interface target
and notices. Configuring an ordinary consumer never discovers or fetches test
dependencies. Applications supply their backend compiler flags, runtime linking,
launching and synchronization; see [backend contracts](BACKENDS.md).

## Portable tests

Opt-in tests require Git, Python 3 and a clean fmt checkout at
`0fc31633e7d53ac895ae0a688b83b7d44f0b0c1f`. CMake verifies the checkout without
modifying it. Make can fetch that exact reference if it is absent.

```sh
make verify-fmt CXX=g++
make -j2 test-portable CXX=g++
cmake -S . -B build/cmake -DFFMT_BUILD_TESTS=ON -DFFMT_FMT_DIR="$PWD/third_party/fmt"
cmake --build build/cmake -j2
ctest --test-dir build/cmake --output-on-failure
```

Both frontends read the same explicit inventory, `test/direct/tests.mk`. They
build the original shared-case host harness and direct API regressions. Missing
sources are errors. Assertions stay enabled in Release builds. fmt's implementation
is compiled once for the host, including in accelerator builds.

`make test-cxx23` or `-DFFMT_BUILD_CXX23_TESTS=ON` opts into C++23 tests. Compiler
extension tests return 77 when unsupported; this is a visible skip, not a pass.
The isolated consumer target compiles with only a copied `base.hpp`, without the
repository include directory, compatibility wrappers, or fmt.

See the [test guide](CONFORMANCE.md) and [maintenance inventory](MAINTENANCE.md)
for test boundaries, flags and retained components.
