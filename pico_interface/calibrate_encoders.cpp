#include <iostream>
#include <iomanip>
#include <csignal>
#include <unistd.h>
#include <atomic>
#include <chrono>
#include "pico_interface/TelemetryDefs.hpp"
#include "pico_interface/TelemetryQueue.hpp"
#include "pico_interface/PicoInterface.hpp"

// Global control flag
std::atomic<bool> run_calibration(true);

// Signal handler
void signal_handler(int signal) {
    if (signal == SIGINT) {
        run_calibration = false;
    }
}

int main() {
    std::signal(SIGINT, signal_handler);
    const char* port_name = "/dev/ttyACM0";

    TelemetryQueue<EncoderIMUTelemetry> queue;
    PicoInterface pico(port_name, queue);

    std::clog << "Connecting to Pico on " << port_name << "...\n";
    if (!pico.start()) {
        std::cerr << "Failed to connect to Pico. Exiting.\n";
        return 1;
    }

    std::clog << "\n=======================================================================\n";
    std::clog << "               WHEEL ENCODER TICK CALIBRATION TOOL                     \n";
    std::clog << "=======================================================================\n";
    std::clog << "Directions: Spin a wheel exactly one full rotation by hand,             \n";
    std::clog << "            and note the total difference in tick counts (DIFF).        \n";
    std::clog << "\nExpected targets:                                                    \n";
    std::clog << "  - 1X Decoding:  ~1,080 Ticks                                         \n";
    std::clog << "  - 4X Quadrature Decoding: ~4,320 Ticks                               \n";
    std::clog << "Press Ctrl+C to save and exit.\n";
    std::clog << "=======================================================================\n\n";

    EncoderIMUTelemetry initial_tel;
    bool got_initial = false;

    while (run_calibration) {
        EncoderIMUTelemetry tel;
        if (queue.pop(tel, std::chrono::milliseconds(100))) {
            if (!got_initial) {
                initial_tel = tel;
                got_initial = true;
                std::clog << "Baseline set! Initial counts recorded:\n";
                std::clog << "  FL: " << tel.count_fl 
                          << " | RL: " << tel.count_rl 
                          << " | FR: " << tel.count_fr 
                          << " | RR: " << tel.count_rr << "\n\n";
            }

            int32_t diff_fl = tel.count_fl - initial_tel.count_fl;
            int32_t diff_rl = tel.count_rl - initial_tel.count_rl;
            int32_t diff_fr = tel.count_fr - initial_tel.count_fr;
            int32_t diff_rr = tel.count_rr - initial_tel.count_rr;

            // Carriage return printing continuously on a single line
            std::clog << "\r[RAW] FL: " << std::setw(6) << tel.count_fl 
                      << " | FR: " << std::setw(6) << tel.count_fr 
                      << " || [DIFF FROM START] FL: " << std::setw(6) << diff_fl
                      << " | FR: " << std::setw(6) << diff_fr << std::flush;
        }
    }

    pico.stop();
    std::clog << "\n\nCalibration helper closed safely.\n";
    return 0;
}
