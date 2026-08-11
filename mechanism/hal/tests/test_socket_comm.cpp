#include <arpa/inet.h>
#include <gtest/gtest.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <string>

#include "mechanism/hal/hardware_impl.h"
#include "mechanism/hal/hardware_interfaces.h"

class MotorDriverTest : public ::testing::Test {
 protected:
  int server_fd = -1;
  int test_port = 0;

  void SetUp() override {
    // Create a UDP socket to act as the Isaac Sim receiver
    server_fd = socket(AF_INET, SOCK_DGRAM, 0);
    ASSERT_GE(server_fd, 0) << "Failed to create test server socket";

    // Bind to an ephemeral port (Port 0) so the OS assigns a free one
    struct sockaddr_in server_addr {};
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = inet_addr("127.0.0.1");
    server_addr.sin_port = 0;

    ASSERT_EQ(
        bind(server_fd, (struct sockaddr*)&server_addr, sizeof(server_addr)), 0)
        << "Failed to bind test server socket";

    // Find out which port the OS actually assigned
    socklen_t len = sizeof(server_addr);
    ASSERT_EQ(getsockname(server_fd, (struct sockaddr*)&server_addr, &len), 0);
    test_port = ntohs(server_addr.sin_port);

    // Set a 1-second timeout so the test doesn't hang if it fails
    struct timeval tv;
    tv.tv_sec = 1;
    tv.tv_usec = 0;
    setsockopt(server_fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
  }

  void TearDown() override {
    if (server_fd >= 0) {
      close(server_fd);
    }
  }
};

TEST_F(MotorDriverTest, UDPCommunication) {
  // Arrange: Initialize driver with the dynamically assigned port
  IsaacSimMotorDriver driver("127.0.0.1", test_port);
  float left_cmd = 1.5f;
  float right_cmd = -2.75f;

  // The exact string we expect the driver to generate
  std::string expected_payload =
      std::to_string(left_cmd) + "," + std::to_string(right_cmd);

  // Act: Send the command
  driver.sendVelocityCommand(left_cmd, right_cmd);

  // Assert: Receive the UDP packet on our test server
  char buffer[256];
  struct sockaddr_in client_addr {};
  socklen_t client_len = sizeof(client_addr);

  ssize_t bytes_read = recvfrom(server_fd, buffer, sizeof(buffer) - 1, 0,
                                (struct sockaddr*)&client_addr, &client_len);

  // Ensure we actually received data (will be -1 if it timed out)
  ASSERT_GT(bytes_read, 0) << "Failed to receive UDP packet (timed out?)";

  // Null-terminate and check the payload
  buffer[bytes_read] = '\0';
  std::string actual_payload(buffer);

  EXPECT_EQ(actual_payload, expected_payload);
}
