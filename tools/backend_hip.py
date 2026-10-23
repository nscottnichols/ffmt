"""Handwritten HIP launchers, including the chipStar compiler route."""
from device_arch import hip_architecture

ROUTES = ("hip", "chipstar")

def compiler(route):
    return "hipcc"

def flags(route, optimization):
    result = ["-std=c++20", "-O" + str(optimization), "-fno-fast-math", "-x", "hip"]
    return result + (["--offload-arch=" + hip_architecture()] if route == "hip" else [])

def prelude(route):
    return "#include <hip/hip_runtime.h>\n#define HD __host__ __device__\n"

def launcher(route):
    return r"""
__global__ void kernel(int index) { execute_case(index); }
int main(int argc, char** argv) {
  if (argc != 2) return 2;
  int count = 0;
  if (hipGetDeviceCount(&count) != hipSuccess || count == 0) return 77;
  hipDeviceProp_t properties{};
  if (hipGetDeviceProperties(&properties, 0) != hipSuccess) return 78;
  std::fprintf(stderr, "FFMT_DEVICE hip name=%s arch=%d.%d\n",
               properties.name, properties.major, properties.minor);
  hipLaunchKernelGGL(kernel, dim3(1), dim3(1), 0, 0, std::atoi(argv[1]));
  if (hipGetLastError() != hipSuccess) return 79;
  if (hipDeviceSynchronize() != hipSuccess) return 80;
  return 0;
}
"""


def numeric_launcher(route):
    return r"""
__global__ void memory_kernel(const Input* input, Output* output, unsigned count) {
  unsigned i = blockIdx.x * blockDim.x + threadIdx.x;
  if (i < count) format_record(input[i], output[i]);
}
int main() {
  auto input = inputs();
  std::vector<Output> output(input.size());
  Input* device_input = nullptr;
  Output* device_output = nullptr;
  if (hipMalloc((void**)&device_input, input.size() * sizeof(Input)) != hipSuccess) return 81;
  if (hipMalloc((void**)&device_output, output.size() * sizeof(Output)) != hipSuccess) return 82;
  if (hipMemcpy(device_input, input.data(), input.size() * sizeof(Input), hipMemcpyHostToDevice) != hipSuccess) return 83;
  if (hipMemset(device_output, 0, output.size() * sizeof(Output)) != hipSuccess) return 84;
  hipLaunchKernelGGL(memory_kernel, dim3((input.size() + 63) / 64), dim3(64), 0, 0, device_input, device_output, unsigned(input.size()));
  if (hipGetLastError() != hipSuccess || hipDeviceSynchronize() != hipSuccess) return 85;
  if (hipMemcpy(output.data(), device_output, output.size() * sizeof(Output), hipMemcpyDeviceToHost) != hipSuccess) return 86;
  hipFree(device_input);
  hipFree(device_output);
  serialize(input, output);
  return 0;
}
"""
