#include <iostream>
#include <cassert>
#include <thread>
#include <vector>
#include <chrono>
#include <cmath>
#include <atomic>
#include "pico_interface/TelemetryDefs.hpp"
#include "pico_interface/TelemetryQueue.hpp"
#include "pico_interface/StateEstimator.hpp"

// Simple testing assert macro with detailed logging
#define TEST_ASSERT(cond, msg) \
    do { \
        if (!(cond)) { \
            std::cerr << "\n[TEST FAILURE] " << __FILE__ << ":" << __LINE__ \
                      << " -> " << msg << " (Assertion: " << #cond << " failed)\n"; \
            std::exit(1); \
        } \
    } while (0)

// 1. Threading and Deadlock Test
void test_queue_threading() {
    std::clog << "[Test] Running Threading and Queue Deadlock Stress Test...\n";

    TelemetryQueue<EncoderIMUTelemetry> queue(50); // Small size to force evictions
    std::atomic<bool> run_test{true};
    std::atomic<int> items_produced{0};
    std::atomic<int> items_consumed{0};

    const int num_producers = 4;
    const int num_consumers = 4;

    std::vector<std::thread> producers;
    std::vector<std::thread> consumers;

    // Start consumers
    for (int i = 0; i < num_consumers; ++i) {
        consumers.emplace_back([&queue, &run_test, &items_consumed]() {
            while (run_test) {
                EncoderIMUTelemetry tel;
                if (queue.pop(tel, std::chrono::milliseconds(10))) {
                    items_consumed++;
                }
            }
            // Drain remaining
            EncoderIMUTelemetry tel;
            while (queue.pop(tel, std::chrono::milliseconds(0))) {
                items_consumed++;
            }
        });
    }

    // Start producers pushing rapidly
    for (int i = 0; i < num_producers; ++i) {
        producers.emplace_back([&queue, &run_test, &items_produced]() {
            while (run_test) {
                EncoderIMUTelemetry tel{};
                tel.timestamp_us = 1000;
                queue.push(tel);
                items_produced++;
                std::this_thread::sleep_for(std::chrono::microseconds(50));
            }
        });
    }

    // Let them run for 1 second to stress-test concurrency locks
    std::this_thread::sleep_for(std::chrono::seconds(1));

    // Signal stop and join
    run_test = false;
    for (auto& t : producers) {
        if (t.joinable()) {
            t.join();
        }
    }
    for (auto& t : consumers) {
        if (t.joinable()) {
            t.join();
        }
    }
    producers.clear();
    consumers.clear();

    std::clog << "  - Stress test complete without deadlock!\n";
    std::clog << "  - Total Produced: " << items_produced.load() \
              << ", Total Consumed: " << items_consumed.load() << "\n";
    TEST_ASSERT(items_produced > 0, "Producers should have pushed items");
    TEST_ASSERT(items_consumed > 0, "Consumers should have popped items");
    std::clog << "[Pass] Threading test completed successfully.\n\n";
}

// 2. EKF Mathematical Correctness Tests
void test_ekf_initialization() {
    std::clog << "[Test] Running EKF Initialization Test...\n";
    TelemetryQueue<EncoderIMUTelemetry> queue;
    StateEstimator estimator(queue);

    EncoderIMUTelemetry t1{};
    t1.timestamp_us = 1000000; // 1 sec
    t1.count_fl = 100;
    t1.count_rl = 100;
    t1.count_fr = 100;
    t1.count_rr = 100;

    estimator.process_telemetry(t1);

    RobotPose pose = estimator.get_pose();
    TEST_ASSERT(pose.x == 0.0, "Initial X must be 0");
    TEST_ASSERT(pose.y == 0.0, "Initial Y must be 0");
    TEST_ASSERT(pose.theta == 0.0, "Initial Theta must be 0");
    std::clog << "[Pass] EKF initialized correctly on first packet.\n\n";
}

void test_ekf_straight_line() {
    std::clog << "[Test] Running EKF Straight Line Prediction Test...\n";
    TelemetryQueue<EncoderIMUTelemetry> queue;
    StateEstimator estimator(queue);

    // Force-initialize estimator state to (0,0,0) with encoder counts at 1000
    estimator.test_initialize(0.0, 0.0, 0.0, 1000000, 1000.0, 1000.0);

    // Simulate moving forward by 1440 ticks (1 full wheel rotation)
    // Ticks to distance: 1 rotation = 2 * pi * R = 2 * pi * 0.033 = 0.207345 m
    EncoderIMUTelemetry t2{};
    t2.timestamp_us = 1100000; // dt = 0.1 sec
    t2.count_fl = 2440;
    t2.count_rl = 2440;
    t2.count_fr = 2440;
    t2.count_rr = 2440;
    t2.gyro_z = 0.0; // Straight line

    estimator.process_telemetry(t2);

    RobotPose pose = estimator.get_pose();
    double expected_dist = 2.0 * PI * 0.033; // ~0.2073
    std::clog << "  - Final Pose: X=" << pose.x << ", Y=" << pose.y << ", Theta=" << pose.theta << "\n";
    TEST_ASSERT(std::abs(pose.x - expected_dist) < 0.001, "X displacement is wrong");
    TEST_ASSERT(std::abs(pose.y) < 1e-9, "Y displacement should be 0");
    TEST_ASSERT(std::abs(pose.theta) < 1e-9, "Theta should be 0");
    std::clog << "[Pass] Straight line EKF prediction verified.\n\n";
}

void test_ekf_pure_rotation() {
    std::clog << "[Test] Running EKF Pure Rotation Prediction Test...\n";
    TelemetryQueue<EncoderIMUTelemetry> queue;
    StateEstimator estimator(queue);

    estimator.test_initialize(0.0, 0.0, 0.0, 1000000, 1000.0, 1000.0);

    // Pivot Turn (turn counter-clockwise):
    // Left wheel moves backward, Right wheel moves forward.
    // For 1 full wheel rotation difference: Left -1 rotation (-1440 ticks), Right +1 rotation (+1440 ticks)
    // Delta left ticks = -1440, Delta right ticks = +1440
    // d_s_L = -0.2073, d_s_R = +0.2073
    // d_s = 0.0 (No translation!)
    // d_theta = (d_s_R - d_s_L) / Wheelbase = (0.2073 - (-0.2073)) / 0.16 = 0.41469 / 0.16 = 2.5918 rad
    EncoderIMUTelemetry t2{};
    t2.timestamp_us = 1100000; // dt = 0.1s
    t2.count_fl = 1000 - 1440;
    t2.count_rl = 1000 - 1440;
    t2.count_fr = 1000 + 1440;
    t2.count_rr = 1000 + 1440;
    t2.gyro_z = 25.918; // Make IMU yaw rate match exactly to avoid innovation corrections

    estimator.process_telemetry(t2);

    RobotPose pose = estimator.get_pose();
    double expected_theta = (2.0 * 2.0 * PI * 0.033) / 0.16; // 2.5918
    std::clog << "  - Final Pose: X=" << pose.x << ", Y=" << pose.y << ", Theta=" << pose.theta << "\n";
    TEST_ASSERT(std::abs(pose.x) < 1e-9, "X should remain 0");
    TEST_ASSERT(std::abs(pose.y) < 1e-9, "Y should remain 0");
    TEST_ASSERT(std::abs(pose.theta - expected_theta) < 0.001, "Rotation heading is wrong");
    std::clog << "[Pass] Pure rotation EKF prediction verified.\n\n";
}

void test_ekf_sensor_fusion() {
    std::clog << "[Test] Running EKF Sensor Fusion (Correction) Test...\n";
    TelemetryQueue<EncoderIMUTelemetry> queue;
    StateEstimator estimator(queue);

    estimator.test_initialize(0.0, 0.0, 0.0, 1000000, 1000.0, 1000.0);

    // We introduce a mismatch!
    // Encoder says we turned by 0.5 rad, but IMU Gyro says we turned by 1.0 rad.
    // Let's compute exact ticks for a 0.5 rad turn:
    // d_theta = (d_s_R - d_s_L) / Wheelbase = 0.5 => d_s_R - d_s_L = 0.5 * 0.16 = 0.08 m
    // Let d_s_R = 0.04 m, d_s_L = -0.04 m (d_s = 0.0)
    // Ticks = d_s / (2 * pi * R) * TICKS_PER_REV = 0.04 / (2 * pi * 0.033) * 1440 = 277.78 ticks
    EncoderIMUTelemetry t2{};
    t2.timestamp_us = 1100000; // dt = 0.1s
    t2.count_fl = 1000 - 278;
    t2.count_rl = 1000 - 278;
    t2.count_fr = 1000 + 278;
    t2.count_rr = 1000 + 278;
    // IMU Yaw rate: we want integrated IMU yaw rate to be 1.0 rad.
    // gyro_z * dt = 1.0 => gyro_z = 1.0 / 0.1 = 10.0 rad/s
    t2.gyro_z = 10.0;

    double initial_cov[3][3];
    estimator.get_covariance(initial_cov);

    estimator.process_telemetry(t2);

    RobotPose pose = estimator.get_pose();
    
    // The EKF should fuse them!
    // Encoder heading prediction: d_theta_enc = (2 * (278/1440) * 2 * pi * 0.033) / 0.16 = 0.5003 rad
    // IMU integrated heading: imu_theta = 1.0 rad
    // Since Kalman Gain is non-zero, the posterior heading should be strictly between 0.5003 and 1.0.
    std::clog << "  - Encoder Only: ~0.5003 rad, IMU Only: 1.0000 rad\n";
    std::clog << "  - EKF Fused Heading: " << pose.theta << " rad\n";
    
    TEST_ASSERT(pose.theta > 0.5003 && pose.theta < 1.0, "Fused heading must lie between encoder prediction and IMU update");
    std::clog << "[Pass] EKF sensor fusion correction verified.\n\n";
}

int main() {
    std::clog << "========================================================\n";
    std::clog << "          SIMPLE CONTROL UNIT TEST SUITE (C++20)        \n";
    std::clog << "========================================================\n\n";

    test_queue_threading();
    test_ekf_initialization();
    test_ekf_straight_line();
    test_ekf_pure_rotation();
    test_ekf_sensor_fusion();

    std::clog << "========================================================\n";
    std::clog << "              ALL TESTS PASSED SUCCESSFULLY!            \n";
    std::clog << "========================================================\n";
    return 0;
}
