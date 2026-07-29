#pragma once

#include "base_streamer.h"
#include "mechanism/common/camera_config.h"

class DepthStreamer : public BaseStreamer {
 private:
  int stream_fps_;
  uint16_t width_, height_;
  CameraIntrinsics intrinsics_;

 public:
  DepthStreamer(zmq::context_t& context, const std::string& endpoint,
                int stream_fps, uint16_t width, uint16_t height,
                const CameraIntrinsics& cam_intrinsics)
      : BaseStreamer(context, endpoint),
        stream_fps_(stream_fps),
        width_(width),
        height_(height),
        intrinsics_(cam_intrinsics) {}

  void setupPipeline(dai::Pipeline& pipeline) override {
    auto monoLeft = pipeline.create<dai::node::Camera>()->build(
        dai::CameraBoardSocket::CAM_B);
    auto monoRight = pipeline.create<dai::node::Camera>()->build(
        dai::CameraBoardSocket::CAM_C);

    auto stereo = pipeline.create<dai::node::StereoDepth>();

    // TODO: set custom width and height??
    // auto monoLeftOut = monoLeft->requestFullResolutionOutput(std::nullopt,
    // (float)stream_fps_, false); auto monoRightOut =
    // monoRight->requestFullResolutionOutput(std::nullopt, (float)stream_fps_,
    // false);

    auto monoLeftOut = monoLeft->requestOutput(
        std::make_pair(width_, height_), std::nullopt, dai::ImgResizeMode::CROP,
        (float)stream_fps_, std::nullopt);
    auto monoRightOut = monoRight->requestOutput(
        std::make_pair(width_, height_), std::nullopt, dai::ImgResizeMode::CROP,
        (float)stream_fps_, std::nullopt);
    monoLeftOut->link(stereo->left);
    monoRightOut->link(stereo->right);

    stereo->setRectification(true);
    stereo->setExtendedDisparity(true);
    stereo->setLeftRightCheck(true);

    auto disparityQueue = stereo->disparity.createOutputQueue();

    loop_function_ = [this, disparityQueue](dai::Pipeline* current_pipeline) {
      std::cout << "Depth Camera ready at " << stream_fps_ << " FPS..."
                << std::endl;

      while (current_pipeline->isRunning() && !quitEvent) {
        auto disparity = disparityQueue->get<dai::ImgFrame>();
        if (disparity == nullptr) continue;

        // Pull the raw byte vector directly from the DepthAI hardware buffer
        auto raw_data = disparity->getData();

        // Dump it straight into ZeroMQ (No OpenCV involved!)
        zmq::message_t message(raw_data.size());
        std::memcpy(message.data(), raw_data.data(), raw_data.size());
        publisher_.send(message, zmq::send_flags::none);
      }
    };
  }
};
