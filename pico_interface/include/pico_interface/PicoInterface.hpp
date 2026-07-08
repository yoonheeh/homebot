#pragma once
#include <unistd.h>

#include <atomic>
#include <cstring>
#include <string>
#include <thread>

#include "TelemetryDefs.hpp"
#include "TelemetryQueue.hpp"

class PicoInterface {
 private:
  std::string port_name_;
  int fd_ = -1;
  std::atomic<bool> run_reader_{false};
  std::thread reader_thread_;
  TelemetryQueue<EncoderIMUTelemetry> &rx_queue_;

  void reader_loop() {
    uint8_t rx_buffer[128];
    size_t rx_index = 0;

    while (run_reader_) {
      uint8_t c;
      ssize_t bytes_read = read(fd_, &c, 1);
      if (bytes_read > 0) {
        if (c == 0x00) {
          uint8_t decoded[sizeof(EncoderIMUTelemetry)];
          size_t decoded_len =
              cobs_decode(rx_buffer, rx_index, decoded, sizeof(decoded));

          if (decoded_len == sizeof(EncoderIMUTelemetry)) {
            // Validate CRC on the raw decoded bytes before interpreting them.
            uint16_t received_crc;
            std::memcpy(
                &received_crc,
                decoded + sizeof(EncoderIMUTelemetry) - sizeof(uint16_t),
                sizeof(uint16_t));
            uint16_t check_crc = calculate_crc16(
                decoded, sizeof(EncoderIMUTelemetry) - sizeof(uint16_t));
            if (check_crc == received_crc) {
              EncoderIMUTelemetry incoming;
              std::memcpy(&incoming, decoded, sizeof(EncoderIMUTelemetry));
              rx_queue_.push(incoming);
            }
          }
          rx_index = 0;  // Reset buffer
        } else {
          if (rx_index < sizeof(rx_buffer)) {
            rx_buffer[rx_index++] = c;
          } else {
            rx_index = 0;  // Overflow, drop garbage
          }
        }
      } else {
        // Short sleep in microseconds to reduce CPU usage when no characters
        // are ready
        usleep(100);
      }
    }
  }

 public:
  PicoInterface(const std::string &port_name,
                TelemetryQueue<EncoderIMUTelemetry> &rx_queue)
      : port_name_(port_name), rx_queue_(rx_queue) {}

  ~PicoInterface() { stop(); }

  bool start() {
    fd_ = configure_serial(port_name_.c_str());
    if (fd_ < 0) return false;

    run_reader_ = true;
    reader_thread_ = std::thread([this]() { reader_loop(); });
    return true;
  }

  void stop() {
    run_reader_ = false;
    if (reader_thread_.joinable()) {
      reader_thread_.join();
    }
    if (fd_ >= 0) {
      close(fd_);
      fd_ = -1;
    }
  }

  bool send_velocity_target(float left, float right) {
    if (fd_ < 0) return false;

    VelocityTarget msg;
    msg.left_rad_sec = left;
    msg.right_rad_sec = right;
    msg.crc16 = calculate_crc16((uint8_t *)&msg, sizeof(float) * 2);

    uint8_t struct_buffer[sizeof(VelocityTarget)];
    uint8_t encoded_buffer[sizeof(VelocityTarget) + 2];

    std::memcpy(struct_buffer, &msg, sizeof(VelocityTarget));
    size_t encoded_len =
        cobs_encode(struct_buffer, sizeof(VelocityTarget), encoded_buffer);

    ssize_t bytes_written = write(fd_, encoded_buffer, encoded_len);
    return bytes_written == static_cast<ssize_t>(encoded_len);
  }
};
