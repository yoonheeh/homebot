#include <zmq.hpp>
#include <iostream>
#include <string>

int main() {
    // Initialize the ZeroMQ context (1 thread is standard)
    zmq::context_t context(1);
    
    // Create a SUB (Subscriber) socket
    zmq::socket_t subscriber(context, zmq::socket_type::sub);
    
    std::clog << "Connecting to velocity publisher at tcp://localhost:5556...\n";
    subscriber.connect("tcp://localhost:5556");
    
    // Subscribe to all messages (empty string means filter nothing)
    subscriber.set(zmq::sockopt::subscribe, "");

    std::clog << "Listening for velocity commands...\n";

    while (true) {
        zmq::message_t message;
        
        // Block and wait for a message to arrive
        auto res = subscriber.recv(message, zmq::recv_flags::none);
        
        if (res) {
            // Convert the raw message payload back into a standard C++ string
            std::string payload(static_cast<char*>(message.data()), message.size());
            std::cout << "[CLIENT RECEIVER] " << payload << std::endl;
        }
    }

    return 0;
}
