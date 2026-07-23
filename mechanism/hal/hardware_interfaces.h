// interfaces.hpp
#pragma once

// Interface for getting driving commands (Hardware Keyboard, ZMQ, or Mock)
class IInputProvider {
 public:
  virtual ~IInputProvider() = default;
  virtual bool readChar(char &ch) = 0;
};

// Interface for sending velocity to the Pico (Hardware Serial or Mock)
class IMotorDriver {
 public:
  virtual ~IMotorDriver() = default;
  virtual void sendVelocityCommand(float left_rad_sec, float right_rad_sec) = 0;
};

// Interface for broadcasting telemetry (ZMQ Publisher or Mock)
class ITelemetryPublisher {
 public:
  virtual ~ITelemetryPublisher() = default;
  virtual void publishAction(float left_rad_sec, float right_rad_sec) = 0;
};
