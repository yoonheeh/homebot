# pico_firmware/

Embedded C/C++ firmware for the Raspberry Pi Pico (RP2040). It runs the low-level control loop: reading quadrature encoders and IMU data, sending telemetry to the host, and receiving wheel-velocity commands over USB serial.

## Files

- `main.cpp` — the real-time firmware loop.
  - Initializes 4x PIO-based quadrature encoder readers on `pio0` state machines 0–3.
  - Sets up PWM motor control on GP12/GP13 (shared slice 6) and direction pins on GP10/GP11 (left) and GP14/GP15 (right).
  - Initializes I2C0 for the MPU-6050 IMU on GP16/GP17 and performs an I2C bus scan on startup.
  - Receives `VelocityTarget` commands from the host via COBS-decoded USB serial.
  - Builds and transmits `EncoderIMUTelemetry` packets at a strict 50Hz loop rate.
  - Maps open-loop rad/s commands to PWM duty cycles.
- `quadrature_encoder.pio` — RP2040 PIO program for 4× quadrature decoding.
- `CMakeLists.txt` — standalone firmware CMake project.
- `pico_sdk_import.cmake` — imports the Raspberry Pi Pico SDK.
- `README.md` — detailed wiring tables and flashing instructions.

## Build

Requires the ARM embedded toolchain:

```bash
sudo apt install gcc-arm-none-eabi libnewlib-arm-none-eabi build-essential cmake

cd pico_firmware
mkdir -p build && cd build
cmake ..
make -j$(nproc)
```

Outputs `pico_firmware.uf2`, ready to flash.

## Flashing

1. Hold the Pico `BOOTSEL` button.
2. Plug in USB.
3. Release `BOOTSEL`; the Pico mounts as `RPI-RP2`.
4. Copy `build/pico_firmware.uf2` onto the drive.
5. The Pico reboots and starts running the firmware.

## Hardware pinout

| Function | Pins |
|----------|------|
| Front-Left encoder | GP2 / GP3 |
| Rear-Left encoder | GP4 / GP5 |
| Front-Right encoder | GP6 / GP7 |
| Rear-Right encoder | GP8 / GP9 |
| Left PWM | GP12 |
| Left direction | GP10 / GP11 |
| Right PWM | GP13 |
| Right direction | GP14 / GP15 |
| IMU I2C0 SDA / SCL | GP16 / GP17 |

See `README.md` for the full wiring schematic, power/ground guidance, and motor-polarity notes.

## Communication protocol

- USB serial at 115200 baud.
- Host → Pico: `VelocityTarget` (left/right rad/s, CRC16-CCITT), COBS-encoded.
- Pico → Host: `EncoderIMUTelemetry` (timestamp, 4 encoder counts, accel/gyro, CRC16-CCITT), COBS-encoded.

The structs and encode/decode helpers are mirrored in `pico_interface/include/pico_interface/TelemetryDefs.hpp`.

## Loop timing

The firmware enforces a 50Hz control/telemetry loop (20ms). Encoder and IMU readings are packed, CRC-checked, COBS-encoded, and sent each iteration. Motors are updated with open-loop PWM derived from the most recent host velocity target.
