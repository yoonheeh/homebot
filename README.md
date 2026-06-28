# Homebot Workspace

This repository is the central workspace for the **Homebot** robotics platform. It contains:
1. **Host Software:** Python and C++ high-level nodes running on the primary Linux SBC, built and managed using **Bazel**.
2. **Pico Firmware (pico_firmware):** Low-level motor control, encoder readings, and telemetry feedback running on the Raspberry Pi Pico (RP2040), built using **CMake**.

---

## 1. Project Directory Structure

<pre>
homebot/
├── BUILD.bazel          # Top-level Bazel build targets
├── MODULE.bazel         # Bazel modules and dependency definitions
├── src/                 # Host C++/Python source files
├── object_detection/    # Object detection machine learning pipeline
├── pico_firmware/       # Raspberry Pi Pico embedded C/C++ firmware
│   ├── CMakeLists.txt   # Firmware-specific CMake definition
│   ├── main.cpp         # Low-level controller loop, PIO encoder setup, EKF outputs
│   └── build/           # Build output folder
└── README.md            # This workspace documentation
</pre>

---

## 2. Low-Level Firmware: pico_firmware

The pico_firmware directory contains the C++ code running on the Raspberry Pi Pico to handle physical motor PWM control, quadrature encoder decoding via PIO, and I2C telemetry from the MPU-6050 IMU.

### Prerequisites

You need the standard GCC ARM Embedded Toolchain and CMake installed on your host Linux build machine:

<pre>
sudo apt update
sudo apt install gcc-arm-none-eabi libnewlib-arm-none-eabi build-essential cmake
</pre>

### Building the Firmware

The pico_firmware folder is configured with standalone CMake build files that automatically locate or bootstrap the Pico SDK dependency.

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

Deploying the compiled code onto your Raspberry Pi Pico is quick and easy:

1. **Unplug** the USB cable connecting the Pico to the host board.
2. Press and hold down the white **BOOTSEL** button on the Pico board.
3. While holding down **BOOTSEL**, **plug** the USB cable back into your host computer.
4. Release the **BOOTSEL** button. The Pico will mount onto your Linux machine as a standard USB mass storage drive named **RPI-RP2**.
5. Copy (or drag-and-drop) the generated **pico_firmware.uf2** file directly onto the mounted drive:
   <pre>
   # Example command-line copy (adjust path to match your system mount point if necessary)
   cp pico_firmware.uf2 /media/username/RPI-RP2/
   </pre>
6. The Pico will automatically reboot, flash the new software, and begin running the low-level controller loop.

---

## 3. High-Level Host Software (Bazel)

All software running on the main robot computer is built and executed using **Bazel**.

### Build and Run commands

To build the primary host software:
<pre>
bazel build //...
</pre>

To run the main Python entry node:
<pre>
bazel run //:main
</pre>
