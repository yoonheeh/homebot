#include <gtest/gtest.h>

#include <vector>

#include "data_collector/Collector.hpp"

// 1. Create a Mock Writer that saves data to RAM instead of Disk
class MockDatasetWriter : public IDatasetWriter {
 public:
  struct LoggedFrame {
    double timestamp;
    std::string filename;
    float v_left;
    float v_right;
    std::string prompt;
    cv::Mat image;
  };

  std::vector<LoggedFrame> frames;
  cv::Mat last_image;

  void saveImage(const std::string& filename, const cv::Mat& image) override {
    last_image = image.clone();  // Deep copy the image array
  }

  void logFrame(double timestamp, const std::string& filename, float l, float r,
                const std::string& p) override {
    frames.push_back({timestamp, filename, l, r, p, last_image});
  }
};

// 2. The Test
TEST(DataCollectorTest, SynchronizesAndCropsCorrectly) {
  MockDatasetWriter mock_writer;
  DataCollectorCore collector(mock_writer, "drive to kitchen");

  // 1. Simulate WASD control arriving over the network
  collector.onControlMessage(1.5f, -0.5f);

  // 2. Simulate 640x480 Camera frame arriving (Solid Blue)
  cv::Mat raw_img(480, 640, CV_8UC3, cv::Scalar(255, 0, 0));
  collector.onCameraMessage(raw_img, 12345.678);

  // 3. Verify exactly 1 frame was written
  ASSERT_EQ(mock_writer.frames.size(), 1);

  // 4. Verify the math and string formatting
  auto frame = mock_writer.frames[0];
  EXPECT_EQ(frame.filename, "frame_00000.jpg");
  EXPECT_FLOAT_EQ(frame.v_left, 1.5f);
  EXPECT_FLOAT_EQ(frame.v_right, -0.5f);
  EXPECT_EQ(frame.prompt, "drive to kitchen");

  // 5. Verify the image was correctly center-cropped and scaled down to 224x224
  EXPECT_EQ(frame.image.cols, 224);
  EXPECT_EQ(frame.image.rows, 224);
}
