cmake_minimum_required(VERSION 3.10)
project(pico_interface CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find threads package
find_package(Threads REQUIRED)

# Include directories
include_directories(include)

# Main executable (Passive Estimator)
add_executable(pico_interface main.cpp)
target_link_libraries(pico_interface PRIVATE Threads::Threads)

# Test executable
add_executable(test_suite test_suite.cpp)
target_link_libraries(test_suite PRIVATE Threads::Threads)

# Control utility executable
add_executable(control control.cpp)
target_link_libraries(control PRIVATE Threads::Threads)

# Encoder Calibration utility
add_executable(calibrate_encoders calibrate_encoders.cpp)
target_link_libraries(calibrate_encoders PRIVATE Threads::Threads)
