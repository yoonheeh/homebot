# pico_interface/

Host-side C++ software that talks to the Raspberry Pi Pico over USB serial, fuses encoder and IMU data into robot pose, and provides calibration / manual-control utilities.

## Files

### Applications

- `main.cpp` — state estimator node.
  - Opens `/dev/ttyACM0`.
  - Loads `data/robot_calibration.txt` (or uses defaults).
  - Starts the `PicoInterface` reader and `StateEstimator` EKF consumer.
  - Streams `x,y,theta` poses on stdout at 20Hz and writes human-readable logs to `logs/estimator.log`.
- `control.cpp` — sends a single `VelocityTarget` command (left/right rad/s) to the Pico over USB serial.
- `calibrate_encoders.cpp` — interactive straight-line calibration tool that computes an odometry scale factor.
- `test_suite.cpp` — unit tests covering telemetry queue stress, serialization, and EKF math.

### Library headers (`include/pico_interface/`)

- `TelemetryDefs.hpp` — shared packet structs (`VelocityTarget`, `EncoderIMUTelemetry`, `RobotPose`) plus COBS encode/decode, CRC16-CCITT, and Linux serial setup.
- `TelemetryQueue.hpp` — thread-safe, bounded telemetry queue used between the serial reader and the EKF consumer.
- `PicoInterface.hpp` — serial reader thread, COBS/CRC validation, and velocity sending.
- `StateEstimator.hpp` — EKF that fuses averaged wheel-encoder deltas with integrated IMU gyro yaw to estimate `(x, y, theta)`.
- `RobotConfigIO.hpp` — reads/writes the `data/robot_calibration.txt` parameter file.

### Build files

- `BUILD.bazel` — Bazel targets for the library, four binaries, and `remote_run` wrappers.
- `CMakeLists.txt` — legacy standalone CMake build.
- `docs/calibration.md` — step-by-step wheel-radius / odometry calibration procedure.

## Build

```bash
# Bazel (recommended)
bazel build //pico_interface/...
bazel run //pico_interface:pico_interface

# Cross-compile for Firefly aarch64
bazel build --config=arm64 //pico_interface/...

# CMake (legacy)
cd pico_interface && mkdir -p build && cd build && cmake .. && make
```

## Remote execution targets

`BUILD.bazel` defines `remote_run` wrappers that cross-compile, scp the binary to a board, and run it over SSH:

```bash
bazel run --config=arm64 //pico_interface:run_state_estimator -- user@board-host
bazel run --config=arm64 //pico_interface:run_control -- user@board-host 1.0 -1.0
bazel run --config=arm64 //pico_interface:run_calibrate_encoders -- user@board-host
```

## Calibration

`calibrate_encoders` drives the robot straight for 2 seconds, asks for the measured distance, and writes:

```text
wheel_radius=0.032500
wheel_base=0.160000
ticks_per_rev=4320.000000
scale_factor=1.000000
```

The state estimator multiplies `wheel_radius` by `scale_factor` at runtime. See `docs/calibration.md` for the full procedure.

## Wire format

- USB serial, 115200 baud.
- Host → Pico: `VelocityTarget` (2 floats + CRC16), COBS-encoded.
- Pico → Host: `EncoderIMUTelemetry` (timestamp, 4 encoder counts, accel/gyro, CRC16), COBS-encoded.
