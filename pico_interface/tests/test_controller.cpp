// test_controller.cpp
#include <gtest/gtest.h>
#include "pico_interface/TeleopController.hpp"

// Simple Mocks
class MockInput : public IInputProvider {
public:
    char next_char = '\0';
    bool readChar(char& ch) override {
        if (next_char != '\0') {
            ch = next_char;
            next_char = '\0';
            return true;
        }
        return false;
    }
};

class MockMotors : public IMotorDriver {
public:
    float last_left = 0.0f;
    float last_right = 0.0f;
    void sendVelocityCommand(float left, float right) override {
        last_left = left;
        last_right = right;
    }
};

class MockTelemetry : public ITelemetryPublisher {
    void publishAction(float left, float right) override {}
};

// The Test
TEST(TeleopControllerTest, ForwardKeySendsPositiveVelocity) {
    MockInput input;
    MockMotors motors;
    MockTelemetry telemetry;
    TeleopController controller(input, motors, telemetry);

    // Simulate pressing 'W'
    input.next_char = 'w';
    
    // Run one loop iteration
    controller.step();

    // Verify the math worked (accounting for the motor inversion logic)
    EXPECT_FLOAT_EQ(motors.last_left, -9.0f);
    EXPECT_FLOAT_EQ(motors.last_right, 9.0f);
}
