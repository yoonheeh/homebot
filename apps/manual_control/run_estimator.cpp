#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <atomic>
#include <chrono>
#include <csignal>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <thread>
#include <vector>

#include "mechanism/common/robot_config.h"
#include "mechanism/common/robot_config_io.h"
#include "mechanism/common/telemetry.h"
#include "mechanism/common/telemetry_queue.h"
#include "mechanism/estimation/state_estimator.h"
#include "mechanism/hal/data_reader.h"

// TODO: Clean this file up

// Global flag to control program lifetime
std::atomic<bool> run_program(true);

// Signal handler for clean termination on Ctrl+C
void signal_handler(int signal) {
  if (signal == SIGINT) {
    run_program = false;
  }
}

// Background thread to print EKF estimated pose at 1Hz
void pose_printer_thread_func(StateEstimator &estimator,
                              std::atomic<bool> &run_printer) {
  while (run_printer) {
    sleep(1);  // 1Hz output
    RobotPose pose = estimator.get_pose();
    uint64_t now_us = std::chrono::duration_cast<std::chrono::microseconds>(
                          std::chrono::steady_clock::now().time_since_epoch())
                          .count();
    double latency_ms = 0.0;
    if (pose.estimated_at_us > 0) {
      latency_ms = (now_us - pose.estimated_at_us) / 1000.0;
    }
    std::clog
        << "\n================ ESTIMATED ROBOT POSE (1Hz) ================\n";
    std::clog << std::fixed << std::setprecision(4);
    std::clog << "Pose X     -> " << std::setw(8) << pose.x << " m\n";
    std::clog << "Pose Y     -> " << std::setw(8) << pose.y << " m\n";
    std::clog << "Pose Theta -> " << std::setw(8) << pose.theta << " rad ("
              << std::setw(6) << std::setprecision(2)
              << (pose.theta * 180.0 / PI) << " deg)\n";
    std::clog << "Latency    -> " << std::setw(8) << std::setprecision(2)
              << latency_ms << " ms\n";
    std::clog
        << "============================================================\n\n";
  }
}

// Background thread to stream EKF estimated pose to stdout
void pose_publisher_thread_func(StateEstimator &estimator,
                                std::atomic<bool> &run_publisher) {
  while (run_publisher) {
    // 20Hz output
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    RobotPose pose = estimator.get_pose();
    uint64_t now_us = std::chrono::duration_cast<std::chrono::microseconds>(
                          std::chrono::steady_clock::now().time_since_epoch())
                          .count();
    double latency_ms = 0.0;
    if (pose.estimated_at_us > 0) {
      latency_ms = (now_us - pose.estimated_at_us) / 1000.0;
    }
    std::cout << pose.x << "," << pose.y << "," << pose.theta << ","
              << latency_ms << std::endl;
  }
}

// RAII class to redirect std::clog to a file inside a designated folder
struct ClogRedirector {
  std::ofstream file;
  std::streambuf *old_buf;

  ClogRedirector(const std::string &path) : old_buf(nullptr) {
    // Create standard "logs" directory if it's missing (POSIX mkdir)
    mkdir("logs", 0777);
    file.open(path);
    if (file.is_open()) {
      old_buf = std::clog.rdbuf(file.rdbuf());
    } else {
      std::cerr << "Warning: Could not open log file: " << path << "\n";
    }
  }

  ~ClogRedirector() {
    if (old_buf) {
      std::clog.rdbuf(old_buf);
    }
  }
};

int main(int argc, char *argv[]) {
  // Redirect std::clog to logs/estimator.log and auto-create directories
  ClogRedirector redirector("logs/estimator.log");

  // Register the POSIX signal handler
  std::signal(SIGINT, signal_handler);

  const char *port_name = "/dev/ttyACM0";
  std::string calib_file = (argc > 1) ? argv[1] : "data/robot_calibration.txt";

  // Configure robot physical calibration parameters
  config::RobotParameters robot_parameters;
  auto calib_config = robot_parameters.kinematics;
  calib_config.wheel_radius = 0.0325;   // 32.5mm calibrated radius
  calib_config.wheel_base = 0.16;       // 160mm default track width
  calib_config.ticks_per_rev = 4320.0;  // Default to 4X Quadrature

  std::clog << "Loading calibration from: " << calib_file << "\n";
  if (!load_robot_config(calib_file, robot_parameters)) {
    std::clog
        << "WARNING: failed to load calibration file. Using default values.\n";
  }

  // Instantiate thread-safe queue and interface objects
  TelemetryQueue<EncoderIMUTelemetry> telemetry_queue;
  DataReader data_reader(port_name, telemetry_queue);
  StateEstimator state_estimator(telemetry_queue, robot_parameters);

  std::clog << "Connecting to Pico on " << port_name
            << " in passive estimation-only mode...\n";
  std::clog << "Using Calibrated Geometry:\n";
  std::clog << std::fixed << std::setprecision(4);
  std::clog << "  - Wheel Radius       : " << calib_config.wheel_radius
            << " m\n";
  std::clog << "  - Scale Factor       : " << calib_config.scale_factor << "\n";
  std::clog << "  - Effective Radius   : "
            << calib_config.effective_wheel_radius() << " m\n";
  std::clog << "  - Base Width         : " << calib_config.wheel_base << " m\n";
  std::clog << "  - Ticks / Rev        : " << calib_config.ticks_per_rev
            << "\n\n";

  if (!data_reader.start()) {
    std::cerr << "Failed to initialize Pico interface. Exiting.\n";
    return 1;
  }
  std::clog << "Connected. Starting State Estimator...\n";
  state_estimator.start();

  // Start 1Hz printer thread (outputs to logs / std::clog)
  std::atomic<bool> run_printer(true);
  std::thread printer_thread(pose_printer_thread_func,
                             std::ref(state_estimator), std::ref(run_printer));

  // Start 20Hz publisher thread (outputs only x,y,theta to std::cout)
  std::atomic<bool> run_publisher(true);
  std::thread publisher_thread(pose_publisher_thread_func,
                               std::ref(state_estimator),
                               std::ref(run_publisher));

  std::clog
      << "\nState estimator running continuously. Press Ctrl+C to stop.\n\n";

  // Block main thread until SIGINT/Ctrl+C is captured
  while (run_program) {
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
  }

  std::clog << "\nSIGINT received. Initiating clean shutdown...\n";

  // Stop and clean up
  run_printer = false;
  run_publisher = false;
  if (printer_thread.joinable()) {
    printer_thread.join();
  }
  if (publisher_thread.joinable()) {
    publisher_thread.join();
  }

  state_estimator.stop();
  data_reader.stop();

  std::clog << "Shutdown complete. Safely stopped threads and closed serial "
               "interface.\n";
  return 0;
}
