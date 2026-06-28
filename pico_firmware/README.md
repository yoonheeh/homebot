# Pico Firmware (4WD Encoder & IMU Feedback Chassis)

This repository contains high-performance, low-latency firmware for a 4WD robotic chassis driven by a Raspberry Pi Pico (RP2040) and dual H-bridge motor drivers. It features hardware-accelerated 4x quadrature encoder decoding via PIO, I2C IMU sensor querying, COBS-encoded full-duplex USB serial communication with CRC16-CCITT error checking, and microsecond hardware timestamping.

---

## 1. Electrical Components Used

1.  **Microcontroller:** Raspberry Pi Pico (RP2040)
2.  **Motor Drivers:** 2x TB6612FNG (or equivalent) H-Bridge Dual Motor Driver Boards:
    *   **Board L:** Left Side (Front-Left & Rear-Left Motors)
    *   **Board R:** Right Side (Front-Right & Rear-Right Motors)
3.  **Motors with Integrated Encoders:** 4x Yellow Plastic TT Gearbox Motors (1:48 ratio) with built-in Hall-effect Quadrature Encoders (12 PPR magnetic disc on the high-speed rear motor shaft, yielding 2,304 counts per output wheel revolution under 4x PIO decoding).
4.  **Inertial Measurement Unit (IMU):** 1x MPU-6050 / DZ-223 breakout board (6-axis Accelerometer & Gyroscope).
5.  **Power Supply:** 7.4V nominal rechargeable battery pack (e.g., 2S LiPo, dual 18650 Li-ion cells, or equivalent) with a physical slide/rocker power switch.
6.  **Prototyping:** Breadboard, terminal blocks, and miscellaneous jumper wires.

---

## 2. Verified Wiring Schema

### A. System Power & Common Ground Network

All ground lines (except the actual motor phase wires) must connect to a unified reference point (such as a shared breadboard rail) to complete the control logic circuit.

| Component / Pin | Connected to | Destination Pin / Wire | Purpose |
| :--- | :--- | :--- | :--- |
| **Battery Pack (+)** | Board L & Board R | `VM` | High-current motor supply (7.4V nominal) |
| **Battery Pack (-)** | Board L & Board R | `GND` | High-current motor ground return |
| **Pico 3V3 (Pin 36)** | Board L & Board R | `VCC` & `STBY` | Powers driver logic chips & wakes them from standby |
| **Pico 3V3 (Pin 36)** | All 4 Encoders & MPU-6050 | `VCC` (or Pin 2 on encoders) | 3.3V logic power for sensors |
| **Pico GND (e.g. Pin 3)** | Board L | Logic `gnd` | Establishes control signal reference pool |
| **Board L logic `gnd`** | Board R | Logic `gnd` | **Daisy-chained** ground reference between drivers |
| **Pico GND (e.g. Pin 3)** | All 4 Encoders & MPU-6050 | `GND` (or Pin 5 on encoders) | Ground return for sensor logic |

> ⚠️ **CRITICAL WARNING:** Do **not** connect the actual motor output terminal wires (e.g., `ao1`, `ao2`) to ground. Doing so will create an immediate short-circuit that will destroy the motor driver.

---

### B. Pico to Motor Drivers (Logic Control)

Small jumper wires are used on the driver boards to bridge the A and B channels together. This forces both channels to mirror the exact same Pico command, allowing one driver board to power both motors on its side of the robot.

*   **Board L (Left Driver):** Physically bridge `PWMA` ➔ `PWMB`, `AIN1` ➔ `BIN1`, and `AIN2` ➔ `BIN2`.
*   **Board R (Right Driver):** Physically bridge `PWMA` ➔ `PWMB`, `AIN1` ➔ `BIN1`, and `AIN2` ➔ `BIN2`.

| Pico Pin | Board L (Left Driver) | Board R (Right Driver) | Hardware Function | Purpose |
| :--- | :--- | :--- | :--- | :--- |
| **GP12** | `PWMA` (bridged to `PWMB`) | -- | PWM Slice 6, Channel A | Left Side Speed Control |
| **GP10** | `AIN1` (bridged to `BIN1`) | -- | GPIO Output | Left Side Direction Pin 1 |
| **GP11** | `AIN2` (bridged to `BIN2`) | -- | GPIO Output | Left Side Direction Pin 2 |
| **GP13** | -- | `PWMA` (bridged to `PWMB`) | PWM Slice 6, Channel B | Right Side Speed Control |
| **GP14** | -- | `AIN1` (bridged to `BIN1`) | GPIO Output | Right Side Direction Pin 1 |
| **GP15** | -- | `AIN2` (bridged to `BIN2`) | GPIO Output | Right Side Direction Pin 2 |

---

### C. Motor Drivers to Motors (High Current Output)

These connections deliver raw power directly to the motors. If a motor turns backward relative to the other on its side, simply swap the order of the two wires in its green terminal block.

| Driver Board | Output Terminal | Connects to Motor | Motor Pin / Terminal | Default Polarity |
| :--- | :--- | :--- | :--- | :--- |
| **Board L** | `ao1` | Front-Left | Pin 6 | Positive (+) |
| **Board L** | `ao2` | Front-Left | Pin 1 | Negative (-) |
| **Board L** | `bo1` | Rear-Left | Pin 6 | Positive (+) |
| **Board L** | `bo2` | Rear-Left | Pin 1 | Negative (-) |
| **Board R** | `ao1` | Front-Right | Pin 6 | Positive (+) |
| **Board R** | `ao2` | Front-Right | Pin 1 | Negative (-) |
| **Board R** | `bo1` | Rear-Right | Pin 6 | Positive (+) |
| **Board R** | `bo2` | Rear-Right | Pin 1 | Negative (-) |

---

### D. Encoders & IMU to Pico (Sensor Inputs)

The PIO module utilizes consecutive pin pairs (`GP(X)` and `GP(X+1)`) for hardware state evaluation. The MPU-6050 communicates over standard hardware I2C0.

| Sensor / Location | Physical Connection | Pico Pin Number | Pico GPIO Designation | Assigned Driver Block |
| :--- | :--- | :--- | :--- | :--- |
| **Front-Left Encoder** | Phase A & Phase B | Pin 4 & Pin 5 | `GP2` & `GP3` | PIO0, State Machine 0 |
| **Rear-Left Encoder** | Phase A & Phase B | Pin 6 & Pin 7 | `GP4` & `GP5` | PIO0, State Machine 1 |
| **Front-Right Encoder** | Phase A & Phase B | Pin 9 & Pin 10 | `GP6` & `GP7` | PIO0, State Machine 2 |
| **Rear-Right Encoder** | Phase A & Phase B | Pin 11 & Pin 12 | `GP8` & `GP9` | PIO0, State Machine 3 |
| **MPU-6050 SDA** | I2C Serial Data | **Pin 21** | **`GP16`** | Hardware I2C0 Block |
| **MPU-6050 SCL** | I2C Serial Clock | **Pin 22** | **`GP17`** | Hardware I2C0 Block |

---

## 3. Communication Protocol

All communications over the USB serial interface are wrapped using **COBS (Consistent Overhead Byte Stuffing)** to guarantee that `0x00` is used strictly as a packet delimiter, preventing frame corruption. Every packet contains a **CRC16-CCITT** trailing word to prevent the consumption of noise-distorted bytes.

### A. Host to Pico: `VelocityTarget` Struct (10 Bytes)

Sent by the Linux state estimator to command speeds.

```cpp
struct __attribute__((packed)) VelocityTarget {
    float left_rad_sec;   // Command Left speed (rad/s)
    float right_rad_sec;  // Command Right speed (rad/s)
    uint16_t crc16;       // Checksum on the 2 floats
};
```

### B. Pico to Host: `EncoderIMUTelemetry` Struct (46 Bytes)

Streamed by the Pico at 50Hz for real-time sensor fusion.

```cpp
struct __attribute__((packed)) EncoderIMUTelemetry {
    uint32_t timestamp_us; // Hardware microsecond timestamp since boot
    int32_t count_fl;      // Front-Left encoder count
    int32_t count_rl;      // Rear-Left encoder count
    int32_t count_fr;      // Front-Right encoder count
    int32_t count_rr;      // Rear-Right encoder count
    float accel_x;         // Accelerometer X (g-forces: earth gravity ~ 1.0g)
    float accel_y;         // Accelerometer Y (g-forces)
    float accel_z;         // Accelerometer Z (g-forces)
    float gyro_x;          // Gyroscope X (rad/s)
    float gyro_y;          // Gyroscope Y (rad/s)
    float gyro_z;          // Gyroscope Z (rad/s)
    uint16_t crc16;        // Checksum on the first 44 bytes
};
```

---

## 4. Grounding & Noise Isolation Best Practices

To protect the Pico and encoders from high-frequency electrical noise and voltage spikes generated by H-bridge switching:

1.  **Keep High-Current Grounds Separate:** Route the heavy battery negative line directly into the high-power `GND` terminal of the driver boards using thick wire.
2.  **Daisy-Chain Signal Grounds:** Connect the logic-level grounds using a neat daisy chain:
    `Pico GND` ➔ `Board L logic gnd` ➔ `Board R logic gnd`
    This provides a clean, noise-free voltage reference without exposing the Pico to high motor current return loops.
