# src/

Placeholder C++ package used primarily to validate the Bazel C++ toolchain and cross-compilation setup.

## Files

- `main.cc` — minimal architecture-detection binary. Prints whether it is running on `aarch64`, `x86_64`, or an unknown architecture.
- `BUILD.bazel` — defines `//src:system_node`, a `cc_binary` built with `-std=c++17 -O3`.

## Role in the repo

`src/` is not active robotics code; the real host software lives in `pico_interface/`. This package exists as a smoke test for the monorepo Bazel setup, especially the aarch64 cross-compilation path:

```bash
# host
bazel build //src:system_node

# Firefly aarch64
bazel build --config=arm64 //src:system_node
```

The cross-compiled `bazel-bin/src/system_node` will be an ELF aarch64 binary suitable for copying to the Firefly board.

## Dependencies

Standard C++ library only; no external deps beyond the Bazel `rules_cc` toolchain.
