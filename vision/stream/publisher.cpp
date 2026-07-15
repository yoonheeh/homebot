#include <atomic>
#include <csignal>
#include <depthai/depthai.hpp>
#include <iostream>
#include <memory>
#include <opencv2/opencv.hpp>
#include <vector>
#include <zmq.hpp>

std::atomic<bool> quitEvent(false);

void signalHandler(int) { quitEvent = true; }

int main() {
  // Initialize ZeroMQ Context and Publisher Socket
  zmq::context_t context(1);
  zmq::socket_t publisher(context, ZMQ_PUB);
  constexpr const char* stream_addr = "ipc:///tmp/oakd_rgb_stream.ipc";
  publisher.bind(stream_addr);
  std::cout << "ZeroMQ Publisher bound to " << stream_addr << std::endl;

  std::vector<uchar> buffer;
  std::vector<int> params = {cv::IMWRITE_JPEG_QUALITY,
                             80};  // 80% compression quality

  signal(SIGTERM, signalHandler);
  signal(SIGINT, signalHandler);

  // Create device
  std::shared_ptr<dai::Device> device = std::make_shared<dai::Device>();
  std::cout << "OAK-D Lite pipeline started successfully." << std::endl;

  // Create pipeline
  dai::Pipeline pipeline(device);

  // Create nodes
  // auto cam = pipeline.create<dai::node::Camera>()->build();
  // Create nodes to disable auto-focusing
  auto cam = pipeline.create<dai::node::Camera>()->build(
      dai::CameraBoardSocket::CAM_A);
  auto camQIn = cam->inputControl.createInputQueue();
  auto videoQueue =
      cam->requestOutput(std::make_pair(640, 400))->createOutputQueue();

  // Start pipeline
  pipeline.start();

  // Disable auto focus, auto exposure and white balancing
  uint16_t exposure_time_us = 20000;  // to reduce motion blur
  uint32_t sensitivity_iso = 400;
  int color_temperature_k = 4500;
  auto ctrl = std::make_shared<dai::CameraControl>();
  ctrl->setManualExposure(exposure_time_us, sensitivity_iso);
  ctrl->setManualWhiteBalance(color_temperature_k);
  ctrl->setManualFocus(80);  // manually tuned value
  camQIn->send(ctrl);

  // Read from the image queue and discard them for ~0.5 seconds
  // to ensure the camera hardware has fully latched onto these settings.
  for (int i = 0; i < 15; ++i) {  // 15 frames @ 30FPS = ~0.5 seconds
    videoQueue->get<dai::ImgFrame>();
  }

  std::cout << "Camera ready for recording..." << std::endl;

  while (pipeline.isRunning() && !quitEvent) {
    auto videoIn = videoQueue->get<dai::ImgFrame>();
    if (videoIn == nullptr) continue;
    // Wrap the raw YUV420p byte array into a 1-channel OpenCV Mat
    // Height is multiplied by 1.5 to account for the U and V color planes
    cv::Mat yuv(videoIn->getHeight() * 3 / 2, videoIn->getWidth(), CV_8UC1,
                videoIn->getData().data());

    cv::Mat frame;
    // Convert YUV420p (I420) to standard BGR
    cv::cvtColor(yuv, frame, cv::COLOR_YUV2BGR_I420);

    // Compress to JPEG to save network bandwidth
    cv::imencode(".jpg", frame, buffer, params);

    // Create and send ZeroMQ message
    zmq::message_t message(buffer.size());
    memcpy(message.data(), buffer.data(), buffer.size());

    // Send the frame data (non-blocking if queue fills up)
    publisher.send(message, zmq::send_flags::none);
  }

  pipeline.stop();
  pipeline.wait();
}
