#pragma once

#include <atomic>
#include <depthai/depthai.hpp>
#include <functional>
#include <iostream>
#include <string>
#include <thread>
#include <vector>
#include <zmq.hpp>

// Shared global kill switch
extern std::atomic<bool> quitEvent;

class BaseStreamer {
 protected:
  zmq::socket_t publisher_;
  std::string endpoint_;

  std::thread worker_thread_;
  std::function<void(dai::Pipeline*)> loop_function_;

 public:
  BaseStreamer(zmq::context_t& context, const std::string& endpoint)
      : publisher_(context, ZMQ_PUB), endpoint_(endpoint) {
    publisher_.bind(endpoint_);
    std::cout << "ZeroMQ Publisher bound to " << endpoint_ << std::endl;
  }

  virtual ~BaseStreamer() { stop(); }

  // Inheriting classes must implement this to attach their nodes to the
  // pipeline
  virtual void setupPipeline(dai::Pipeline& pipeline) = 0;

  // Starts the worker thread executing the derived class's processing loop
  void start(dai::Pipeline* pipeline) {
    if (loop_function_) {
      worker_thread_ = std::thread(loop_function_, pipeline);
    }
  }

  // Safely joins the worker thread
  void stop() {
    if (worker_thread_.joinable()) {
      worker_thread_.join();
    }
  }
};
