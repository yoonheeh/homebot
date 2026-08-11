// HardwareImpl.hpp
#pragma once
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <termios.h>
#include <unistd.h>

#include <string>
#include <zmq.hpp>

#include "hardware_interfaces.h"
#include "mechanism/common/telemetry.h"

class RawTerminalGuard {
 public:
  RawTerminalGuard() {
    tcgetattr(STDIN_FILENO, &oldt_);
    struct termios newt = oldt_;
    newt.c_lflag &= ~(ICANON | ECHO);
    newt.c_cc[VMIN] = 0;
    newt.c_cc[VTIME] = 0;
    tcsetattr(STDIN_FILENO, TCSANOW, &newt);
  }
  ~RawTerminalGuard() { tcsetattr(STDIN_FILENO, TCSANOW, &oldt_); }

 private:
  struct termios oldt_;
};

class KeyboardInput : public IInputProvider {
 public:
  bool readChar(char &ch) override { return read(STDIN_FILENO, &ch, 1) > 0; }
};

// Pico Serial Sender
class SerialMotorDriver : public IMotorDriver {
 public:
  SerialMotorDriver(int fd) : fd_(fd) {}

  void sendVelocityCommand(float left_rad_sec, float right_rad_sec) override {
    VelocityTarget msg;
    msg.left_rad_sec =
        left_rad_sec * -1.0f;  // Motor rotating direction inversion
    msg.right_rad_sec = right_rad_sec;
    msg.crc16 = calculate_crc16(reinterpret_cast<const uint8_t *>(&msg),
                                sizeof(float) * 2);

    uint8_t struct_buffer[sizeof(VelocityTarget)];
    uint8_t encoded_buffer[sizeof(VelocityTarget) + 2];

    std::memcpy(struct_buffer, &msg, sizeof(VelocityTarget));
    size_t encoded_len =
        cobs_encode(struct_buffer, sizeof(VelocityTarget), encoded_buffer);

    write(fd_, encoded_buffer, encoded_len);
  }

 private:
  int fd_;
};

// UDP Motor Driver implementation for Isaac Sim HIL Bridge
class IsaacSimMotorDriver : public IMotorDriver {
 public:
  IsaacSimMotorDriver(const std::string &ip = "127.0.0.1", int port = 5005) {
    sock_ = socket(AF_INET, SOCK_DGRAM, 0);
    server_addr_.sin_family = AF_INET;
    server_addr_.sin_port = htons(port);
    inet_pton(AF_INET, ip.c_str(), &server_addr_.sin_addr);
  }

  ~IsaacSimMotorDriver() override {
    if (sock_ >= 0) {
      close(sock_);
    }
  }

  void sendVelocityCommand(float left_target, float right_target) override {
    std::string payload =
        std::to_string(left_target) + "," + std::to_string(right_target);
    sendto(sock_, payload.c_str(), payload.length(), 0,
           reinterpret_cast<struct sockaddr *>(&server_addr_),
           sizeof(server_addr_));
  }

 private:
  int sock_ = -1;
  struct sockaddr_in server_addr_ {};
};

// ZeroMQ Publisher
class ZmqTelemetryPublisher : public ITelemetryPublisher {
 public:
  ZmqTelemetryPublisher(zmq::socket_t &socket) : socket_(socket) {}

  void publishAction(float left_rad_sec, float right_rad_sec) override {
    std::string payload = "{\"v_left\": " + std::to_string(left_rad_sec) +
                          ", \"v_right\": " + std::to_string(right_rad_sec) +
                          "}";
    zmq::message_t message(payload.size());
    std::memcpy(message.data(), payload.data(), payload.size());
    socket_.send(message, zmq::send_flags::none);
  }

 private:
  zmq::socket_t &socket_;
};
