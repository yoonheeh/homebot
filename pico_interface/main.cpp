// main.cpp
#include "pico_interface/TelemetryDefs.hpp"
#include "pico_interface/TeleopController.hpp"
#include "pico_interface/HardwareImpl.hpp"
#include <iostream>

int main(int argc, char* argv[]) {
    int fd = configure_serial("/dev/ttyACM0"); // Handle errors appropriately
    zmq::context_t context(1); 
    zmq::socket_t zmq_pub(context, zmq::socket_type::pub);
    zmq_pub.bind("tcp://*:5556");

    RawTerminalGuard terminal_guard; // Automatically sets raw mode, cleans up on exit
    KeyboardInput keyboard;
    SerialMotorDriver motors(fd);
    ZmqTelemetryPublisher telemetry(zmq_pub);

    TeleopController robot(keyboard, motors, telemetry);

    std::clog << "Control Mode Active! Use WASD to drive, Q to quit.\n";

    while (robot.isRunning()) {
        robot.step();
        usleep(20000); // 50Hz
    }

    robot.stopRobot();
    close(fd);
    return 0;
}
