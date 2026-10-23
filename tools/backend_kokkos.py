"""Handwritten Kokkos wrappers for adapters present in this revision."""
import os
from pathlib import Path

BASES = {"kokkos-serial": "host-buffer", "kokkos-cuda": "cuda",
         "kokkos-hip": "hip", "kokkos-sycl": "sycl"}

def compiler(route):
    import backends
    return " ".join(backends.compiler(BASES[route]))

def prefix(route):
    from backends import WORKSPACE
    key = "FFMT_KOKKOS_" + route.split("-", 1)[1].upper() + "_PREFIX"
    fallback = WORKSPACE / "software" / os.environ.get("FFMT_PROFILE", "local") / "install" / route
    return Path(os.environ.get(key, fallback))

def flags(route, optimization):
    import backends
    return backends.flags(BASES[route], optimization) + ["-I" + str(prefix(route) / "include")]

def link_flags(route):
    library = prefix(route) / "lib"
    if not library.is_dir():
        library = prefix(route) / "lib64"
    return ["-L" + str(library), "-lkokkoscontainers", "-lkokkoscore", "-ldl", "-pthread"] + (["-lcuda"] if route == "kokkos-cuda" else [])

def prelude(route):
    import backends
    return backends.prelude(BASES[route]) + "#include <Kokkos_Core.hpp>\n"

def device_guard(route):
    if route == "kokkos-sycl":
        return r"""
  auto device = Kokkos::DefaultExecutionSpace{}.sycl_queue().get_device();
  if (!device.is_gpu()) { Kokkos::finalize(); return 77; }
  std::fprintf(stderr, "FFMT_DEVICE kokkos space=SYCL name=%s\n",
               device.get_info<sycl::info::device::name>().c_str());
"""
    if route in ("kokkos-cuda", "kokkos-hip"):
        api = "cuda" if route == "kokkos-cuda" else "hip"
        properties = "cudaDeviceProp" if api == "cuda" else "hipDeviceProp_t"
        space = "Cuda" if api == "cuda" else "HIP"
        return r"""
  int device_id = 0;
  PROPERTIES properties{};
  if (APIGetDevice(&device_id) != APISuccess ||
      APIGetDeviceProperties(&properties, device_id) != APISuccess) {
    Kokkos::finalize(); return 78;
  }
  std::fprintf(stderr, "FFMT_DEVICE kokkos space=SPACE name=%s arch=%d.%d\n",
               properties.name, properties.major, properties.minor);
""".replace("PROPERTIES", properties).replace("API", api).replace("SPACE", space)
    return '  std::fprintf(stderr, "FFMT_DEVICE kokkos space=Serial\\n");\n'

def launcher(route):
    space = {"kokkos-serial": "Serial", "kokkos-cuda": "Cuda",
             "kokkos-hip": "HIP", "kokkos-sycl": "SYCL"}[route]
    return r"""
#include <cstring>
int main(int argc, char** argv) {
  if (argc != 2) return 2;
  Kokkos::initialize(argc, argv);
  if (std::strcmp(Kokkos::DefaultExecutionSpace::name(), "SPACE") != 0) {
    Kokkos::finalize(); return 77;
  }
GUARD
  int index = std::atoi(argv[1]);
  Kokkos::parallel_for("ffmt-case", Kokkos::RangePolicy<>(0, 1),
                      KOKKOS_LAMBDA(int) { execute_case(index); });
  Kokkos::fence();
  Kokkos::finalize();
  return 0;
}
""".replace("SPACE", space).replace("GUARD", device_guard(route))


def numeric_launcher(route):
    space = {"kokkos-serial": "Serial", "kokkos-cuda": "Cuda",
             "kokkos-hip": "HIP", "kokkos-sycl": "SYCL"}[route]
    return r"""
#include <cstring>
int main(int argc, char** argv) {
  Kokkos::initialize(argc, argv);
  if (std::strcmp(Kokkos::DefaultExecutionSpace::name(), "SPACE") != 0) {
    Kokkos::finalize(); return 77;
  }
GUARD
  {
    auto input = inputs();
    std::vector<Output> output(input.size());
    unsigned count = unsigned(input.size());
    Kokkos::View<Input*> device_input("input", count);
    Kokkos::View<Output*> device_output("output", count);
    auto host_input = Kokkos::create_mirror_view(device_input);
    for (unsigned i = 0; i < count; ++i) host_input(i) = input[i];
    Kokkos::deep_copy(device_input, host_input);
    Kokkos::parallel_for("ffmt-memory", Kokkos::RangePolicy<>(0, count),
      KOKKOS_LAMBDA(int i) { format_record(device_input(i), device_output(i)); });
    auto host_output = Kokkos::create_mirror_view_and_copy(Kokkos::HostSpace{}, device_output);
    for (unsigned i = 0; i < count; ++i) output[i] = host_output(i);
    serialize(input, output);
  }
  Kokkos::finalize();
  return 0;
}
""".replace("SPACE", space).replace("GUARD", device_guard(route))
