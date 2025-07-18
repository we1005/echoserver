#ifndef ECHO_SERVER_CONNECTION_H
#define ECHO_SERVER_CONNECTION_H

#include <memory>
#include <string>
#include <functional>
#include <atomic>
#include "buffer.h"
#include "socket.h"
#include "event_loop.h"

namespace echo_server {

class Buffer;
class EventLoop;

/**
 * @brief 连接状态枚举
 */
enum class ConnectionState {
    CONNECTING,   ///< 连接中
    CONNECTED,    ///< 已连接
    DISCONNECTING, ///< 断开连接中
    DISCONNECTED  ///< 已断开连接
};

/**
 * @brief TCP连接类
 * 
 * 封装TCP连接的管理，包括数据收发、连接状态管理等
 * 采用非阻塞I/O和事件驱动模式
 */
class Connection : public std::enable_shared_from_this<Connection> {
public:
    using MessageCallback = std::function<void(std::shared_ptr<Connection>, Buffer*)>;
    using CloseCallback = std::function<void(std::shared_ptr<Connection>)>;
    using ErrorCallback = std::function<void(std::shared_ptr<Connection>)>;
    using WriteCompleteCallback = std::function<void(std::shared_ptr<Connection>)>;

    /**
     * @brief 构造函数
     * @param loop 事件循环
     * @param socket 套接字
     * @param name 连接名称
     */
    Connection(EventLoop* loop, std::unique_ptr<Socket> socket, const std::string& name);

    /**
     * @brief 析构函数
     */
    ~Connection();

    // 禁用拷贝构造和赋值
    Connection(const Connection&) = delete;
    Connection& operator=(const Connection&) = delete;

    /**
     * @brief 建立连接
     */
    void establishConnection();

    /**
     * @brief 销毁连接
     */
    void destroyConnection();

    /**
     * @brief 发送数据
     * @param data 数据指针
     * @param len 数据长度
     */
    void send(const void* data, size_t len);

    /**
     * @brief 发送字符串数据
     * @param message 字符串消息
     */
    void send(const std::string& message);

    /**
     * @brief 关闭连接
     */
    void shutdown();

    /**
     * @brief 强制关闭连接
     */
    void forceClose();

    /**
     * @brief 设置消息回调函数
     * @param callback 回调函数
     */
    void setMessageCallback(const MessageCallback& callback) {
        message_callback_ = callback;
    }

    /**
     * @brief 设置连接关闭回调函数
     * @param callback 回调函数
     */
    void setCloseCallback(const CloseCallback& callback) {
        close_callback_ = callback;
    }

    /**
     * @brief 设置错误回调函数
     * @param callback 回调函数
     */
    void setErrorCallback(const ErrorCallback& callback) {
        error_callback_ = callback;
    }

    /**
     * @brief 设置写完成回调函数
     * @param callback 回调函数
     */
    void setWriteCompleteCallback(const WriteCompleteCallback& callback) {
        write_complete_callback_ = callback;
    }

    /**
     * @brief 获取连接名称
     * @return const std::string& 连接名称
     */
    const std::string& getName() const { return name_; }

    /**
     * @brief 获取连接状态
     * @return ConnectionState 连接状态
     */
    ConnectionState getState() const { return state_; }

    /**
     * @brief 检查连接是否已建立
     * @return bool 是否已连接
     */
    bool isConnected() const { return state_ == ConnectionState::CONNECTED; }

    /**
     * @brief 获取本地地址
     * @return std::string 本地地址
     */
    std::string getLocalAddress() const;

    /**
     * @brief 获取对端地址
     * @return std::string 对端地址
     */
    std::string getPeerAddress() const;

    /**
     * @brief 获取套接字文件描述符
     * @return int 文件描述符
     */
    int getFd() const;

private:
    /**
     * @brief 处理读事件
     */
    void handleRead();

    /**
     * @brief 处理写事件
     */
    void handleWrite();

    /**
     * @brief 处理关闭事件
     */
    void handleClose();

    /**
     * @brief 处理错误事件
     */
    void handleError();

    /**
     * @brief 在事件循环中发送数据
     * @param data 数据
     */
    void sendInLoop(const std::string& data);

    /**
     * @brief 在事件循环中关闭连接
     */
    void shutdownInLoop();

    /**
     * @brief 在事件循环中强制关闭连接
     */
    void forceCloseInLoop();

    EventLoop* loop_;                           ///< 事件循环
    std::unique_ptr<Socket> socket_;           ///< 套接字
    std::shared_ptr<EventHandler> handler_;    ///< 事件处理器
    std::string name_;                         ///< 连接名称
    std::atomic<ConnectionState> state_;       ///< 连接状态
    
    std::unique_ptr<Buffer> input_buffer_;     ///< 输入缓冲区
    std::unique_ptr<Buffer> output_buffer_;    ///< 输出缓冲区
    
    MessageCallback message_callback_;         ///< 消息回调
    CloseCallback close_callback_;             ///< 关闭回调
    ErrorCallback error_callback_;             ///< 错误回调
    WriteCompleteCallback write_complete_callback_; ///< 写完成回调
    
    static std::atomic<uint64_t> connection_id_; ///< 连接ID生成器
};

} // namespace echo_server

#endif // ECHO_SERVER_CONNECTION_H