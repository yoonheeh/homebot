cmake_minimum_required(VERSION 3.13)

# 1. Automatically fetch the Pico SDK using FetchContent
include(FetchContent)
FetchContent_Declare(
    pico_sdk
    GIT_REPOSITORY https://github.com/raspberrypi/pico-sdk.git
    GIT_TAG        2.0.0
)

FetchContent_GetProperties(pico_sdk)
if (NOT pico_sdk_POPULATED)
    message(STATUS "Downloading/Extracting Pico SDK 2.0.0 inside the repository build tree...")
    FetchContent_Populate(pico_sdk)
    set(PICO_SDK_PATH ${pico_sdk_SOURCE_DIR} CACHE PATH "Path to the Pico SDK" FORCE)
endif()

# 2. Automatically default to the Pico ARM cross-compiler toolchain if none is specified
if (NOT DEFINED CMAKE_TOOLCHAIN_FILE)
    if (EXISTS "${PICO_SDK_PATH}/cmake/preload/toolchains/pico_arm_cortex_m0plus_gcc.cmake")
        set(CMAKE_TOOLCHAIN_FILE "${PICO_SDK_PATH}/cmake/preload/toolchains/pico_arm_cortex_m0plus_gcc.cmake" CACHE STRING "" FORCE)
    endif()
endif()

# 3. Include the import script directly from the downloaded SDK folder
include(${PICO_SDK_PATH}/external/pico_sdk_import.cmake)

project(pico_firmware C CXX ASM)

# 4. Initialize the Pico SDK
pico_sdk_init()

# 5. Define the executable and point it to your main.cpp file
add_executable(pico_firmware 
    main.cpp
)

# 6. Generate the PIO header 
pico_generate_pio_header(pico_firmware ${CMAKE_CURRENT_LIST_DIR}/quadrature_encoder.pio)

# 7. Link the required hardware libraries
target_link_libraries(pico_firmware PRIVATE
    pico_stdlib       # Core utilities, UART, GPIO
    hardware_pio      # Programmable I/O for the encoders
    hardware_pwm      # PWM for the motor shield
    hardware_timer    # High-precision time_us_64() for the control loop
    hardware_i2c      # Hardware I2C for the MPU-6050 IMU
)

# 8. Enable/Disable standard output streams
pico_enable_stdio_usb(pico_firmware 1)
pico_enable_stdio_uart(pico_firmware 0)

# 9. Create the .uf2 file for drag-and-drop flashing
pico_add_extra_outputs(pico_firmware)
