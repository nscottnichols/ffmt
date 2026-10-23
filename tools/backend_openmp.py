"""Handwritten mandatory-offload OpenMP launchers."""
import os
from device_arch import cuda_architecture, hip_architecture

ROUTES = ("omp-cuda", "omp-hip", "omp-spirv")

def compiler(route):
    return "icpx" if route == "omp-spirv" else "clang++"

def flags(route, optimization):
    common = ["-std=c++20", "-O" + str(optimization), "-fno-fast-math"]
    if route == "omp-spirv":
        return common + ["-fiopenmp", "-fopenmp-targets=spir64"]
    if route == "omp-cuda":
        return common + ["-fopenmp", "--offload-arch=" + cuda_architecture(),
                         "--cuda-path=" + os.environ.get("CUDA_PATH", "/usr/local/cuda")]
    return common + ["-fopenmp", "--offload-arch=" + hip_architecture(),
                     "--rocm-path=" + os.environ.get("ROCM_PATH", "/opt/rocm")]

def prelude(route):
    return "#include <omp.h>\n#define HD\n"

def launcher(route):
    return r"""
int main(int argc, char** argv) {
  if (argc != 2) return 2;
  int initial_device = 1;
  #pragma omp target map(from: initial_device)
  { initial_device = omp_is_initial_device(); }
  if (initial_device) return 77;
  std::fprintf(stderr, "FFMT_DEVICE openmp device=%d initial_device=false\n",
               omp_get_default_device());
  int index = std::atoi(argv[1]);
  #pragma omp target firstprivate(index)
  { execute_case(index); }
  return 0;
}
"""


def numeric_launcher(route):
    return r"""
int main() {
  auto input = inputs();
  std::vector<Output> output(input.size());
  unsigned count = unsigned(input.size());
  Input* device_input = input.data();
  Output* device_output = output.data();
  int initial_device = 1;
  #pragma omp target map(from: initial_device)
  { initial_device = omp_is_initial_device(); }
  if (initial_device) return 77;
  #pragma omp target teams distribute parallel for map(to: device_input[0:count]) map(from: device_output[0:count])
  for (unsigned i = 0; i < count; ++i)
    format_record(device_input[i], device_output[i]);
  serialize(input, output);
  return 0;
}
"""
