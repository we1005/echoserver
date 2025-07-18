#include "echo_server.h"
#include "logger.h"
#include <iostream>
#include <signal.h>
#include <thread>

using namespace echo_server;

// 全局服务器实例
std::unique_ptr<EchoServer> g_server;

// 信号处理函数
void signalHandler(int sig) {
    LOG_INFO("Received signal " + std::to_string(sig) + ", shutting down server...");
    if (g_server) {
        g_server->stop();
    }
}

// 打印使用说明
void printUsage(const char* program_name) {
    std::cout << "Usage: " << program_name << " [options]\n"
              << "Options:\n"
              << "  -h, --help           Show this help message\n"
              << "  -p, --port PORT      Listen port (default: 8080)\n"
              << "  -a, --address ADDR   Listen address (default: 0.0.0.0)\n"
              << "  -t, --threads NUM    Number of worker threads (default: CPU cores)\n"
              << "  -l, --log-level LVL  Log level: DEBUG, INFO, WARN, ERROR, FATAL (default: INFO)\n"
              << "  -f, --log-file FILE  Log file path (default: console only)\n"
              << "\nExample:\n"
              << "  " << program_name << " -p 9999 -t 4 -l DEBUG\n";
}

// 解析日志级别
LogLevel parseLogLevel(const std::string& level) {
    if (level == "DEBUG") return LogLevel::DEBUG;
    if (level == "INFO") return LogLevel::INFO;
    if (level == "WARN") return LogLevel::WARN;
    if (level == "ERROR") return LogLevel::ERROR;
    if (level == "FATAL") return LogLevel::FATAL;
    return LogLevel::INFO;
}

int main(int argc, char* argv[]) {
    // 默认参数
    std::string address = "0.0.0.0";
    int port = 8080;
    int thread_num = std::thread::hardware_concurrency();
    LogLevel log_level = LogLevel::INFO;
    std::string log_file;
    
    // 解析命令行参数
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        
        if (arg == "-h" || arg == "--help") {
            printUsage(argv[0]);
            return 0;
        } else if (arg == "-p" || arg == "--port") {
            if (i + 1 < argc) {
                port = std::atoi(argv[++i]);
                if (port <= 0 || port > 65535) {
                    std::cerr << "Error: Invalid port number: " << port << std::endl;
                    return 1;
                }
            } else {
                std::cerr << "Error: --port requires a value" << std::endl;
                return 1;
            }
        } else if (arg == "-a" || arg == "--address") {
            if (i + 1 < argc) {
                address = argv[++i];
            } else {
                std::cerr << "Error: --address requires a value" << std::endl;
                return 1;
            }
        } else if (arg == "-t" || arg == "--threads") {
            if (i + 1 < argc) {
                thread_num = std::atoi(argv[++i]);
                if (thread_num < 0) {
                    std::cerr << "Error: Invalid thread number: " << thread_num << std::endl;
                    return 1;
                }
            } else {
                std::cerr << "Error: --threads requires a value" << std::endl;
                return 1;
            }
        } else if (arg == "-l" || arg == "--log-level") {
            if (i + 1 < argc) {
                log_level = parseLogLevel(argv[++i]);
            } else {
                std::cerr << "Error: --log-level requires a value" << std::endl;
                return 1;
            }
        } else if (arg == "-f" || arg == "--log-file") {
            if (i + 1 < argc) {
                log_file = argv[++i];
            } else {
                std::cerr << "Error: --log-file requires a value" << std::endl;
                return 1;
            }
        } else {
            std::cerr << "Error: Unknown option: " << arg << std::endl;
            printUsage(argv[0]);
            return 1;
        }
    }
    
    // 配置日志系统
    Logger::getInstance().setLogLevel(log_level);
    if (!log_file.empty()) {
        if (!Logger::getInstance().setLogFile(log_file)) {
            std::cerr << "Error: Failed to open log file: " << log_file << std::endl;
            return 1;
        }
    }
    
    // 设置信号处理
    signal(SIGINT, signalHandler);
    signal(SIGTERM, signalHandler);
    signal(SIGPIPE, SIG_IGN); // 忽略SIGPIPE信号
    
    try {
        // 创建并启动服务器
        g_server = std::make_unique<EchoServer>(address, port, thread_num);
        
        LOG_INFO("Starting EchoServer...");
        LOG_INFO("Configuration:");
        LOG_INFO("  Address: " + address);
        LOG_INFO("  Port: " + std::to_string(port));
        LOG_INFO("  Worker threads: " + std::to_string(thread_num));
        LOG_INFO("  Log level: " + std::to_string(static_cast<int>(log_level)));
        if (!log_file.empty()) {
            LOG_INFO("  Log file: " + log_file);
        }
        
        // 启动服务器（阻塞调用）
        g_server->start();
        
    } catch (const std::exception& e) {
        LOG_FATAL("Server error: " + std::string(e.what()));
        return 1;
    } catch (...) {
        LOG_FATAL("Unknown server error");
        return 1;
    }
    
    LOG_INFO("EchoServer shutdown complete");
    return 0;
}