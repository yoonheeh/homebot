// TeleopController.hpp
#pragma once
#include "mechanism/hal/hardware_interfaces.h"

class TeleopController {
 public:
  TeleopController(IInputProvider &input, IMotorDriver &motors,
                   ITelemetryPublisher &telemetry)
      : input_(input), motors_(motors), telemetry_(telemetry), running_(true) {}

  bool isRunning() const { return running_; }

  void step() {
    char ch;
    bool w_pressed = false, s_pressed = false, a_pressed = false,
         d_pressed = false;

    while (input_.readChar(ch)) {
      if (ch == 'q' || ch == 'Q') running_ = false;
      if (ch == 'w' || ch == 'W') {
        w_pressed = true;
        s_pressed = false;
      }
      if (ch == 's' || ch == 'S') {
        s_pressed = true;
        w_pressed = false;
      }
      if (ch == 'a' || ch == 'A') {
        a_pressed = true;
        d_pressed = false;
      }
      if (ch == 'd' || ch == 'D') {
        d_pressed = true;
        a_pressed = false;
      }
      if (ch == ' ') w_pressed = s_pressed = a_pressed = d_pressed = false;
    }

    float left_target = 0.0f;
    float right_target = 0.0f;

    if (w_pressed) {
      left_target = LINEAR_SPEED;
      right_target = LINEAR_SPEED;
    } else if (s_pressed) {
      left_target = -LINEAR_SPEED;
      right_target = -LINEAR_SPEED;
    }

    if (d_pressed) {
      left_target += TURN_SPEED;
      right_target -= TURN_SPEED;
    } else if (a_pressed) {
      left_target -= TURN_SPEED;
      right_target += TURN_SPEED;
    }

    left_target *= -1.0f;  // Motor rotating direction inversion

    motors_.sendVelocityCommand(left_target, right_target);
    telemetry_.publishAction(left_target, right_target);
  }

  void stopRobot() { motors_.sendVelocityCommand(0.0f, 0.0f); }

 private:
  IInputProvider &input_;
  IMotorDriver &motors_;
  ITelemetryPublisher &telemetry_;
  bool running_;

  static constexpr float LINEAR_SPEED = 9.0f;
  static constexpr float TURN_SPEED = 8.5f;
};
