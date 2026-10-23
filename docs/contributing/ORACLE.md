# Portable formatting oracle

Portable comparisons use fmt commit `0fc31633e7d53ac895ae0a688b83b7d44f0b0c1f`.
Pass `FMT_DIR=/path/to/fmt` to Make to use an existing checkout. The differential
runner verifies the immutable revision before producing qualification records.

Pinning expected behavior avoids changes caused by the system standard library
or a moving upstream branch. Updating this pin is an explicit compatibility
decision; it does not change ffmt headers.

Make verifies the Git revision and clean checkout before reference builds. A
mismatch fails without modifying that directory. Unpacked sources without Git
metadata are not accepted for Make oracle checks; supply an existing clean clone.
