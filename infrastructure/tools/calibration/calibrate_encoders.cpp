#include <libgen.h>
#include <unistd.h>

#include <atomic>
#include <chrono>
#include <csignal>
#include <cstdlib>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <limits>
#include <thread>
#include <vector>

#include "mechanism/common/robot_config_io.h"
#include "mechanism/common/telemetry.h"

// Global control flag
std::atomic<bool> run_calibration(true);

// Signal handler
void signal_handler(int signal) {
  if (signal == SIGINT) {
    run_calibration = false;
  }
}

void print_usage(const char *prog_name) {
  std::clog << "Usage: " << prog_name
            << " [wheel_radius_m] [wheel_base_m] [ticks_per_rev] [port_name] "
               "[calibration_file]\n";
  std::clog << "  wheel_radius_m    : nominal wheel radius in meters (default: "
               "0.0325)\n";
  std::clog << "  wheel_base_m      : distance between left and right wheels "
               "in meters (default: 0.16)\n";
  std::clog << "  ticks_per_rev     : encoder ticks per wheel revolution "
               "(default: 4320.0)\n";
  std::clog << "  port_name         : serial port (default: /dev/ttyACM0)\n";
  std::clog << "  calibration_file  : output file path (default: "
               "data/robot_calibration.txt)\n";
  std::clog << "\nExample:\n";
  std::clog << "  " << prog_name
            << " 0.033 0.16 4320.0 /dev/ttyACM0 data/robot_calibration.txt\n";
}

// Locate the companion 'control' binary. Under Bazel it lives in the runfiles
// tree; in a CMake/standalone build it is expected to sit next to this binary.
// TODO: refactor this to be assembled in a separate main.cpp file by
// instantiating object rather than calling the binary
std::string find_control_binary(const char *argv0) {
  const char *runfiles_dir = getenv("RUNFILES_DIR");
  if (runfiles_dir) {
    std::vector<std::string> candidates = {
        std::string(runfiles_dir) + "/_main/pico_interface/control",
        std::string(runfiles_dir) + "/homebot/pico_interface/control",
    };
    for (const auto &candidate : candidates) {
      if (access(candidate.c_str(), X_OK) == 0) {
        return candidate;
      }
    }
  }

  // Same directory as this executable (CMake standalone build).
  std::string self_path = argv0;
  std::vector<char> buf(self_path.begin(), self_path.end());
  buf.push_back('\0');
  std::string self_dir = dirname(buf.data());
  std::string same_dir = self_dir + "/control";
  if (access(same_dir.c_str(), X_OK) == 0) {
    return same_dir;
  }

  // Last resort: hope it is on PATH.
  return "control";
}

bool invoke_control(const std::string &control_bin,
                    const std::string &port_name, float left_rad_sec,
                    float right_rad_sec) {
  std::ostringstream cmd;
  cmd << control_bin << " " << std::fixed << std::setprecision(2)
      << left_rad_sec << " " << right_rad_sec << " " << port_name;
  std::clog << "  " << cmd.str() << "\n";
  int ret = std::system(cmd.str().c_str());
  return ret == 0;
}

int main(int argc, char *argv[]) {
  std::signal(SIGINT, signal_handler);

  if (argc > 1 &&
      (std::string(argv[1]) == "-h" || std::string(argv[1]) == "--help")) {
    print_usage(argv[0]);
    return 0;
  }

  RobotConfig config;
  config.wheel_radius = (argc > 1) ? std::stod(argv[1]) : 0.0325;
  config.wheel_base = (argc > 2) ? std::stod(argv[2]) : 0.16;
  config.ticks_per_rev = (argc > 3) ? std::stod(argv[3]) : 4320.0;
  std::string port_name = (argc > 4) ? argv[4] : "/dev/ttyACM0";
  std::string calib_file = (argc > 5) ? argv[5] : "data/robot_calibration.txt";

  std::string control_bin = find_control_binary(argv[0]);
  std::clog << "Using control binary: " << control_bin << "\n";

  std::clog << "\n============================================================="
               "==========\n";
  std::clog << "               WHEEL RADIUS / ODOMETRY CALIBRATION TOOL        "
               "        \n";
  std::clog << "==============================================================="
               "========\n";
  std::clog
      << "This tool drives the robot straight for a fixed time and uses the\n";
  std::clog
      << "measured travel distance to compute the effective wheel radius.\n";
  std::clog << "Configuration:\n";
  std::clog << "  Wheel radius   : " << std::fixed << std::setprecision(4)
            << config.wheel_radius << " m\n";
  std::clog << "  Wheel base     : " << std::setprecision(4)
            << config.wheel_base << " m\n";
  std::clog << "  Ticks per rev  : " << std::setprecision(1)
            << config.ticks_per_rev << "\n";
  std::clog << "  Serial port    : " << port_name << "\n";
  std::clog << "  Output file    : " << calib_file << "\n";
  std::clog << "==============================================================="
               "========\n\n";

  std::clog
      << "STEP 1: Place the robot on a flat surface and mark its starting\n";
  std::clog << "        position as 0, 0, 0. Make sure it has room to drive "
               "forward.\n";
  std::clog << "\nPress ENTER when ready...";
  std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');

  if (!run_calibration) {
    std::clog << "\nCalibration cancelled.\n";
    return 0;
  }

  // On this robot, negative left + positive right produces forward motion.
  const float left_vel = -4.0f;
  const float right_vel = 4.0f;
  const int drive_duration_sec = 2;
  const float wheel_travel_rad =
      std::abs(left_vel) * drive_duration_sec;  // 8 rad

  std::clog << "\nSTEP 2: Driving straight for " << drive_duration_sec
            << " seconds...\n";
  std::clog << "  Left  wheel command: " << left_vel << " rad/sec\n";
  std::clog << "  Right wheel command: " << right_vel << " rad/sec\n";
  std::clog << "  Expected distance  : " << std::setprecision(4)
            << (wheel_travel_rad * config.wheel_radius) << " m\n\n";

  if (!invoke_control(control_bin, port_name, left_vel, right_vel)) {
    std::cerr << "ERROR: failed to send drive command.\n";
    return 1;
  }

  // Countdown while allowing Ctrl+C to abort early.
  for (int i = 0; i < drive_duration_sec && run_calibration; ++i) {
    std::this_thread::sleep_for(std::chrono::seconds(1));
  }

  // Always send a stop command, even if calibration was interrupted.
  std::clog << "\nStopping wheels...\n";
  if (!invoke_control(control_bin, port_name, 0.0f, 0.0f)) {
    std::cerr << "WARNING: failed to send stop command.\n";
  }

  if (!run_calibration) {
    std::clog << "\nCalibration cancelled.\n";
    return 0;
  }

  std::clog << "\nSTEP 3: Measure how far the robot actually traveled in a "
               "straight line\n";
  std::clog << "        and enter the distance in meters.\n";
  std::clog << "Actual distance (meters): ";
  double actual_distance = 0.0;
  if (!(std::cin >> actual_distance)) {
    std::cerr << "ERROR: invalid distance input. Exiting.\n";
    return 1;
  }

  if (actual_distance <= 0.0) {
    std::cerr << "ERROR: measured distance must be positive. Exiting.\n";
    return 1;
  }

  // Calibrated scale factor = actual_distance / expected_distance.
  // The state estimator applies this to the nominal wheel_radius at runtime.
  double expected_distance = wheel_travel_rad * config.wheel_radius;
  config.scale_factor = actual_distance / expected_distance;

  std::clog << "\n============================================================="
               "==========\n";
  std::clog << "Calibration result:\n";
  std::clog << "  Measured distance  : " << std::setprecision(4)
            << actual_distance << " m\n";
  std::clog << "  Expected distance  : " << std::setprecision(4)
            << expected_distance << " m\n";
  std::clog << "  Wheel rotation     : " << std::setprecision(1)
            << wheel_travel_rad << " rad\n";
  std::clog << "  Scale factor       : " << std::setprecision(6)
            << config.scale_factor << "\n";
  std::clog << "  Effective radius   : " << std::setprecision(4)
            << config.effective_wheel_radius() << " m\n";
  std::clog << "  Writing to         : " << calib_file << "\n";
  std::clog << "==============================================================="
               "========\n";

  if (!save_robot_config(calib_file, config)) {
    std::cerr << "ERROR: failed to write calibration file.\n";
    return 1;
  }

  std::clog << "Calibration saved. You can now run the state estimator with:\n";
  std::clog << "  bazel run --config=arm64 "
               "//pico_interface:run_state_estimator -- user@host\n";
  std::clog << "(ensure " << calib_file << " is present on the board)\n\n";

  return 0;
}
