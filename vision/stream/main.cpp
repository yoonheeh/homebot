#include <atomic>
#include <csignal>
#include <depthai/depthai.hpp>
#include <iostream>
#include <memory>
#include <zmq.hpp>

#include "depth_streamer.hpp"
#include "rgb_streamer.hpp"

std::atomic<bool> quitEvent(false);

void signalHandler(int) { quitEvent = true; }

int main() {
  signal(SIGTERM, signalHandler);
  signal(SIGINT, signalHandler);

  // Initialize unified resources
  zmq::context_t zmq_context(1);
  std::shared_ptr<dai::Device> device = std::make_shared<dai::Device>();
  dai::Pipeline pipeline(device);

  uint16_t rgb_fps{15};  // suitable for data collection for diffusion policy
  uint16_t depth_fps{
      5};  // TODO: may need to update based on obstacle detection results
  RgbStreamer rgb(zmq_context, "ipc:///tmp/oakd_rgb_stream.ipc", rgb_fps);
  DepthStreamer depth(zmq_context, "ipc:///tmp/oakd_depth_stream.ipc",
                      depth_fps);

  rgb.setupPipeline(pipeline);
  depth.setupPipeline(pipeline);

  std::cout << "Starting OAK-D Lite pipeline..." << std::endl;

  pipeline.start();

  rgb.start(&pipeline);
  depth.start(&pipeline);

  // Block main thread until SIGINT
  while (!quitEvent) {
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
  }

  std::cout << "\nShutting down pipelines safely..." << std::endl;

  rgb.stop();
  depth.stop();

  pipeline.stop();
  pipeline.wait();

  return 0;
}
