#include <atomic>
#include <csignal>
#include <iostream>
#include <memory>
#include <vector>
#include <zmq.hpp>
#include <opencv2/opencv.hpp>
#include <depthai/depthai.hpp>

std::atomic<bool> quitEvent(false);

void signalHandler(int) {
    quitEvent = true;
}

int main() {
    // Initialize ZeroMQ Context and Publisher Socket
    zmq::context_t context(1);
    zmq::socket_t publisher(context, ZMQ_PUB);
    publisher.bind("tcp://*:5556");
    std::cout << "ZeroMQ Publisher bound to tcp://*:5556" << std::endl;

    std::vector<uchar> buffer;
    std::vector<int> params = {cv::IMWRITE_JPEG_QUALITY, 80}; // 80% compression quality

    signal(SIGTERM, signalHandler);
    signal(SIGINT, signalHandler);

    // Create device
    std::shared_ptr<dai::Device> device = std::make_shared<dai::Device>();
    std::cout << "OAK-D Lite pipeline started successfully." << std::endl;

    // Create pipeline
    dai::Pipeline pipeline(device);

    // Create nodes
    auto cam = pipeline.create<dai::node::Camera>()->build();
    auto videoQueue = cam->requestOutput(std::make_pair(640, 400))->createOutputQueue();

    // Start pipeline
    pipeline.start();

    while(pipeline.isRunning() && !quitEvent) {
        auto videoIn = videoQueue->get<dai::ImgFrame>();
        if(videoIn == nullptr) continue;
        // Wrap the raw YUV420p byte array into a 1-channel OpenCV Mat
        // Height is multiplied by 1.5 to account for the U and V color planes
        cv::Mat yuv(videoIn->getHeight() * 3 / 2, videoIn->getWidth(), CV_8UC1, videoIn->getData().data());
        
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


