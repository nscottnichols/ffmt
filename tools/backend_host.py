"""Host execution through the two historical public macro settings."""
import os

ROUTES = ("host-buffer", "host-specifiers")

def compiler(route):
    return os.environ.get("CXX", "g++")

def flags(route, optimization):
    buffer_path = "1" if route == "host-buffer" else "0"
    return ["-std=c++20", "-O" + str(optimization), "-fno-fast-math",
            "-DFFMT_BUFFER_PATH=" + buffer_path]

def prelude(route):
    return "#define HD\n"

def launcher(route):
    return r"""
int main(int argc, char** argv) {
  if (argc != 2) return 2;
  std::fprintf(stderr, "FFMT_DEVICE host\n");
  execute_case(std::atoi(argv[1]));
  return 0;
}
"""


def numeric_launcher(route):
    return r"""
int main() {
  auto input = inputs();
  std::vector<Output> output(input.size());
  for (unsigned i = 0; i < input.size(); ++i)
    format_record(input[i], output[i]);
  serialize(input, output);
  return 0;
}
"""
