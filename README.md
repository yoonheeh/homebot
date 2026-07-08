# Homebot Workspace

This repository is the central workspace for the **Homebot** robotics platform. It contains:
1. **Host C++ Code:** High-level robot algorithms, state estimators, and native drivers (including the pico_interface package), built and managed using **Bazel**.
2. **Host Python Code:** High-level robotic behaviors, machine learning, and automation managed using **uv**.
3. **Pico Firmware (pico_firmware):** Low-level motor control and sensor feedback running on the Raspberry Pi Pico (RP2040), built as a standalone **CMake** project.

---

## 1. Project Directory Structure

<pre>
homebot/
├── BUILD.bazel          # Top-level Bazel build targets
├── MODULE.bazel         # Bazel modules and dependency definitions
├── src/                 # Host C++ and Python source files
├── object_detection/    # Object detection machine learning pipeline
├── pico_interface/      # C++ Host State Estimator, testing suite, and utilities
│   ├── BUILD.bazel      # Bazel build definitions for host tools
│   ├── CMakeLists.txt   # Legacy standalone CMake definition
│   ├── main.cpp         # High-frequency EKF state estimator and pose publisher
│   └── build/           # Standalone build output folder
├── pico_firmware/       # Raspberry Pi Pico embedded C/C++ firmware
│   ├── CMakeLists.txt   # Standalone firmware CMake definition
│   ├── main.cpp         # Low-level controller loop, PIO encoder setup, sensor queries
│   └── build/           # Build output folder
├── pyproject.toml       # Python package configuration (uv compatible)
├── uv.lock              # Strict Python lockfile managed by uv
└── README.md            # This workspace documentation
</pre>

---

## 2. Host C++ State Estimator: pico_interface

The pico_interface directory contains native Linux C++ applications to handle high-frequency sensor fusion (EKF) and pose streaming. It is fully integrated into the monorepo Bazel build graph (see Section 4), but can also be compiled standalone using CMake if desired.

### Building with Bazel

From the root directory of the workspace:
<pre>
# Build everything in pico_interface
bazel build //pico_interface/...

# Run the state estimator
bazel run //pico_interface:pico_interface
</pre>

This generates four main executables inside the build/ folder:
* **pico_interface**: The passive state estimator. Reads telemetry from the Pico at high-frequency, runs EKF, writes status logs to logs/estimator.log, and streams raw coordinates (x,y,theta) on stdout at 20Hz (ideal for plotting over SSH).
* **test_suite**: Executes the full unit test suite, verifying threading safety, queue stress tests, and EKF mathematical correctness.
* **control**: Sends manual wheel velocity commands (rad/sec) to the Pico over USB.
* **calibrate_encoders**: An interactive tool to calibrate wheel encoder tick targets.

---

## 3. Low-Level Firmware: pico_firmware

The pico_firmware directory contains the C++ code running on the Raspberry Pi Pico to handle physical motor PWM control, quadrature encoder decoding via PIO, and I2C telemetry from the MPU-6050 IMU.

### Prerequisites

You need the standard GCC ARM Embedded Toolchain and CMake installed on your host Linux build machine:

<pre>
sudo apt update
sudo apt install gcc-arm-none-eabi libnewlib-arm-none-eabi build-essential cmake
</pre>

### Building the Firmware

To build, simply run:

<pre>
cd pico_firmware

# Create and enter build folder
mkdir -p build && cd build

# Configure and compile
cmake ..
make
</pre>

This compiles the firmware and outputs the executable files inside the build/ folder. The primary file is:
* **pico_firmware.uf2**: The final ready-to-flash binary.

### Deployment & Flashing Instructions

1. **Unplug** the USB cable connecting the Pico to the host board.
2. Press and hold down the white **BOOTSEL** button on the Pico board.
3. While holding down **BOOTSEL**, **plug** the USB cable back into your host computer.
4. Release the **BOOTSEL** button. The Pico will mount onto your Linux machine as a standard USB mass storage drive named **RPI-RP2**.
5. Copy (or drag-and-drop) the generated **pico_firmware.uf2** file directly onto the mounted drive:
   <pre>
   cp pico_firmware.uf2 /media/username/RPI-RP2/
   </pre>
6. The Pico will automatically reboot, flash the new software, and begin running the low-level controller loop.

---

## 4. High-Level C++ Host Software (Bazel)

All high-level C++ applications and drivers running on the main robot computer (including the **pico_interface** package, SLAM, planners, and hardware node bridges) are compiled and executed using **Bazel**.

### Build Commands

To build all host C++ packages and binaries:
<pre>
bazel build //...
</pre>

To build only the pico_interface package:
<pre>
bazel build //pico_interface/...
</pre>

To run a specific C++ target:
<pre>
bazel run //pico_interface:pico_interface
</pre>

---

## 5. Host Python Software (uv)

Python applications (such as object detection, machine learning, and high-level behavioral scripts) are managed using **uv**, an extremely fast, modern Python package installer and virtual environment manager.

### Setting Up the Environment

To set up your local virtual environment and install all packages:
<pre>
# Install uv if you do not have it
curl -LsSf https://astral.sh/uv/install.sh | sh

# Sync the virtual environment and install dependencies
uv sync
</pre>

### Running Python Scripts

To run the main python node:
<pre>
uv run main.py
</pre>

To run scripts inside the virtual environment:
<pre>
uv run python path/to/script.py
</pre>
