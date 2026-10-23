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


def numeric_launcher(route):
    return r"""
int main() {
  try {
    auto input = inputs();
    std::vector<Output> output(input.size());
    sycl::queue queue{sycl::gpu_selector_v};
    Input* device_input = sycl::malloc_device<Input>(input.size(), queue);
    Output* device_output = sycl::malloc_device<Output>(output.size(), queue);
    if (!device_input || !device_output) return 81;
    queue.memcpy(device_input, input.data(), input.size() * sizeof(Input)).wait_and_throw();
    queue.memset(device_output, 0, output.size() * sizeof(Output)).wait_and_throw();
    unsigned count = unsigned(input.size());
    queue.parallel_for(sycl::range<1>(count), [=](sycl::id<1> id) {
      format_record(device_input[id[0]], device_output[id[0]]);
    }).wait_and_throw();
    queue.memcpy(output.data(), device_output, output.size() * sizeof(Output)).wait_and_throw();
    sycl::free(device_input, queue);
    sycl::free(device_output, queue);
    serialize(input, output);
    return 0;
  } catch (const std::exception& error) {
    std::fprintf(stderr, "%s\n", error.what());
    return 80;
  }
}
"""
