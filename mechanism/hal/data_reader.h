#pragma once
#include <unistd.h>

#include <atomic>
#include <cstring>
#include <string>
#include <thread>

#include "mechanism/common/telemetry.h"
#include "mechanism/common/telemetry_queue.h"

class DataReader {
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
  DataReader(const std::string &port_name,
             TelemetryQueue<EncoderIMUTelemetry> &rx_queue)
      : port_name_(port_name), rx_queue_(rx_queue) {}

  ~DataReader() { stop(); }

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
};
