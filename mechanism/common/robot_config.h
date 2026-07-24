#pragma once

#include <array>
#include <cmath>

namespace config {

// Reusable 6-DOF Transform for any physical sensor mount
struct Transform6DOF {
  double x = 0.0;
  double y = 0.0;
  double z = 0.0;
  double roll = 0.0;
  double pitch = 0.0;
  double yaw = 0.0;

  // Precomputes a flat 4x4 row-major homogeneous transformation matrix.
  std::array<double, 16> to_matrix() const {
    double cy = std::cos(yaw);
    double sy = std::sin(yaw);
    double cp = std::cos(pitch);
    double sp = std::sin(pitch);
    double cr = std::cos(roll);
    double sr = std::sin(roll);

    return {
      cy * cp, cy * sp * sr - sy * cr, cy * sp * cr + sy * sr, x,
      sy * cp, sy * sp * sr + cy * cr, sy * sp * cr - cy * sr, y,
      -sp,     cp * sr,                cp * cr,                z,
      0.0,     0.0,                    0.0,                    1.0
    };
  }
};

// Strict physical drive-train parameters
struct KinematicParameters {
  double wheel_radius = 0.033;
  double wheel_base = 0.16;
  double ticks_per_rev = 1440.0;
  double scale_factor = 1.0;

  double effective_wheel_radius() const { return wheel_radius * scale_factor; }
};

// The composed top-level message type/payload
struct RobotParameters {
  KinematicParameters kinematics;
  Transform6DOF camera_to_base_footprint;
  
  // As your hardware stack grows, append here:
  // Transform6DOF imu_to_base_footprint;
  // NetworkParameters networking;
};

}  // namespace config
