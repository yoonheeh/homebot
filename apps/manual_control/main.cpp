// main.cpp
#include <iostream>

#include "infrastructure/teleop/teleop_controller.h"
#include "mechanism/common/telemetry.h"
#include "mechanism/hal/hardware_impl.h"  // includes RawTerminalGuard, KeyboardINput, SerialMotorDriver

enum class ControlMode { kReal, kIsaacSim };

void printUsage(const char *prog_name) {
  std::cerr << "Usage: " << prog_name
            << " [--mode real|sim] [--port /dev/ttyACM0] [--sim-ip 127.0.0.1] "
               "[--sim-port 5005]\n";
  std::cerr << "  --mode      'real' for physical hardware, 'sim' for Isaac "
               "Sim UDP bridge (default: real)\n";
  std::cerr << "  --port      Serial device path for real mode (default: "
               "/dev/ttyACM0)\n";
  std::cerr
      << "  --sim-ip    IP address of Isaac Sim host (default: 127.0.0.1)\n";
  std::cerr << "  --sim-port  UDP port for Isaac Sim bridge (default: 5005)\n";
}

int main(int argc, char *argv[]) {
  ControlMode mode = ControlMode::kReal;
  std::string serial_port = "/dev/ttyACM0";
  std::string sim_ip = "127.0.0.1";
  int sim_port = 5005;

  // Command-Line Argument Parsing
  for (int i = 1; i < argc; ++i) {
    std::string arg = argv[i];
    if (arg == "--mode" && i + 1 < argc) {
      std::string mode_str = argv[++i];
      if (mode_str == "sim" || mode_str == "isaac") {
        mode = ControlMode::kIsaacSim;
      } else if (mode_str == "real") {
        mode = ControlMode::kReal;
      } else {
        std::cerr << "Error: Invalid mode '" << mode_str
                  << "'. Use 'real' or 'sim'.\n";
        printUsage(argv[0]);
        return 1;
      }
    } else if (arg == "--port" && i + 1 < argc) {
      serial_port = argv[++i];
    } else if (arg == "--sim-ip" && i + 1 < argc) {
      sim_ip = argv[++i];
    } else if (arg == "--sim-port" && i + 1 < argc) {
      sim_port = std::stoi(argv[++i]);
    } else if (arg == "-h" || arg == "--help") {
      printUsage(argv[0]);
      return 0;
    }
  }

  int serial_fd = -1;
  std::unique_ptr<IMotorDriver> motors;

  // Driver Selection Logic
  if (mode == ControlMode::kReal) {
    serial_fd = configure_serial(serial_port.c_str());
    if (serial_fd < 0) {
      std::cerr << "Error: Failed to open serial port " << serial_port << "\n";
      return 1;
    }
    motors = std::make_unique<SerialMotorDriver>(serial_fd);
    std::clog << "[Mode: REAL HARDWARE] Using serial connection on "
              << serial_port << "\n";
  } else {
    motors = std::make_unique<IsaacSimMotorDriver>(sim_ip, sim_port);
    std::clog << "[Mode: ISAAC SIM] Target UDP socket set to " << sim_ip << ":"
              << sim_port << "\n";
  }

  zmq::context_t context(1);
  zmq::socket_t zmq_pub(context, zmq::socket_type::pub);
  zmq_pub.bind("ipc:///tmp/control_command.ipc");

  RawTerminalGuard terminal_guard;  // Sets raw terminal mode, cleans up on exit
  KeyboardInput keyboard;
  ZmqTelemetryPublisher telemetry(zmq_pub);

  // Bind TeleopController to selected motor driver implementation
  TeleopController robot(keyboard, *motors, telemetry);

  std::clog << "Control Loop Running ("
            << (mode == ControlMode::kReal ? "REAL" : "ISAAC SIM")
            << ")! Use WASD to drive, Q to quit.\n";

  while (robot.isRunning()) {
    robot.step();
    usleep(20000);  // 50Hz
  }

  robot.stopRobot();

  if (serial_fd >= 0) {
    close(serial_fd);
  }

  return 0;
}
