#pragma once

// Robot Configuration parameters for sensor calibration and geometry
struct RobotConfig {
  double wheel_radius =
      0.033;                 // Nominal wheel radius in meters (default: 33mm)
  double wheel_base = 0.16;  // Distance between left and right wheels in meters
                             // (default: 160mm)
  double ticks_per_rev =
      1440.0;  // Encoder ticks per one full wheel revolution (default: 1440.0)
  double scale_factor =
      1.0;  // Linear odometry scale: actual_distance / expected_distance

  // Effective radius used by the EKF, including calibration scale.
  double effective_wheel_radius() const { return wheel_radius * scale_factor; }
};
