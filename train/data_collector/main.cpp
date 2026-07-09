#include <atomic>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <thread>
#include <zmq.hpp>

#include "data_collector/Collector.hpp"

namespace fs = std::filesystem;

// The Hardware implementation of the Writer
class DiskDatasetWriter : public IDatasetWriter {
 public:
  DiskDatasetWriter(const std::string& directory) : dir_(directory) {
    fs::create_directories(dir_);
    metadata_.open(dir_ + "/metadata.csv");
    metadata_ << "timestamp,frame_id,v_left,v_right,prompt\n";
  }

  ~DiskDatasetWriter() {
    if (metadata_.is_open()) metadata_.close();
  }

  void saveImage(const std::string& filename, const cv::Mat& image) override {
    cv::imwrite(dir_ + "/" + filename, image);
  }

  void logFrame(double ts, const std::string& fname, float l, float r,
                const std::string& prompt) override {
    metadata_ << std::fixed << std::setprecision(4) << ts << "," << fname << ","
              << l << "," << r << "," << prompt << "\n";
  }

 private:
  std::string dir_;
  std::ofstream metadata_;
};

// --- Threads ---
void control_subscriber_thread(DataCollectorCore& core,
                               std::atomic<bool>& running) {
  zmq::context_t context(1);
  zmq::socket_t sub(context, zmq::socket_type::sub);
  sub.connect("ipc:///tmp/control_command.ipc");
  sub.set(zmq::sockopt::subscribe, "");
  zmq::pollitem_t items[] = {{sub, 0, ZMQ_POLLIN, 0}};

  while (running) {
    zmq::poll(&items[0], 1, std::chrono::milliseconds(100));
    if (items[0].revents & ZMQ_POLLIN) {
      zmq::message_t msg;
      if (sub.recv(msg, zmq::recv_flags::none)) {
        std::string payload(static_cast<char*>(msg.data()), msg.size());
        try {
          size_t l_pos = payload.find("\"v_left\":") + 9;
          size_t r_pos = payload.find("\"v_right\":") + 10;
          float l = std::stof(
              payload.substr(l_pos, payload.find(",", l_pos) - l_pos));
          float r = std::stof(
              payload.substr(r_pos, payload.find("}", r_pos) - r_pos));
          core.onControlMessage(l, r);  // Push to Core
        } catch (...) {                 /* Handle json parse error */
        }
      }
    }
  }
}

void camera_subscriber_thread(DataCollectorCore& core,
                              std::atomic<bool>& running) {
  zmq::context_t context(1);
  zmq::socket_t sub(context, zmq::socket_type::sub);
  sub.connect("ipc:///tmp/oakd_rgb_stream.ipc");
  sub.set(zmq::sockopt::subscribe, "");
  zmq::pollitem_t items[] = {{sub, 0, ZMQ_POLLIN, 0}};

  while (running) {
    zmq::poll(&items[0], 1, std::chrono::milliseconds(100));
    if (items[0].revents & ZMQ_POLLIN) {
      zmq::message_t msg;
      if (sub.recv(msg, zmq::recv_flags::none)) {
        auto now = std::chrono::system_clock::now();
        double ts =
            std::chrono::duration<double>(now.time_since_epoch()).count();

        size_t expected_raw_size = 640 * 480 * 3;  // 921,600 bytes
        cv::Mat raw_img;

        if (msg.size() == expected_raw_size) {
          // It is a verified raw array. Safe to clone.
          raw_img = cv::Mat(480, 640, CV_8UC3, msg.data()).clone();
        } else if (msg.size() > 0) {
          // The payload is smaller! DepthAI is sending a compressed JPEG.
          // Step 1: Deep copy the ZMQ memory into a safe C++ vector FIRST
          std::vector<uchar> safe_buffer((uchar*)msg.data(),
                                         (uchar*)msg.data() + msg.size());

          // Step 2: Dynamically decode the JPEG into an RGB Mat
          raw_img = cv::imdecode(safe_buffer, cv::IMREAD_COLOR);
        }

        if (!raw_img.empty()) {
          core.onCameraMessage(raw_img, ts);  // Push to Core safely
        } else {
          std::cerr << "[ZMQ ERROR] Could not decode image payload of size: "
                    << msg.size() << " bytes.\n";
        }
      }
    }
  }
}

int main() {
  std::string prompt;
  std::cout << "Enter prompt: ";
  std::getline(std::cin, prompt);

  auto now_time = std::time(nullptr);
  std::stringstream dir_name;
  dir_name << "episode_"
           << std::put_time(std::localtime(&now_time), "%Y%m%d_%H%M%S");

  DiskDatasetWriter writer(dir_name.str());
  DataCollectorCore core(writer, prompt);

  std::atomic<bool> running(true);
  std::thread t1(control_subscriber_thread, std::ref(core), std::ref(running));
  std::thread t2(camera_subscriber_thread, std::ref(core), std::ref(running));

  std::cout << "Recording... Press ENTER to stop.\n";
  std::cin.ignore();
  running = false;

  t1.join();
  t2.join();
  return 0;
}
