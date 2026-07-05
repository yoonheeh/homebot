# Homebot Repository

This is the central workspace for the **Homebot** robotics platform: a polyglot robotics stack with C++ host software and Python perception/behavior scripts, plus embedded firmware for a Raspberry Pi Pico motor/sensor controller.

## Project structure

- `src/` — host C++ entry points and Bazel `cc_binary` targets.
- `pico_interface/` — C++ state estimator, telemetry bridge, unit tests, and calibration tools that talk to the Pico over USB serial.
- `pico_firmware/` — standalone CMake firmware for the Raspberry Pi Pico (RP2040); handles PWM motor control, quadrature encoder decoding via PIO, and MPU-6050 IMU telemetry.
- `object_detection/` — Rockchip RK3588 NPU object-detection pipeline built around a shared `YoloEngine`.
- `scripts/` — Python utilities for streaming, evaluation, inference testing, and deploying binaries to the robot board.
- `platforms/` — Bazel platform definition for cross-compiling to the Firefly aarch64 board.
- `data/` — runtime data directory for calibration files, snapshots, and evaluation datasets.

## Build systems

The repo deliberately uses two independent build systems (see `NOTES.md`, ADR-001):

- **Bazel** for C++ (`MODULE.bazel`, `.bazelrc`, `BUILD.bazel` files).
- **uv** for Python (`pyproject.toml`, `uv.lock`).

C++ and Python do not link against each other; they communicate only over IPC (USB serial / stdout / network streams), so the split is clean.

## Quick commands

```bash
# C++ host
bazel build //...
bazel run //pico_interface:pico_interface

# C++ cross-compile for Firefly aarch64
bazel build --config=arm64 //pico_interface/...

# Python
uv sync
uv run main.py
uv run python scripts/test_inference.py
```

## Hardware context

- Compute: Firefly ITX-3588J (Ubuntu 22.04, RK3588 SoC with 3x NPU cores).
- Chassis: 4WD TT-motor chassis with quadrature encoders and TB6612FNG motor drivers.
- Microcontroller: Raspberry Pi Pico (RP2040) running `pico_firmware`.
- Camera: ESP32-CAM WiFi video stream (`192.168.4.1:81/stream`).

## Communication

Host ↔ Pico exchange COBS-encoded, CRC16-CCITT-protected packets over USB serial at 115200 baud:

- Host → Pico: `VelocityTarget` (left/right wheel speeds in rad/s).
- Pico → Host: `EncoderIMUTelemetry` (encoder counts + accelerometer + gyroscope).

The host fuses this into `(x, y, theta)` pose using an EKF in `pico_interface/include/pico_interface/StateEstimator.hpp`.
