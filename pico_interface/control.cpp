#include <iostream>
#include <string>
#include <cstdlib>
#include <iomanip>
#include <unistd.h>
#include <cstring>
#include <termios.h>
#include <fcntl.h>
#include <zmq.hpp>
#include "pico_interface/TelemetryDefs.hpp"

// Base speed variables (adjust these to fit your robot's physical capabilities)
const float LINEAR_SPEED = 9.0f;  // rad/sec for straight lines
const float TURN_SPEED = 8.5f;   // rad/sec for turns

// Non-blocking keyboard read configuration
void set_terminal_raw_mode(bool enable) {
    static struct termios oldt, newt;
    if (enable) {
        tcgetattr(STDIN_FILENO, &oldt);
        newt = oldt;
        newt.c_lflag &= ~(ICANON | ECHO); // Disable buffering and echoing
        newt.c_cc[VMIN] = 0;              // Non-blocking read
        newt.c_cc[VTIME] = 0;             // No timeout delay
        tcsetattr(STDIN_FILENO, TCSANOW, &newt);
    } else {
        tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
    }
}

// Helper function to handle serial packet building and sending
void send_velocity_command(int fd, float left_vel, float right_vel) {
    VelocityTarget msg;
    msg.left_rad_sec = left_vel;
    msg.right_rad_sec = right_vel;
    msg.crc16 = calculate_crc16(reinterpret_cast<const uint8_t*>(&msg), sizeof(float) * 2);

    uint8_t struct_buffer[sizeof(VelocityTarget)];
    uint8_t encoded_buffer[sizeof(VelocityTarget) + 2];

    std::memcpy(struct_buffer, &msg, sizeof(VelocityTarget));
    size_t encoded_len = cobs_encode(struct_buffer, sizeof(VelocityTarget), encoded_buffer);

    write(fd, encoded_buffer, encoded_len);
}

int main(int argc, char* argv[]) {
    std::string port_name = "/dev/ttyACM0";
    if (argc == 2) {
        port_name = argv[1];
    } else if (argc > 2) {
        std::clog << "Usage: " << argv[0] << " [port_name]\n";
        return 1;
    }

    std::clog << "Opening serial port " << port_name << "...\n";
    int fd = configure_serial(port_name.c_str());
    if (fd < 0) {
        std::cerr << "Failed to open serial port. Exiting.\n";
        return 1;
    }
    
    // ZMQ setup
    zmq::context_t context(1); 
    zmq::socket_t publisher(context, zmq::socket_type::pub);

    // Put terminal into raw, non-blocking mode
    set_terminal_raw_mode(true);

    std::clog << "\n====================================================\n";
    std::clog << " Control Mode Active! Use WASD to drive, Q to quit.\n";
    std::clog << "====================================================\n\n";

    char ch;
    bool running = true;

    // Track active key states manually to handle overlapping inputs
    bool w_pressed = false;
    bool s_pressed = false;
    bool a_pressed = false;
    bool d_pressed = false;

    while (running) {
        // Read character stream to update current key states
        while (read(STDIN_FILENO, &ch, 1) > 0) {
            if (ch == 'q' || ch == 'Q') {
                running = false;
            }
            
            // Map character strokes to active vector triggers
            if (ch == 'w' || ch == 'W') { w_pressed = true; s_pressed = false; }
            if (ch == 's' || ch == 'S') { s_pressed = true; w_pressed = false; }
            if (ch == 'a' || ch == 'A') { a_pressed = true; d_pressed = false; }
            if (ch == 'd' || ch == 'D') { d_pressed = true; a_pressed = false; }
            if (ch == ' ') { // Spacebar emergency brake
                w_pressed = s_pressed = a_pressed = d_pressed = false;
            }
        }

        // Initialize frame velocities
        float left_target = 0.0f;
        float right_target = 0.0f;
    
        // 1. Handle Linear Movement
        if (w_pressed) {
            left_target = LINEAR_SPEED;
            right_target = LINEAR_SPEED;
        } else if (s_pressed) {
            left_target = -LINEAR_SPEED;
            right_target = -LINEAR_SPEED;
        }

        // 2. Handle Angular Turns & Pure Rotation
        if (d_pressed) {
            // If driving forward, create a sweeping turn. If stationary, perform a zero-radius spin.
            left_target  += TURN_SPEED;
            right_target -= TURN_SPEED;
        } else if (a_pressed) {
            left_target  -= TURN_SPEED;
            right_target += TURN_SPEED;
        }
    
        // Incorporate motor rotating direction
        left_target *= -1.0f;

        // Send the mixed command down the serial pipeline
        send_velocity_command(fd, left_target, right_target);

        // Reset tracking variables for next iteration cycle
        // If the key is held down, the next cycle loop will re-trigger them true
        w_pressed = s_pressed = a_pressed = d_pressed = false;

        // Control frequency: loop updates roughly every 20ms (50Hz)
        usleep(20000); 
    }

    // Clean up: stop the robot, restore terminal settings, close port descriptor
    std::clog << "\nShutting down interface. Stopping robot...\n";
    send_velocity_command(fd, 0.0f, 0.0f);
    usleep(50000);

    set_terminal_raw_mode(false);
    close(fd);
    return 0;
}

