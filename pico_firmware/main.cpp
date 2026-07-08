#include <stdio.h>
#include <string.h>

#include "hardware/i2c.h"
#include "hardware/pio.h"
#include "hardware/pwm.h"
#include "hardware/timer.h"
#include "pico/stdlib.h"
#include "quadrature_encoder.pio.h"

// 1. Target Velocity Struct (from Host to Pico)
struct __attribute__((packed)) VelocityTarget {
  float left_rad_sec;
  float right_rad_sec;
  uint16_t crc16;
};

// 2. Telemetry Packet Struct (from Pico to Host)
struct __attribute__((packed)) EncoderIMUTelemetry {
  uint32_t timestamp_us;  // Hardware microsecond timestamp since boot
  int32_t count_fl;
  int32_t count_rl;
  int32_t count_fr;
  int32_t count_rr;
  float accel_x;
  float accel_y;
  float accel_z;
  float gyro_x;
  float gyro_y;
  float gyro_z;
  uint16_t crc16;
};

// Safety configuration for desktop testing.
const bool DESK_TEST_MODE = false;

// Hardware Pin Definitions - Aligned with requested wiring setup
// Encoders (Quadrature Phase A Pins)
const uint PIN_ENC_FL_A =
    2;  // Front-Left Encoder: GP2 (Phase A) & GP3 (Phase B)
const uint PIN_ENC_RL_A =
    4;  // Rear-Left Encoder:  GP4 (Phase A) & GP5 (Phase B)
const uint PIN_ENC_FR_A =
    6;  // Front-Right Encoder: GP6 (Phase A) & GP7 (Phase B)
const uint PIN_ENC_RR_A =
    8;  // Rear-Right Encoder:  GP8 (Phase A) & GP9 (Phase B)

// Left Motor Driver Control (Board L)
const uint PIN_PWM_LEFT =
    12;  // Left PWM Speed Control: GP12 (PWMA bridged to PWMB)
const uint PIN_DIR_L1 =
    10;  // Left Direction Pin 1:   GP10 (AIN1 bridged to BIN1)
const uint PIN_DIR_L2 =
    11;  // Left Direction Pin 2:   GP11 (AIN2 bridged to BIN2)

// Right Motor Driver Control (Board R)
const uint PIN_PWM_RIGHT =
    13;  // Right PWM Speed Control: GP13 (PWMA bridged to PWMB)
const uint PIN_DIR_R1 =
    14;  // Right Direction Pin 1:   GP14 (AIN1 bridged to BIN1)
const uint PIN_DIR_R2 =
    15;  // Right Direction Pin 2:   GP15 (AIN2 bridged to BIN2)

// MPU-6050 IMU Pin Definitions (I2C0 Block)
const uint PIN_SDA = 16;     // I2C0 SDA: GP16 (Pico Pin 21)
const uint PIN_SCL = 17;     // I2C0 SCL: GP17 (Pico Pin 22)
const uint MPU_ADDR = 0x68;  // Default MPU-6050 I2C Address

// 3. CRC16 Validator
uint16_t calculate_crc16(const uint8_t *data, size_t length) {
  uint16_t crc = 0xFFFF;
  for (size_t i = 0; i < length; ++i) {
    crc ^= data[i] << 8;
    for (uint8_t j = 0; j < 8; ++j) {
      if (crc & 0x8000)
        crc = (crc << 1) ^ 0x1021;
      else
        crc <<= 1;
    }
  }
  return crc;
}

// 4. COBS Encoding Algorithm (removes all 0x00 bytes and appends 0x00
// delimiter)
size_t cobs_encode(const uint8_t *ptr, size_t length, uint8_t *dst) {
  size_t read_index = 0, write_index = 1, code_index = 0;
  uint8_t code = 1;

  while (read_index < length) {
    if (ptr[read_index] == 0) {
      dst[code_index] = code;
      code = 1;
      code_index = write_index++;
      read_index++;
    } else {
      dst[write_index++] = ptr[read_index++];
      code++;
      if (code == 0xFF) {
        dst[code_index] = code;
        code = 1;
        code_index = write_index++;
      }
    }
  }
  dst[code_index] = code;
  dst[write_index++] = 0x00;  // Frame delimiter
  return write_index;
}

// 5. COBS Decoding Algorithm
size_t cobs_decode(const uint8_t *ptr, size_t length, uint8_t *dst) {
  size_t read_index = 0, write_index = 0, code = 0, i = 0;
  while (read_index < length) {
    code = ptr[read_index];
    if (read_index + code > length && code != 1) return 0;  // Malformed
    read_index++;
    for (i = 1; i < code; i++) {
      dst[write_index++] = ptr[read_index++];
    }
    if (code != 0xFF && read_index != length) {
      dst[write_index++] = 0;
    }
  }
  return write_index;
}

// 6. Transmit raw telemetry packet over stdio
void send_telemetry(const EncoderIMUTelemetry &telemetry) {
  uint8_t raw_buffer[sizeof(EncoderIMUTelemetry)];
  memcpy(raw_buffer, &telemetry, sizeof(EncoderIMUTelemetry));

  // Allocate space for COBS encoded buffer (+ 2 bytes overhead/delimiter)
  uint8_t encoded_buffer[sizeof(EncoderIMUTelemetry) + 2];
  size_t encoded_len =
      cobs_encode(raw_buffer, sizeof(EncoderIMUTelemetry), encoded_buffer);

  // Send raw bytes using non-converting SDK output
  for (size_t i = 0; i < encoded_len; ++i) {
    putchar_raw(encoded_buffer[i]);
  }
}

int main() {
  stdio_init_all();

  // Initialize PIO and load the quadrature encoder program on pio0
  PIO pio = pio0;
  pio_add_program(pio, &quadrature_encoder_program);

  // Initialize all 4 encoders on separate state machines of pio0
  quadrature_encoder_program_init(pio, 0, PIN_ENC_FL_A, 0);
  quadrature_encoder_program_init(pio, 1, PIN_ENC_RL_A, 0);
  quadrature_encoder_program_init(pio, 2, PIN_ENC_FR_A, 0);
  quadrature_encoder_program_init(pio, 3, PIN_ENC_RR_A, 0);

  // Initialize PWM functions for GP12 (Left) and GP13 (Right)
  gpio_set_function(PIN_PWM_LEFT, GPIO_FUNC_PWM);
  gpio_set_function(PIN_PWM_RIGHT, GPIO_FUNC_PWM);

  // GP12 and GP13 share PWM Slice 6
  uint slice_num = pwm_gpio_to_slice_num(PIN_PWM_LEFT);
  pwm_set_wrap(slice_num, 255);
  pwm_set_clkdiv(slice_num, 64.0f);  // 7.63 kHz switching frequency
  pwm_set_enabled(slice_num, true);

  // Resolve channels dynamically
  uint chan_left = pwm_gpio_to_channel(PIN_PWM_LEFT);
  uint chan_right = pwm_gpio_to_channel(PIN_PWM_RIGHT);

  // Initialize Direction GPIO pins as outputs
  gpio_init(PIN_DIR_L1);
  gpio_set_dir(PIN_DIR_L1, GPIO_OUT);
  gpio_init(PIN_DIR_L2);
  gpio_set_dir(PIN_DIR_L2, GPIO_OUT);
  gpio_init(PIN_DIR_R1);
  gpio_set_dir(PIN_DIR_R1, GPIO_OUT);
  gpio_init(PIN_DIR_R2);
  gpio_set_dir(PIN_DIR_R2, GPIO_OUT);

  // Initialize I2C0 for MPU-6050 IMU on GP16 (SDA) / GP17 (SCL)
  i2c_init(i2c0, 400 * 1000);  // 400 kHz Fast Mode
  gpio_set_function(PIN_SDA, GPIO_FUNC_I2C);
  gpio_set_function(PIN_SCL, GPIO_FUNC_I2C);
  gpio_pull_up(PIN_SDA);
  gpio_pull_up(PIN_SCL);

  // Quick I2C Bus Scan to find any connected device on startup
  printf("\n=== I2C BUS SCAN ===\n");
  for (int addr = 1; addr < 127; ++addr) {
    uint8_t rx_val;
    // Perform a 1-byte read test
    int res = i2c_read_blocking(i2c0, addr, &rx_val, 1, false);
    if (res >= 0) {
      printf("I2C Device Found at Address: 0x%02X\n", addr);
    }
  }
  printf("====================\n\n");

  // Graceful IMU connection scan
  bool imu_present = false;
  uint8_t active_imu_addr = 0x68;
  uint8_t wake_cmd[] = {0x6B, 0x00};

  // Test both 0x68 and 0x69
  if (i2c_write_blocking(i2c0, 0x68, wake_cmd, 2, false) >= 0) {
    imu_present = true;
    active_imu_addr = 0x68;
  } else if (i2c_write_blocking(i2c0, 0x69, wake_cmd, 2, false) >= 0) {
    imu_present = true;
    active_imu_addr = 0x69;
  }

  // Default target speeds set to 0.0 rad/s
  VelocityTarget target = {0.0f, 0.0f, 0};

  // Serial receive buffers
  uint8_t rx_buffer[32];
  uint rx_index = 0;

  while (true) {
    uint64_t start_time = time_us_64();

    // A. Read USB Serial byte-by-byte (Non-blocking)
    int c;
    while ((c = getchar_timeout_us(0)) != PICO_ERROR_TIMEOUT) {
      if (c == 0x00) {
        // We hit the COBS delimiter! Process the frame.
        uint8_t decoded[sizeof(VelocityTarget)];
        size_t decoded_len = cobs_decode(rx_buffer, rx_index, decoded);

        if (decoded_len == sizeof(VelocityTarget)) {
          VelocityTarget incoming = *(VelocityTarget *)decoded;

          // Validate CRC before trusting the math
          uint16_t check_crc =
              calculate_crc16((uint8_t *)&incoming, sizeof(float) * 2);
          if (check_crc == incoming.crc16) {
            target = incoming;
          }
        }
        rx_index = 0;  // Reset buffer for the next frame
      } else {
        // Add byte to buffer
        if (rx_index < sizeof(rx_buffer)) {
          rx_buffer[rx_index++] = (uint8_t)c;
        } else {
          rx_index = 0;  // Buffer overflow, drop garbage
        }
      }
    }

    // B. Read Sensors and Pack Telemetry Packet
    EncoderIMUTelemetry telemetry = {0};
    telemetry.timestamp_us = time_us_32();

    // Background auto-detection: if IMU is not present, try to initialize it
    // once per second (every 50 loops at 50Hz)
    static uint32_t imu_detect_counter = 0;
    if (!imu_present) {
      imu_detect_counter++;
      if (imu_detect_counter >= 50) {
        imu_detect_counter = 0;
        uint8_t wake_cmd[] = {0x6B, 0x00};
        if (i2c_write_blocking(i2c0, 0x68, wake_cmd, 2, false) >= 0) {
          imu_present = true;
          active_imu_addr = 0x68;
        } else if (i2c_write_blocking(i2c0, 0x69, wake_cmd, 2, false) >= 0) {
          imu_present = true;
          active_imu_addr = 0x69;
        }
      }
    }

    // Read and drain the PIO encoder FIFO buffers
    telemetry.count_fl = quadrature_encoder_get_count(pio, 0);
    telemetry.count_rl = quadrature_encoder_get_count(pio, 1);
    telemetry.count_fr = quadrature_encoder_get_count(pio, 2);
    telemetry.count_rr = quadrature_encoder_get_count(pio, 3);

    // Read MPU-6050 Accelerometer & Gyroscope data if connected
    if (imu_present) {
      uint8_t reg = 0x3B;  // Accel X High register start address
      uint8_t raw_data[14];
      if (i2c_write_blocking(i2c0, active_imu_addr, &reg, 1, true) >= 0) {
        if (i2c_read_blocking(i2c0, active_imu_addr, raw_data, 14, false) >=
            0) {
          // Combine high and low bytes for 16-bit signed registers
          int16_t raw_acc_x = (raw_data[0] << 8) | raw_data[1];
          int16_t raw_acc_y = (raw_data[2] << 8) | raw_data[3];
          int16_t raw_acc_z = (raw_data[4] << 8) | raw_data[5];
          int16_t raw_gyro_x = (raw_data[8] << 8) | raw_data[9];
          int16_t raw_gyro_y = (raw_data[10] << 8) | raw_data[11];
          int16_t raw_gyro_z = (raw_data[12] << 8) | raw_data[13];

          // Convert raw LSB readings to standard physical units
          telemetry.accel_x = (float)raw_acc_x / 16384.0f;  // g-force scale
          telemetry.accel_y = (float)raw_acc_y / 16384.0f;
          telemetry.accel_z = (float)raw_acc_z / 16384.0f;
          telemetry.gyro_x = ((float)raw_gyro_x / 131.0f) *
                             (3.14159265f / 180.0f);  // rad/sec scale
          telemetry.gyro_y =
              ((float)raw_gyro_y / 131.0f) * (3.14159265f / 180.0f);
          telemetry.gyro_z =
              ((float)raw_gyro_z / 131.0f) * (3.14159265f / 180.0f);
        }
      }
    }

    // Calculate and append CRC16 check code to telemetry packet
    telemetry.crc16 = calculate_crc16(
        (uint8_t *)&telemetry, sizeof(EncoderIMUTelemetry) - sizeof(uint16_t));

    // Transmit the telemetry packet to the host
    send_telemetry(telemetry);

    // C. Actuate Motors (Open-loop velocity-to-PWM mapping)

    // --- Left Side Motors ---
    float left_speed = target.left_rad_sec;
    uint16_t left_pwm = 0;
    if (left_speed > 0.01f) {
      gpio_put(PIN_DIR_L1, 1);
      gpio_put(PIN_DIR_L2, 0);
      float val = (left_speed / 15.0f) * 255.0f;
      left_pwm = (uint16_t)(val > 255.0f ? 255.0f : val);
    } else if (left_speed < -0.01f) {
      gpio_put(PIN_DIR_L1, 0);
      gpio_put(PIN_DIR_L2, 1);
      float val = (-left_speed / 15.0f) * 255.0f;
      left_pwm = (uint16_t)(val > 255.0f ? 255.0f : val);
    } else {
      gpio_put(PIN_DIR_L1, 0);
      gpio_put(PIN_DIR_L2, 0);
      left_pwm = 0;
    }

    // --- Right Side Motors ---
    float right_speed = target.right_rad_sec;
    uint16_t right_pwm = 0;
    if (right_speed > 0.01f) {
      gpio_put(PIN_DIR_R1, 1);
      gpio_put(PIN_DIR_R2, 0);
      float val = (right_speed / 15.0f) * 255.0f;
      right_pwm = (uint16_t)(val > 255.0f ? 255.0f : val);
    } else if (right_speed < -0.01f) {
      gpio_put(PIN_DIR_R1, 0);
      gpio_put(PIN_DIR_R2, 1);
      float val = (-right_speed / 15.0f) * 255.0f;
      right_pwm = (uint16_t)(val > 255.0f ? 255.0f : val);
    } else {
      gpio_put(PIN_DIR_R1, 0);
      gpio_put(PIN_DIR_R2, 0);
      right_pwm = 0;
    }

    // Apply desk test safety limits
    if (DESK_TEST_MODE) {
      if (left_pwm > 65) left_pwm = 65;
      if (right_pwm > 65) right_pwm = 65;
    }

    // Set the active duty cycles
    pwm_set_chan_level(slice_num, chan_left, left_pwm);
    pwm_set_chan_level(slice_num, chan_right, right_pwm);

    // D. Enforce strict 50Hz timing (20,000 microseconds)
    uint64_t execution_time = time_us_64() - start_time;
    if (execution_time < 20000) {
      sleep_us(20000 - execution_time);
    }
  }
}