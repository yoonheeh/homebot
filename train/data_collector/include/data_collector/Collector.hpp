#pragma once
#include <algorithm>
#include <iomanip>
#include <mutex>
#include <opencv2/opencv.hpp>
#include <sstream>
#include <string>

// Interface for writing data (can be Disk or Mock)
class IDatasetWriter {
 public:
  virtual ~IDatasetWriter() = default;
  virtual void saveImage(const std::string& filename, const cv::Mat& image) = 0;
  virtual void logFrame(double timestamp, const std::string& filename,
                        float v_left, float v_right,
                        const std::string& prompt) = 0;
};

// Thread-Safe State
struct RobotState {
  float v_left = 0.0f;
  float v_right = 0.0f;
  std::mutex mtx;

  void update(float l, float r) {
    std::lock_guard<std::mutex> lock(mtx);
    v_left = l;
    v_right = r;
  }

  void get(float& l, float& r) {
    std::lock_guard<std::mutex> lock(mtx);
    l = v_left;
    r = v_right;
  }
};

// Highly Testable Core Logic
class DataCollectorCore {
 public:
  DataCollectorCore(IDatasetWriter& writer, const std::string& prompt)
      : writer_(writer), prompt_(prompt), frame_count_(0) {}

  // Called when a ZMQ control packet arrives
  void onControlMessage(float v_left, float v_right) {
    state_.update(v_left, v_right);
  }

  // Called when a ZMQ camera frame arrives
  void onCameraMessage(const cv::Mat& raw_image, double timestamp) {
    // 1. Grab the latest state
    float v_left, v_right;
    state_.get(v_left, v_right);

    // 2. Process image
    cv::Mat processed = processImage(raw_image);

    // 3. Generate filename
    std::stringstream filename;
    filename << "frame_" << std::setfill('0') << std::setw(5) << frame_count_
             << ".jpg";

    // 4. Send to writer interface
    writer_.saveImage(filename.str(), processed);
    writer_.logFrame(timestamp, filename.str(), v_left, v_right, prompt_);

    frame_count_++;
  }

 private:
  cv::Mat processImage(const cv::Mat& src) {
    if (src.empty()) return src;

    // Center crop
    int min_dim = std::min(src.cols, src.rows);
    cv::Rect roi((src.cols - min_dim) / 2, (src.rows - min_dim) / 2, min_dim,
                 min_dim);
    cv::Mat cropped = src(roi);

    // Resize
    cv::Mat resized;
    cv::resize(cropped, resized, cv::Size(224, 224));
    return resized;
  }

  RobotState state_;
  IDatasetWriter& writer_;
  std::string prompt_;
  int frame_count_;
};