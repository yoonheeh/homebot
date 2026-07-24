#include <atomic>
#include <csignal>
#include <depthai/depthai.hpp>
#include <iostream>
#include <memory>
#include <nlohmann/json.hpp>
#include <zmq.hpp>

#include "mechanism/perception/stream/depth_streamer.h"
#include "mechanism/perception/stream/rgb_streamer.h"

using json = nlohmann::json;

std::atomic<bool> quitEvent(false);

void signalHandler(int) { quitEvent = true; }

int main() {
  signal(SIGTERM, signalHandler);
  signal(SIGINT, signalHandler);

  // Initialize unified resources
  zmq::context_t zmq_context(1);
  std::shared_ptr<dai::Device> device = std::make_shared<dai::Device>();
  dai::Pipeline pipeline(device);

  // Load config
  std::ifstream f("vision_config.json");
  if (!f.is_open()) {
    std::cerr << "[Error] Could not find vision_config.json!" << std::endl;
    return 1;
  }

  // Parse the file into the JSON object
  json config = json::parse(f);

  int rgb_width = config["rgb"]["width"];
  int rgb_height = config["rgb"]["height"];
  int depth_width = config["depth"]["width"];
  int depth_height = config["depth"]["height"];
  std::string rgb_ipc = config["pipeline"]["rgb_ipc"];
  std::string depth_ipc = config["pipeline"]["depth_ipc"];
  uint16_t rgb_fps = config["rgb"]["fps"];
  uint16_t depth_fps = config["depth"]["fps"];
  RgbStreamer rgb(zmq_context, rgb_ipc, rgb_fps, rgb_width, rgb_height);
  DepthStreamer depth(zmq_context, depth_ipc, depth_fps, depth_width,
                      depth_height);

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
