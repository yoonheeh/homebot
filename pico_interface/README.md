# Simple Control & Homebot

This repository contains the software and firmware suite for the **Homebot** robot base estimation and control system.

The codebase is split into two major components:
1. **Host Software:** Native Linux C++ applications (state estimator, unit tests, calibrators, and controllers) built via CMake/Bazel.
2. **Pico Firmware (`pico_firmware`):** Embedded C firmware running on the Raspberry Pi Pico (RP2040) to interface with physical hardware (motors, encoders, IMU).

---

## 1. Host Software (Linux)

The host software resides in the root directory and contains tools to communicate with the Pico over a USB serial connection, run state estimation (EKF), and test mathematical correctness.

### Build Instructions

The host tools are built with **Bazel** (recommended). A legacy standalone **CMake** build is also provided.

#### Option A: Building with Bazel (Recommended)

From the root directory of the workspace:

```bash
# Build everything in pico_interface
bazel build //pico_interface/...

# Run the state estimator
bazel run //pico_interface:pico_interface

# Run the unit-test suite
bazel run //pico_interface:test_suite
```

To cross-compile for the Firefly aarch64 board:

```bash
bazel build --config=arm64 //pico_interface/...
```

This generates four main executables under `bazel-bin/pico_interface/`:
* **`pico_interface`:** The passive state estimator. Reads sensor telemetry from the Pico at high-frequency, runs EKF, writes human-readable status to `stderr` (logs), and streams raw poses (`x,y,theta`) on `stdout` at 20Hz.
* **`test_suite`:** Executes the full unit test suite, verifying threading safety, queue stress tests, and EKF mathematical correctness.
* **`control`:** Sends manual wheel velocity commands (rad/sec) to the Pico over USB.
* **`calibrate_encoders`:** An interactive tool to calibrate wheel encoder tick targets.

#### Option B: Standalone Build with CMake (Legacy)

To build without Bazel:

```bash
cd pico_interface
mkdir -p build && cd build
cmake ..
make -j$(nproc)
```

The same four executables are produced inside `pico_interface/build/`.

### Running the State Estimator and Logging

To stream robot pose data while logging diagnostic statuses to a file:

```bash
# Streams pose data (x,y,theta) to stdout, while logging details inside logs/estimator.log
./pico_interface
```

* Raw coordinates are streamed to `stdout` (e.g., `0.123,0.456,0.012`) at 20Hz, ideal for real-time plotting over SSH.
* Human-readable diagnostic and telemetry logs are written to `logs/estimator.log`.

---

## 2. Pico Firmware (`pico_firmware`)

The embedded microcontroller firmware resides in `pico_firmware/`. It reads wheel encoder ticks, processes IMU angular rates, manages motor PWM drivers, and communicates with the host over COBS-encoded USB serial packets.

### Prerequisites

Ensure you have the ARM cross-compilation toolchain and CMake installed on your build machine:

```bash
sudo apt update
sudo apt install gcc-arm-none-eabi libnewlib-arm-none-eabi build-essential cmake
```

### Build Instructions

The Pico firmware uses a standalone CMake project that automatically downloads the Raspberry Pi Pico SDK during configuration.

```bash
# Navigate to the firmware directory
cd firmware/pico_firmware

# Create and navigate to the build directory
mkdir -p build && cd build

# Configure for cross-compiling
cmake ..

# Compile the firmware
make -j$(nproc)
```

This compilation generates several files, most importantly:
* **`pico_firmware.uf2`**: The final flashable USB binary.
* **`pico_firmware.elf`**: Used for step-through JTAG/SWD debugging.

### Deployment / Flashing Instructions

Deploying the compiled firmware to the Raspberry Pi Pico is simple:

1. **Disconnect** the Pico's USB cable from your host machine.
2. Press and hold the white **`BOOTSEL`** button on the Pico board.
3. While holding the button, **connect** the USB cable back to your host machine.
4. Release the **`BOOTSEL`** button. The Pico will mount as a standard USB mass storage drive named **`RPI-RP2`**.
5. Copy/drag-and-drop the generated **`pico_firmware.uf2`** file directly onto the **`RPI-RP2`** drive:
   ```bash
   # Example command-line deployment
   cp pico_firmware.uf2 /media/$USER/RPI-RP2/
   ```
6. The Pico will automatically reboot, flash the new firmware, and begin running the motor controller and telemetry loop.
