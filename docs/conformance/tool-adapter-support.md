# Share explicit compiler target selection between execution adapters

CUDA, HIP and OpenMP launchers need the same target-selection rules without importing each other.

FFMT_GPU_ARCH=sm_80 selects sm_80 without consulting nvidia-smi.

Isolate architecture discovery behind two small functions; prefer explicit environment overrides and retain existing fallback targets.

Fallback targets support compilation without a device, but do not prove execution compatibility. Execution controls remain required.

Set FFMT_GPU_ARCH explicitly when a compiler target differs from the first visible device. Target discovery is a compiler default, not execution evidence.

Validation:

```sh
python3 -m unittest discover -s test/conformance -p test_device_arch.py
```
