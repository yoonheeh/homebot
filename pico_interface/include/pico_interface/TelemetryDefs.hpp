#pragma once
#include <iostream>
#include <cstring>
#include <fcntl.h>
#include <termios.h>
#include <unistd.h>
#include <cstdint>

// 1. The Shared Zero-Copy Struct (10 Bytes)
struct __attribute__((packed)) VelocityTarget {
    float left_rad_sec;
    float right_rad_sec;
    uint16_t crc16;
};

// 2. Telemetry Packet Struct from Pico (46 Bytes)
struct __attribute__((packed)) EncoderIMUTelemetry {
    uint32_t timestamp_us; // Hardware microsecond timestamp since boot
    int32_t count_fl;
    int32_t count_rl;
    int32_t count_fr;
    int32_t count_rr;
    float accel_x;
    float accel_y;
    float accel_z;
    float gyro_x;
    float gyro_y;
    float gyro_z;
    uint16_t crc16;
};

// Robot Pose representation
struct RobotPose {
    double x = 0.0;
    double y = 0.0;
    double theta = 0.0;
    uint64_t estimated_at_us = 0; // Host steady_clock timestamp in microseconds
};

// Standard CRC16-CCITT helper
inline uint16_t calculate_crc16(const uint8_t *data, size_t length) {
    uint16_t crc = 0xFFFF;
    for (size_t i = 0; i < length; ++i) {
        crc ^= data[i] << 8;
        for (uint8_t j = 0; j < 8; ++j) {
            if (crc & 0x8000) crc = (crc << 1) ^ 0x1021;
            else crc <<= 1;
        }
    }
    return crc;
}

// COBS Encoding Algorithm
inline size_t cobs_encode(const uint8_t *ptr, size_t length, uint8_t *dst) {
    size_t read_index = 0, write_index = 1, code_index = 0;
    uint8_t code = 1;

    while (read_index < length) {
        if (ptr[read_index] == 0) {
            dst[code_index] = code;
            code = 1;
            code_index = write_index++;
            read_index++;
        } else {
            dst[write_index++] = ptr[read_index++];
            code++;
            if (code == 0xFF) {
                dst[code_index] = code;
                code = 1;
                code_index = write_index++;
            }
        }
    }
    dst[code_index] = code;
    dst[write_index++] = 0x00; // Frame delimiter
    return write_index;
}

// COBS Decoding Algorithm
// dst_capacity is the size of the caller-provided dst buffer; decoding aborts
// with 0 if the frame would overflow it.
inline size_t cobs_decode(const uint8_t *ptr, size_t length, uint8_t *dst, size_t dst_capacity) {
    size_t read_index = 0, write_index = 0, code = 0, i = 0;
    while (read_index < length) {
        code = ptr[read_index];
        if (read_index + code > length && code != 1) return 0; // Malformed
        read_index++;
        for (i = 1; i < code; i++) {
            if (write_index >= dst_capacity) return 0;
            dst[write_index++] = ptr[read_index++];
        }
        if (code != 0xFF && read_index != length) {
            if (write_index >= dst_capacity) return 0;
            dst[write_index++] = 0;
        }
    }
    return write_index;
}

// Linux Serial Port Setup Helper
inline int configure_serial(const char* port_name) {
    int fd = open(port_name, O_RDWR | O_NOCTTY | O_SYNC | O_CLOEXEC);
    if (fd < 0) {
        std::cerr << "Error opening " << port_name << "\n";
        return -1;
    }

    struct termios tty;
    if (tcgetattr(fd, &tty) != 0) {
        std::cerr << "Error from tcgetattr\n";
        close(fd);
        return -1;
    }

    cfsetospeed(&tty, B115200);
    cfsetispeed(&tty, B115200);

    tty.c_cflag |= (CLOCAL | CREAD);    // Ignore modem controls, enable reading
    tty.c_cflag &= ~CSIZE;
    tty.c_cflag |= CS8;                 // 8-bit characters
    tty.c_cflag &= ~PARENB;             // No parity bit
    tty.c_cflag &= ~CSTOPB;             // Only need 1 stop bit
    tty.c_cflag &= ~CRTSCTS;            // No hardware flow control

    // Raw mode
    tty.c_lflag &= ~(ICANON | ECHO | ECHOE | ISIG);
    tty.c_iflag &= ~(IXON | IXOFF | IXANY); // Turn off s/w flow ctrl
    tty.c_iflag &= ~(IGNBRK | BRKINT | PARMRK | ISTRIP | INLCR | IGNCR | ICRNL);
    tty.c_oflag &= ~OPOST; // Prevent special interpretation of output bytes

    tty.c_cc[VMIN]  = 0;
    tty.c_cc[VTIME] = 1; // Short timeout (100ms)

    if (tcsetattr(fd, TCSANOW, &tty) != 0) {
        std::cerr << "Error from tcsetattr\n";
        close(fd);
        return -1;
    }
    return fd;
}
