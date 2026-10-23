"""Handwritten CUDA launchers for Clang CUDA and NVIDIA nvcc."""
import os
from device_arch import cuda_architecture

ROUTES = ("cuda", "nvcc")

def compiler(route):
    return "nvcc" if route == "nvcc" else "clang++"

def flags(route, optimization):
    architecture = cuda_architecture()
    common = ["-std=c++20", "-O" + str(optimization)]
    if route == "nvcc":
        return common + ["-x", "cu", "--extended-lambda", "--expt-relaxed-constexpr",
                         "-arch=" + architecture, "-Xcompiler=-fno-fast-math"]
    cuda = os.environ.get("CUDA_PATH", "/usr/local/cuda")
    return common + ["-fno-fast-math", "-x", "cuda", "--cuda-path=" + cuda,
                     "--cuda-gpu-arch=" + architecture, "-L" + cuda + "/lib64", "-lcudart"]

def prelude(route):
    return "#include <cuda_runtime.h>\n#define HD __host__ __device__\n"

def launcher(route):
    return r"""
__global__ void kernel(int index) { execute_case(index); }
int main(int argc, char** argv) {
  if (argc != 2) return 2;
  int count = 0;
  if (cudaGetDeviceCount(&count) != cudaSuccess || count == 0) return 77;
  cudaDeviceProp properties{};
  if (cudaGetDeviceProperties(&properties, 0) != cudaSuccess) return 78;
  std::fprintf(stderr, "FFMT_DEVICE cuda name=%s arch=%d.%d\n",
               properties.name, properties.major, properties.minor);
  kernel<<<1, 1>>>(std::atoi(argv[1]));
  if (cudaGetLastError() != cudaSuccess) return 79;
  if (cudaDeviceSynchronize() != cudaSuccess) return 80;
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
  if (cudaMalloc((void**)&device_input, input.size() * sizeof(Input)) != cudaSuccess) return 81;
  if (cudaMalloc((void**)&device_output, output.size() * sizeof(Output)) != cudaSuccess) return 82;
  if (cudaMemcpy(device_input, input.data(), input.size() * sizeof(Input), cudaMemcpyHostToDevice) != cudaSuccess) return 83;
  if (cudaMemset(device_output, 0, output.size() * sizeof(Output)) != cudaSuccess) return 84;
  memory_kernel<<<(input.size() + 63) / 64, 64>>>(device_input, device_output, unsigned(input.size()));
  if (cudaGetLastError() != cudaSuccess || cudaDeviceSynchronize() != cudaSuccess) return 85;
  if (cudaMemcpy(output.data(), device_output, output.size() * sizeof(Output), cudaMemcpyDeviceToHost) != cudaSuccess) return 86;
  cudaFree(device_input);
  cudaFree(device_output);
  serialize(input, output);
  return 0;
}
"""
