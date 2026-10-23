# Add independent HIP differential execution adapter

Portable output cases need a backend-specific execution launcher without coupling unrelated accelerator runtimes.

Run the same case through a HIP kernel without a fmt device build.

Register this adapter independently; keep handwritten compiler flags and C++ launch code next to each other.

Execution requires the selected HIP or chipStar runtime and a device.

Run the same case through a HIP kernel without a fmt device build. Execution requires the selected HIP or chipStar runtime and a device.

Validation:

```sh
python3 -m unittest discover -s test/conformance -p test_adapter_hip.py
```
