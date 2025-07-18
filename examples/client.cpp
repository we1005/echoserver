#include <iostream>
#include <string>
#include <thread>
#include <chrono>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <cstring>

class SimpleClient {
public:
    SimpleClient(const std::string& server_ip, int server_port)
        : server_ip_(server_ip), server_port_(server_port), socket_fd_(-1) {}
    
    ~SimpleClient() {
        disconnect();
    }
    
    bool connect() {
        socket_fd_ = socket(AF_INET, SOCK_STREAM, 0);
        if (socket_fd_ < 0) {
            std::cerr << "Failed to create socket" << std::endl;
            return false;
        }
        
        sockaddr_in server_addr{};
        server_addr.sin_family = AF_INET;
        server_addr.sin_port = htons(server_port_);
        
        if (inet_pton(AF_INET, server_ip_.c_str(), &server_addr.sin_addr) <= 0) {
            std::cerr << "Invalid server address" << std::endl;
            return false;
        }
        
        if (::connect(socket_fd_, reinterpret_cast<sockaddr*>(&server_addr), sizeof(server_addr)) < 0) {
            std::cerr << "Failed to connect to server" << std::endl;
            return false;
        }
        
        std::cout << "Connected to " << server_ip_ << ":" << server_port_ << std::endl;
        return true;
    }
    
    void disconnect() {
        if (socket_fd_ >= 0) {
            close(socket_fd_);
            socket_fd_ = -1;
            std::cout << "Disconnected from server" << std::endl;
        }
    }
    
    bool sendMessage(const std::string& message) {
        if (socket_fd_ < 0) {
            std::cerr << "Not connected to server" << std::endl;
            return false;
        }
        
        ssize_t sent = send(socket_fd_, message.c_str(), message.length(), 0);
        if (sent < 0) {
            std::cerr << "Failed to send message" << std::endl;
            return false;
        }
        
        std::cout << "Sent: " << message << std::endl;
        return true;
    }
    
    std::string receiveMessage() {
        if (socket_fd_ < 0) {
            std::cerr << "Not connected to server" << std::endl;
            return "";
        }
        
        char buffer[1024];
        ssize_t received = recv(socket_fd_, buffer, sizeof(buffer) - 1, 0);
        if (received < 0) {
            std::cerr << "Failed to receive message" << std::endl;
            return "";
        }
        
        buffer[received] = '\0';
        std::string message(buffer);
        std::cout << "Received: " << message << std::endl;
        return message;
    }
    
private:
    std::string server_ip_;
    int server_port_;
    int socket_fd_;
};

void interactiveMode(const std::string& server_ip, int server_port) {
    SimpleClient client(server_ip, server_port);
    
    if (!client.connect()) {
        return;
    }
    
    std::cout << "\nInteractive mode started. Type 'quit' to exit.\n" << std::endl;
    
    std::string input;
    while (true) {
        std::cout << "Enter message: ";
        std::getline(std::cin, input);
        
        if (input == "quit" || input == "exit") {
            break;
        }
        
        if (input.empty()) {
            continue;
        }
        
        if (client.sendMessage(input)) {
            client.receiveMessage();
        }
    }
}

void benchmarkMode(const std::string& server_ip, int server_port, int num_messages) {
    SimpleClient client(server_ip, server_port);
    
    if (!client.connect()) {
        return;
    }
    
    std::cout << "\nBenchmark mode: sending " << num_messages << " messages...\n" << std::endl;
    
    auto start_time = std::chrono::high_resolution_clock::now();
    
    for (int i = 0; i < num_messages; ++i) {
        std::string message = "Benchmark message " + std::to_string(i + 1);
        
        if (!client.sendMessage(message)) {
            std::cerr << "Failed to send message " << i + 1 << std::endl;
            break;
        }
        
        std::string response = client.receiveMessage();
        if (response != message) {
            std::cerr << "Echo mismatch at message " << i + 1 << std::endl;
        }
        
        // 小延迟避免过快发送 (可通过参数控制)
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    
    auto end_time = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
    
    std::cout << "\nBenchmark completed in " << duration.count() << " ms" << std::endl;
    std::cout << "Average: " << (duration.count() / static_cast<double>(num_messages)) << " ms per message" << std::endl;
}

void printUsage(const char* program_name) {
    std::cout << "Usage: " << program_name << " [options]\n"
              << "Options:\n"
              << "  -h, --help              Show this help message\n"
              << "  -s, --server HOST       Server address (default: 127.0.0.1)\n"
              << "  -p, --port PORT         Server port (default: 8080)\n"
              << "  -b, --benchmark NUM     Benchmark mode with NUM messages\n"
              << "  -i, --interactive       Interactive mode (default)\n"
              << "\nExamples:\n"
              << "  " << program_name << " -s 192.168.1.100 -p 9999\n"
              << "  " << program_name << " -b 1000\n";
}

int main(int argc, char* argv[]) {
    std::string server_ip = "127.0.0.1";
    int server_port = 8080;
    bool benchmark_mode = false;
    int num_messages = 100;
    
    // 解析命令行参数
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        
        if (arg == "-h" || arg == "--help") {
            printUsage(argv[0]);
            return 0;
        } else if (arg == "-s" || arg == "--server") {
            if (i + 1 < argc) {
                server_ip = argv[++i];
            } else {
                std::cerr << "Error: --server requires a value" << std::endl;
                return 1;
            }
        } else if (arg == "-p" || arg == "--port") {
            if (i + 1 < argc) {
                server_port = std::atoi(argv[++i]);
                if (server_port <= 0 || server_port > 65535) {
                    std::cerr << "Error: Invalid port number" << std::endl;
                    return 1;
                }
            } else {
                std::cerr << "Error: --port requires a value" << std::endl;
                return 1;
            }
        } else if (arg == "-b" || arg == "--benchmark") {
            benchmark_mode = true;
            if (i + 1 < argc && argv[i + 1][0] != '-') {
                num_messages = std::atoi(argv[++i]);
                if (num_messages <= 0) {
                    std::cerr << "Error: Invalid number of messages" << std::endl;
                    return 1;
                }
            }
        } else if (arg == "-i" || arg == "--interactive") {
            benchmark_mode = false;
        } else {
            std::cerr << "Error: Unknown option: " << arg << std::endl;
            printUsage(argv[0]);
            return 1;
        }
    }
    
    std::cout << "EchoServer Test Client" << std::endl;
    std::cout << "Connecting to " << server_ip << ":" << server_port << std::endl;
    
    if (benchmark_mode) {
        benchmarkMode(server_ip, server_port, num_messages);
    } else {
        interactiveMode(server_ip, server_port);
    }
    
    return 0;
}