#pragma once
#include <atomic>
#include <chrono>
#include <cmath>
#include <mutex>
#include <thread>

#include "TelemetryDefs.hpp"
#include "TelemetryQueue.hpp"
#include "opentelemetry/trace/provider.h"
#include "opentelemetry/trace/span.h"
#include "opentelemetry/trace/tracer.h"

// Define PI locally for C++17 compatibility
constexpr double PI = 3.14159265358979323846;

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

class StateEstimator {
 private:
  TelemetryQueue<EncoderIMUTelemetry> &rx_queue_;
  std::atomic<bool> run_consumer_{false};
  std::thread consumer_thread_;
  RobotConfig config_;

  // Estimated State and thread-safety
  RobotPose pose_;
  std::mutex pose_mutex_;

  // EKF state variables (Covariance matrix)
  double P_[3][3] = {{0.1, 0.0, 0.0}, {0.0, 0.1, 0.0}, {0.0, 0.0, 0.1}};
  std::mutex covariance_mutex_;

  // Tracking state
  std::atomic<bool> initialized_{false};
  uint32_t last_timestamp_us_ = 0;
  double last_left_ticks_ = 0.0;
  double last_right_ticks_ = 0.0;
  double imu_theta_ = 0.0;  // Integrated IMU heading

  double normalize_angle(double angle) {
    while (angle > PI) angle -= 2.0 * PI;
    while (angle < -PI) angle += 2.0 * PI;
    return angle;
  }

  static bool telemetry_floats_valid(const EncoderIMUTelemetry &tel) {
    return std::isfinite(tel.accel_x) && std::isfinite(tel.accel_y) &&
           std::isfinite(tel.accel_z) && std::isfinite(tel.gyro_x) &&
           std::isfinite(tel.gyro_y) && std::isfinite(tel.gyro_z);
  }

  void consumer_loop() {
    while (run_consumer_) {
      EncoderIMUTelemetry tel;
      if (rx_queue_.pop(tel, std::chrono::milliseconds(100))) {
        process_telemetry(tel);
      }
    }
  }

  // Cached tracer instance for high-frequency zero-overhead tracing
  opentelemetry::nostd::shared_ptr<opentelemetry::trace::Tracer> tracer_;

 public:
  explicit StateEstimator(TelemetryQueue<EncoderIMUTelemetry> &rx_queue,
                          RobotConfig config = RobotConfig{})
      : rx_queue_(rx_queue), config_(config) {
    auto provider = opentelemetry::trace::Provider::GetTracerProvider();
    tracer_ = provider->GetTracer("state_estimator");
  }

  ~StateEstimator() { stop(); }

  void start() {
    if (!run_consumer_) {
      run_consumer_ = true;
      consumer_thread_ = std::thread([this]() { consumer_loop(); });
    }
  }

  void stop() {
    run_consumer_ = false;
    if (consumer_thread_.joinable()) {
      consumer_thread_.join();
    }
  }

  // Process a single telemetry packet and update EKF
  void process_telemetry(const EncoderIMUTelemetry &tel) {
    auto span = tracer_->StartSpan("process_telemetry");

    // Set rich input telemetry attributes on the span
    span->SetAttribute("telemetry.timestamp_us", tel.timestamp_us);
    span->SetAttribute("telemetry.count_fl", tel.count_fl);
    span->SetAttribute("telemetry.count_rl", tel.count_rl);
    span->SetAttribute("telemetry.count_fr", tel.count_fr);
    span->SetAttribute("telemetry.count_rr", tel.count_rr);
    span->SetAttribute("telemetry.accel_x", tel.accel_x);
    span->SetAttribute("telemetry.accel_y", tel.accel_y);
    span->SetAttribute("telemetry.accel_z", tel.accel_z);
    span->SetAttribute("telemetry.gyro_x", tel.gyro_x);
    span->SetAttribute("telemetry.gyro_y", tel.gyro_y);
    span->SetAttribute("telemetry.gyro_z", tel.gyro_z);

    if (!initialized_) {
      last_timestamp_us_ = tel.timestamp_us;
      last_left_ticks_ = (tel.count_fl + tel.count_rl) / 2.0;
      last_right_ticks_ = (tel.count_fr + tel.count_rr) / 2.0;
      imu_theta_ = 0.0;

      std::lock_guard<std::mutex> lock(pose_mutex_);
      pose_ = {0.0, 0.0, 0.0};
      pose_.estimated_at_us =
          std::chrono::duration_cast<std::chrono::microseconds>(
              std::chrono::steady_clock::now().time_since_epoch())
              .count();
      initialized_ = true;

      span->SetAttribute("pose.initialization", true);
      span->SetAttribute("pose.x", 0.0);
      span->SetAttribute("pose.y", 0.0);
      span->SetAttribute("pose.theta", 0.0);
      span->End();
      return;
    }

    // Calculate dt safely
    double dt = 0.0;
    if (tel.timestamp_us >= last_timestamp_us_) {
      dt = (tel.timestamp_us - last_timestamp_us_) / 1000000.0;
    } else {
      // Handle microsecond wrap-around
      dt = ((0xFFFFFFFF - last_timestamp_us_) + tel.timestamp_us + 1) /
           1000000.0;
    }

    if (dt <= 0.0 || dt > 1.0) {
      // Skip invalid dt or long pauses
      last_timestamp_us_ = tel.timestamp_us;
      last_left_ticks_ = (tel.count_fl + tel.count_rl) / 2.0;
      last_right_ticks_ = (tel.count_fr + tel.count_rr) / 2.0;

      span->SetAttribute("pose.error", "invalid_dt");
      span->End();
      return;
    }

    // Guard against invalid geometry that would divide by zero, and reject
    // telemetry with non-finite IMU readings before they poison the EKF.
    if (config_.ticks_per_rev == 0.0 || config_.wheel_base == 0.0 ||
        !telemetry_floats_valid(tel)) {
      last_timestamp_us_ = tel.timestamp_us;
      last_left_ticks_ = (tel.count_fl + tel.count_rl) / 2.0;
      last_right_ticks_ = (tel.count_fr + tel.count_rr) / 2.0;

      span->SetAttribute("pose.error", "invalid_geometry_or_floats");
      span->End();
      return;
    }

    // 1. Calculate encoder displacement deltas
    double left_ticks = (tel.count_fl + tel.count_rl) / 2.0;
    double right_ticks = (tel.count_fr + tel.count_rr) / 2.0;

    double d_ticks_L = left_ticks - last_left_ticks_;
    double d_ticks_R = right_ticks - last_right_ticks_;

    last_left_ticks_ = left_ticks;
    last_right_ticks_ = right_ticks;
    last_timestamp_us_ = tel.timestamp_us;

    double d_s_L = (d_ticks_L / config_.ticks_per_rev) * 2.0 * PI *
                   config_.effective_wheel_radius();
    double d_s_R = (d_ticks_R / config_.ticks_per_rev) * 2.0 * PI *
                   config_.effective_wheel_radius();

    double d_s = (d_s_L + d_s_R) / 2.0;
    double d_theta_enc = (d_s_R - d_s_L) / config_.wheel_base;

    // 2. Integrate IMU Yaw Rate (gyro_z)
    imu_theta_ = normalize_angle(imu_theta_ + tel.gyro_z * dt);

    // 3. EKF Prediction Step (Odometry motion model)
    double current_x, current_y, current_theta;
    {
      std::lock_guard<std::mutex> lock(pose_mutex_);
      current_x = pose_.x;
      current_y = pose_.y;
      current_theta = pose_.theta;
    }

    double pred_theta_mid = current_theta + d_theta_enc / 2.0;
    double pred_x = current_x + d_s * std::cos(pred_theta_mid);
    double pred_y = current_y + d_s * std::sin(pred_theta_mid);
    double pred_theta = normalize_angle(current_theta + d_theta_enc);

    // Process noise matrix Q (diagonal approximations)
    double Q00 = 0.0001;  // x position variance
    double Q11 = 0.0001;  // y position variance
    double Q22 = 0.0004;  // theta heading variance

    // Jacobian Fx
    double f0 = -d_s * std::sin(pred_theta_mid);
    double f1 = d_s * std::cos(pred_theta_mid);

    // Predict covariance: P = Fx * P * Fx^T + Q
    // P_temp = Fx * P
    double P_temp[3][3];
    {
      std::lock_guard<std::mutex> cov_lock(covariance_mutex_);
      for (int j = 0; j < 3; ++j) {
        P_temp[0][j] = P_[0][j] + f0 * P_[2][j];
        P_temp[1][j] = P_[1][j] + f1 * P_[2][j];
        P_temp[2][j] = P_[2][j];
      }
    }

    // P_new = P_temp * Fx^T + Q
    double P_new[3][3];
    P_new[0][0] = P_temp[0][0] + P_temp[0][2] * f0 + Q00;
    P_new[0][1] = P_temp[0][1] + P_temp[0][2] * f1;
    P_new[0][2] = P_temp[0][2];

    P_new[1][0] = P_temp[1][0] + P_temp[1][2] * f0;
    P_new[1][1] = P_temp[1][1] + P_temp[1][2] * f1 + Q11;
    P_new[1][2] = P_temp[1][2];

    P_new[2][0] = P_temp[2][0] + P_temp[2][2] * f0;
    P_new[2][1] = P_temp[2][1] + P_temp[2][2] * f1;
    P_new[2][2] = P_temp[2][2] + Q22;

    // 4. EKF Measurement Update Step
    // Measurement z = imu_theta_
    // Jacobian H = [0, 0, 1]
    // Innovation: y = z - h(X)
    double y = normalize_angle(imu_theta_ - pred_theta);

    // Measurement noise variance R
    double R = 0.0025;  // 0.05 rad std dev squared

    // Innovation covariance: S = H * P_new * H^T + R -> simplifies to
    // P_new[2][2] + R
    double S = P_new[2][2] + R;

    // Kalman Gain: K = P_new * H^T * S^-1
    double K[3];
    K[0] = P_new[0][2] / S;
    K[1] = P_new[1][2] / S;
    K[2] = P_new[2][2] / S;

    // Posterior state estimation
    double est_x = pred_x + K[0] * y;
    double est_y = pred_y + K[1] * y;
    double est_theta = normalize_angle(pred_theta + K[2] * y);

    // Posterior covariance estimation: P = (I - K*H) * P_new
    {
      std::lock_guard<std::mutex> cov_lock(covariance_mutex_);
      for (int i = 0; i < 3; ++i) {
        for (int j = 0; j < 3; ++j) {
          P_[i][j] = P_new[i][j] - K[i] * P_new[2][j];
        }
      }
    }

    // Write the safe pose
    {
      std::lock_guard<std::mutex> lock(pose_mutex_);
      pose_.x = est_x;
      pose_.y = est_y;
      pose_.theta = est_theta;
      pose_.estimated_at_us =
          std::chrono::duration_cast<std::chrono::microseconds>(
              std::chrono::steady_clock::now().time_since_epoch())
              .count();
    }

    // Set output pose attributes on the span and end it
    span->SetAttribute("pose.initialization", false);
    span->SetAttribute("pose.x", est_x);
    span->SetAttribute("pose.y", est_y);
    span->SetAttribute("pose.theta", est_theta);
    span->End();
  }

  RobotPose get_pose() {
    std::lock_guard<std::mutex> lock(pose_mutex_);
    return pose_;
  }

  // Helper to get internal covariance for testing
  void get_covariance(double cov[3][3]) {
    std::lock_guard<std::mutex> cov_lock(covariance_mutex_);
    for (int i = 0; i < 3; ++i) {
      for (int j = 0; j < 3; ++j) {
        cov[i][j] = P_[i][j];
      }
    }
  }

  // Helper to force-initialize for testing math
  void test_initialize(double x, double y, double theta, uint32_t timestamp_us,
                       double left_ticks, double right_ticks) {
    std::lock_guard<std::mutex> lock(pose_mutex_);
    pose_ = {x, y, theta};
    initialized_ = true;
    last_timestamp_us_ = timestamp_us;
    last_left_ticks_ = left_ticks;
    last_right_ticks_ = right_ticks;
    imu_theta_ = theta;
  }
};
