#pragma once

#include "base_streamer.hpp"

class RgbStreamer : public BaseStreamer {
 private:
  int stream_fps_;
  uint16_t width_, height_;

 public:
  RgbStreamer(zmq::context_t& context, const std::string& endpoint,
              int stream_fps, uint16_t width, uint16_t height)
      : BaseStreamer(context, endpoint),
        stream_fps_(stream_fps),
        width_(width),
        height_(height) {}

  void setupPipeline(dai::Pipeline& pipeline) override {
    auto cam = pipeline.create<dai::node::Camera>()->build();
    auto videoQueue = cam->requestOutput(std::make_pair(width_, height_),
                                         std::nullopt, dai::ImgResizeMode::CROP,
                                         (float)stream_fps_, std::nullopt)
                          ->createOutputQueue();

    // TODO: BGR conversion

    auto controlQueue = cam->inputControl.createInputQueue();

    // Capture the queues into the lambda and use Pipeline pointer
    loop_function_ = [this, videoQueue,
                      controlQueue](dai::Pipeline* current_pipeline) {
      auto ctrl = std::make_shared<dai::CameraControl>();
      ctrl->setManualExposure(20000, 400);
      ctrl->setManualWhiteBalance(4500);
      ctrl->setManualFocus(80);
      controlQueue->send(ctrl);

      std::cout << "RGB Camera ready at " << stream_fps_ << " FPS..."
                << std::endl;
      while (current_pipeline->isRunning() && !quitEvent) {
        auto videoIn = videoQueue->get<dai::ImgFrame>();
        if (videoIn == nullptr) {
          continue;
        }

        // Pull the raw BGR byte vector directly from the DepthAI ISP
        auto raw_data = videoIn->getData();

        // Dump it straight into ZeroMQ
        zmq::message_t message(raw_data.size());
        std::memcpy(message.data(), raw_data.data(), raw_data.size());
        publisher_.send(message, zmq::send_flags::none);
      }
    };
  }
};
