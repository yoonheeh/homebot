#pragma once
#include <libgen.h>
#include <sys/stat.h>
#include <sys/types.h>

#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#include "StateEstimator.hpp"

// Simple persistent storage for RobotConfig calibration values.
//
// Format (one key=value per line):
//   wheel_radius=0.0325
//   wheel_base=0.1300
//   ticks_per_rev=4320.0
//
// Unrecognized keys are ignored. Missing keys leave the corresponding
// RobotConfig field unchanged, so callers can set sensible defaults before
// loading.

inline bool save_robot_config(const std::string &path,
                              const RobotConfig &config) {
  // Create parent directory if needed.
  size_t last_slash = path.find_last_of("/\\");
  if (last_slash != std::string::npos) {
    std::string dir = path.substr(0, last_slash);
    if (!dir.empty()) {
      mkdir(dir.c_str(), 0777);
    }
  }

  std::ofstream file(path);
  if (!file.is_open()) {
    std::cerr << "ERROR: cannot open calibration file for writing: " << path
              << "\n";
    return false;
  }

  file << std::fixed << std::setprecision(6);
  file << "wheel_radius=" << config.wheel_radius << "\n";
  file << "wheel_base=" << config.wheel_base << "\n";
  file << "ticks_per_rev=" << config.ticks_per_rev << "\n";
  file << "scale_factor=" << config.scale_factor << "\n";
  return file.good();
}

inline bool load_robot_config(const std::string &path, RobotConfig &config) {
  std::ifstream file(path);
  if (!file.is_open()) {
    std::cerr << "ERROR: cannot open calibration file for reading: " << path
              << "\n";
    return false;
  }

  std::string line;
  while (std::getline(file, line)) {
    // Skip empty lines and comments.
    if (line.empty() || line[0] == '#') continue;

    auto eq = line.find('=');
    if (eq == std::string::npos) continue;

    std::string key = line.substr(0, eq);
    std::string value = line.substr(eq + 1);

    // Trim whitespace around key and value.
    auto trim = [](std::string &s) {
      size_t start = s.find_first_not_of(" \t\r\n");
      if (start == std::string::npos) {
        s.clear();
        return;
      }
      size_t end = s.find_last_not_of(" \t\r\n");
      s = s.substr(start, end - start + 1);
    };
    trim(key);
    trim(value);

    try {
      if (key == "wheel_radius") {
        config.wheel_radius = std::stod(value);
      } else if (key == "wheel_base") {
        config.wheel_base = std::stod(value);
      } else if (key == "ticks_per_rev") {
        config.ticks_per_rev = std::stod(value);
      } else if (key == "scale_factor") {
        config.scale_factor = std::stod(value);
      }
    } catch (const std::exception &e) {
      std::cerr << "WARNING: failed to parse '" << key
                << "' in calibration file: " << e.what() << "\n";
    }
  }
  return true;
}
