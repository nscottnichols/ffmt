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
