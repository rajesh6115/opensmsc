
#include <iostream>
#include <thread>
#include <memory>

class TCPServer {
public:
    void start() {
        std::cout << "SMPP TCP Server initialized" << std::endl;
    }
};

int main() {
    try {
        auto server = std::make_unique<TCPServer>();
        server->start();
        std::cout << "Build successful!" << std::endl;
        return 0;
    } catch(const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
}
