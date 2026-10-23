"""Handwritten GPU-only SYCL and AdaptiveCpp launchers."""
ROUTES = ("sycl", "acpp")

def compiler(route):
    return "acpp" if route == "acpp" else "icpx"

def flags(route, optimization):
    backend = ["--acpp-targets=generic"] if route == "acpp" else ["-fsycl"]
    return ["-std=c++20", "-O" + str(optimization), "-fno-fast-math"] + backend

def prelude(route):
    return "#include <sycl/sycl.hpp>\n#define HD\n"

def launcher(route):
    return r"""
#include <exception>
int main(int argc, char** argv) {
  if (argc != 2) return 2;
  try {
    sycl::queue queue{sycl::gpu_selector_v};
    auto device = queue.get_device();
    auto name = device.get_info<sycl::info::device::name>();
    std::fprintf(stderr, "FFMT_DEVICE sycl name=%s fp64=%d\n",
                 name.c_str(), device.has(sycl::aspect::fp64));
    int index = std::atoi(argv[1]);
    queue.single_task([=] { execute_case(index); }).wait_and_throw();
    return 0;
  } catch (const std::exception& error) {
    std::fprintf(stderr, "%s\n", error.what());
    return 80;
  }
}
"""
