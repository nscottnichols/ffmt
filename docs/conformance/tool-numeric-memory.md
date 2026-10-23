# Compare bounded numeric memory records with host fmt output

Printf transport can hide whether floating-point conversion and bounded output accounting produced the correct memory bytes.

A record for a subnormal double checks the input encoding, output bytes, written/required counts and error status against independent fmt.

Use explicit binary framing and a fixed seed. Keep numeric operations in a conventionally formatted handwritten C++ template that is hashed with runner inputs.

The 4,096 float and double encodings plus boundaries are representative samples; matching this corpus is not exhaustive floating-point proof. This stage executes host memory only.

The numeric memory suite checks byte payloads, framing, input bits, written and required sizes, and error status. The template is handwritten source, not excluded generated material. Raw record framing is unchanged from the preserved suite.

Validation:

```sh
python3 -m unittest discover -s test/conformance -p test_numeric_memory.py
python3 tools/conformance.py --route host-buffer --opt 2 --numeric-samples 32
```
