#ifndef ECHO_SERVER_ECHO_SERVER_H
#define ECHO_SERVER_ECHO_SERVER_H

#include <memory>
#include <string>
#include <unordered_map>
#include <atomic>
#include <thread>
#include <vector>
#include <mutex>
#include "event_loop.h"
#include "socket.h"
#include "connection.h"

namespace echo_server {

class EventLoop;
class Socket;
class Connection;

/**
 * @brief Echo服务器主类
 * 
 * 高性能的Echo服务器实现，支持多线程、连接管理、优雅关闭等功能
 * 采用Reactor模式和线程池处理并发连接
 */
class EchoServer {
public:
    /**
     * @brief 构造函数
     * @param address 监听地址
     * @param port 监听端口
     * @param thread_num 工作线程数量，0表示使用主线程
     */
    EchoServer(const std::string& address, int port, int thread_num = 0);

    /**
     * @brief 析构函数
     */
    ~EchoServer();

    // 禁用拷贝构造和赋值
    EchoServer(const EchoServer&) = delete;
    EchoServer& operator=(const EchoServer&) = delete;

    /**
     * @brief 启动服务器
     */
    void start();

    /**
     * @brief 停止服务器
     */
    void stop();

    /**
     * @brief 设置线程数量
     * @param num_threads 线程数量
     */
    void setThreadNum(int num_threads);

    /**
     * @brief 设置连接建立回调
     * @param callback 回调函数
     */
    void setConnectionCallback(const std::function<void(std::shared_ptr<Connection>)>& callback) {
        connection_callback_ = callback;
    }

    /**
     * @brief 设置消息回调
     * @param callback 回调函数
     */
    void setMessageCallback(const std::function<void(std::shared_ptr<Connection>, Buffer*)>& callback) {
        message_callback_ = callback;
    }

    /**
     * @brief 设置写完成回调
     * @param callback 回调函数
     */
    void setWriteCompleteCallback(const std::function<void(std::shared_ptr<Connection>)>& callback) {
        write_complete_callback_ = callback;
    }

    /**
     * @brief 获取当前连接数
     * @return size_t 连接数
     */
    size_t getConnectionCount() const {
        return connections_.size();
    }

    /**
     * @brief 获取服务器地址
     * @return std::string 服务器地址
     */
    std::string getServerAddress() const {
        return address_ + ":" + std::to_string(port_);
    }

    /**
     * @brief 检查服务器是否正在运行
     * @return bool 是否运行中
     */
    bool isRunning() const {
        return started_;
    }

private:
    /**
     * @brief 处理新连接
     */
    void handleNewConnection();

    /**
     * @brief 移除连接
     * @param conn 连接对象
     */
    void removeConnection(std::shared_ptr<Connection> conn);

    /**
     * @brief 在事件循环中移除连接
     * @param conn 连接对象
     */
    void removeConnectionInLoop(std::shared_ptr<Connection> conn);

    /**
     * @brief 默认连接回调
     * @param conn 连接对象
     */
    void defaultConnectionCallback(std::shared_ptr<Connection> conn);

    /**
     * @brief 默认消息回调（Echo功能）
     * @param conn 连接对象
     * @param buffer 消息缓冲区
     */
    void defaultMessageCallback(std::shared_ptr<Connection> conn, Buffer* buffer);

    /**
     * @brief 工作线程函数
     * @param loop 事件循环
     */
    void threadFunc(EventLoop* loop);

    /**
     * @brief 获取下一个事件循环
     * @return EventLoop* 事件循环指针
     */
    EventLoop* getNextLoop();

    std::string address_;           ///< 监听地址
    int port_;                      ///< 监听端口
    std::atomic<bool> started_;     ///< 启动状态
    int thread_num_;                ///< 线程数量
    int next_conn_id_;              ///< 下一个连接ID
    
    std::unique_ptr<EventLoop> main_loop_;      ///< 主事件循环
    std::unique_ptr<Socket> acceptor_socket_;   ///< 监听套接字
    std::shared_ptr<EventHandler> acceptor_handler_; ///< 监听事件处理器
    
    std::vector<std::unique_ptr<EventLoop>> loops_;     ///< 工作线程事件循环
    std::vector<std::unique_ptr<std::thread>> threads_; ///< 工作线程
    std::atomic<int> next_loop_index_;                  ///< 下一个循环索引
    
    std::unordered_map<std::string, std::shared_ptr<Connection>> connections_; ///< 连接映射
    mutable std::mutex connections_mutex_;  ///< 连接映射互斥锁
    
    // 回调函数
    std::function<void(std::shared_ptr<Connection>)> connection_callback_;
    std::function<void(std::shared_ptr<Connection>, Buffer*)> message_callback_;
    std::function<void(std::shared_ptr<Connection>)> write_complete_callback_;
};

} // namespace echo_server

#endif // ECHO_SERVER_ECHO_SERVER_H