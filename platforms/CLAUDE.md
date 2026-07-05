# platforms/

Bazel platform definitions used for cross-compilation.

## Files

- `BUILD.bazel` — defines the `//platforms:aarch64_linux` Bazel platform.

## Purpose

This platform tells Bazel to resolve the C++ toolchain for a Linux aarch64 target. It is used with the `--config=arm64` flag, which maps to:

```text
build:arm64 --platforms=//platforms:aarch64_linux
```

The actual cross-compiler comes from the `@toolchains_llvm//...` LLVM toolchain registered in `MODULE.bazel`, combined with the pinned Debian Stretch aarch64 sysroot (`@sysroot_linux_aarch64`).

## Usage

```bash
bazel build --config=arm64 //src:system_node
bazel build --config=arm64 //pico_interface/...
```

The resulting binaries in `bazel-bin/` are ELF aarch64 executables suitable for running on the Firefly ITX-3588J board.

## Relationship to other parts

- Referenced by `.bazelrc`.
- Used by `pico_interface/BUILD.bazel` and `scripts/BUILD.bazel` indirectly through the `arm64` config.
- No runtime code lives here; this directory is pure build configuration.
