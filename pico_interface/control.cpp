#include <iostream>
#include <string>
#include <cstdlib>
#include <iomanip>
#include <unistd.h>
#include "pico_interface/TelemetryDefs.hpp"

void print_usage(const char* prog_name) {
    std::clog << "Usage: " << prog_name << " <left_rad_sec> <right_rad_sec> [port_name]\n";
    std::clog << "Example: " << prog_name << " -2.5 2.5 /dev/ttyACM0\n";
    std::clog << "Default port_name is /dev/ttyACM0\n";
}

int main(int argc, char* argv[]) {
    if (argc < 3 || argc > 4) {
        print_usage(argv[0]);
        return 1;
    }

    // Parse velocities
    float left_vel = 0.0f;
    float right_vel = 0.0f;
    try {
        left_vel = std::stof(argv[1]);
        right_vel = std::stof(argv[2]);
    } catch (const std::exception& e) {
        std::cerr << "Error parsing velocities: " << e.what() << "\n";
        print_usage(argv[0]);
        return 1;
    }

    // Parse port
    std::string port_name = "/dev/ttyACM0";
    if (argc == 4) {
        port_name = argv[3];
    }

    std::clog << "Opening serial port " << port_name << "...\n";
    int fd = configure_serial(port_name.c_str());
    if (fd < 0) {
        std::cerr << "Failed to open serial port. Exiting.\n";
        return 1;
    }

    // Build the VelocityTarget message
    VelocityTarget msg;
    msg.left_rad_sec = left_vel;
    msg.right_rad_sec = right_vel;
    msg.crc16 = calculate_crc16(reinterpret_cast<const uint8_t*>(&msg), sizeof(float) * 2);

    // Buffers for encoding
    uint8_t struct_buffer[sizeof(VelocityTarget)];
    uint8_t encoded_buffer[sizeof(VelocityTarget) + 2]; // COBS overhead + delimiter

    std::memcpy(struct_buffer, &msg, sizeof(VelocityTarget));
    size_t encoded_len = cobs_encode(struct_buffer, sizeof(VelocityTarget), encoded_buffer);

    std::clog << std::fixed << std::setprecision(2);
    std::clog << "Sending command:\n";
    std::clog << "  - Left Target  : " << left_vel << " rad/sec\n";
    std::clog << "  - Right Target : " << right_vel << " rad/sec\n";
    std::clog << "  - Packet CRC16 : 0x" << std::hex << std::setw(4) << std::setfill('0') << msg.crc16 << std::dec << "\n";

    // Write to port
    ssize_t bytes_written = write(fd, encoded_buffer, encoded_len);
    if (bytes_written == static_cast<ssize_t>(encoded_len)) {
        std::clog << "Successfully sent command (" << bytes_written << " bytes written).\n";
    } else {
        std::cerr << "Error: Only wrote " << bytes_written << " of " << encoded_len << " bytes.\n";
    }

    // Sleep briefly to ensure transmission completes before closing the port descriptor
    usleep(50000); // 50ms
    close(fd);

    return 0;
}
