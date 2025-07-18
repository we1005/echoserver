#ifndef ECHO_SERVER_EVENT_LOOP_H
#define ECHO_SERVER_EVENT_LOOP_H

#include <functional>
#include <unordered_map>
#include <vector>
#include <memory>
#include <atomic>
#include <mutex>
#include <thread>
#include <sys/epoll.h>

namespace echo_server {

class Socket;

/**
 * @brief 事件类型枚举
 */
enum class EventType : uint32_t {
    READ = EPOLLIN,           ///< 可读事件
    WRITE = EPOLLOUT,         ///< 可写事件
    ERROR = EPOLLERR,         ///< 错误事件
    HANGUP = EPOLLHUP,        ///< 挂起事件
    EDGE_TRIGGERED = EPOLLET  ///< 边缘触发模式
};

/**
 * @brief 事件回调函数类型
 */
using EventCallback = std::function<void()>;

/**
 * @brief 事件处理器
 * 
 * 封装文件描述符及其相关的事件回调函数
 */
class EventHandler {
public:
    /**
     * @brief 构造函数
     * @param fd 文件描述符
     */
    explicit EventHandler(int fd);

    /**
     * @brief 析构函数
     */
    ~EventHandler() = default;

    /**
     * @brief 设置可读事件回调
     * @param callback 回调函数
     */
    void setReadCallback(const EventCallback& callback);

    /**
     * @brief 设置可写事件回调
     * @param callback 回调函数
     */
    void setWriteCallback(const EventCallback& callback);

    /**
     * @brief 设置错误事件回调
     * @param callback 回调函数
     */
    void setErrorCallback(const EventCallback& callback);

    /**
     * @brief 设置关闭事件回调
     * @param callback 回调函数
     */
    void setCloseCallback(const EventCallback& callback);

    /**
     * @brief 处理事件
     * @param events 事件掩码
     */
    void handleEvents(uint32_t events);

    /**
     * @brief 获取文件描述符
     * @return int 文件描述符
     */
    int getFd() const { return fd_; }

    /**
     * @brief 启用读事件
     */
    void enableReading() { events_ |= static_cast<uint32_t>(EventType::READ); }

    /**
     * @brief 启用写事件
     */
    void enableWriting() { events_ |= static_cast<uint32_t>(EventType::WRITE); }

    /**
     * @brief 禁用写事件
     */
    void disableWriting() { events_ &= ~static_cast<uint32_t>(EventType::WRITE); }

    /**
     * @brief 禁用所有事件
     */
    void disableAll() { events_ = 0; }

    /**
     * @brief 获取当前事件掩码
     * @return uint32_t 事件掩码
     */
    uint32_t getEvents() const { return events_; }

    /**
     * @brief 检查是否没有事件
     * @return bool 是否没有事件
     */
    bool isNoneEvent() const { return events_ == 0; }

private:
    int fd_;                    ///< 文件描述符
    uint32_t events_;          ///< 关注的事件
    EventCallback read_callback_;   ///< 读事件回调
    EventCallback write_callback_;  ///< 写事件回调
    EventCallback error_callback_;  ///< 错误事件回调
    EventCallback close_callback_;  ///< 关闭事件回调
};

/**
 * @brief 事件循环类
 * 
 * 基于epoll的高性能事件循环，支持I/O多路复用
 * 采用Reactor模式处理网络事件
 */
class EventLoop {
public:
    /**
     * @brief 构造函数
     */
    EventLoop();

    /**
     * @brief 析构函数
     */
    ~EventLoop();

    // 禁用拷贝构造和赋值
    EventLoop(const EventLoop&) = delete;
    EventLoop& operator=(const EventLoop&) = delete;

    /**
     * @brief 启动事件循环
     */
    void run();

    /**
     * @brief 停止事件循环
     */
    void stop();

    /**
     * @brief 添加事件处理器
     * @param handler 事件处理器
     */
    void addHandler(std::shared_ptr<EventHandler> handler);

    /**
     * @brief 更新事件处理器
     * @param handler 事件处理器
     */
    void updateHandler(std::shared_ptr<EventHandler> handler);

    /**
     * @brief 移除事件处理器
     * @param handler 事件处理器
     */
    void removeHandler(std::shared_ptr<EventHandler> handler);

    /**
     * @brief 在事件循环中执行任务
     * @param task 任务函数
     */
    void runInLoop(const std::function<void()>& task);

    /**
     * @brief 检查是否在事件循环线程中
     * @return bool 是否在事件循环线程
     */
    bool isInLoopThread() const;

private:
    /**
     * @brief 处理epoll事件
     * @param timeout_ms 超时时间（毫秒）
     */
    void handleEvents(int timeout_ms = -1);

    /**
     * @brief 执行待处理任务
     */
    void doPendingTasks();

    /**
     * @brief 唤醒事件循环
     */
    void wakeup();

    /**
     * @brief 处理唤醒事件
     */
    void handleWakeup();

    int epoll_fd_;  ///< epoll文件描述符
    int wakeup_fd_; ///< 唤醒文件描述符
    std::atomic<bool> running_;  ///< 运行状态
    std::atomic<bool> quit_;     ///< 退出标志
    
    std::unordered_map<int, std::shared_ptr<EventHandler>> handlers_;  ///< 事件处理器映射
    std::vector<epoll_event> active_events_;  ///< 活跃事件列表
    
    std::vector<std::function<void()>> pending_tasks_;  ///< 待处理任务
    std::mutex tasks_mutex_;  ///< 任务队列互斥锁
    
    std::thread::id loop_thread_id_;  ///< 事件循环线程ID
    
    static const int kMaxEvents = 1024;  ///< 最大事件数量
};

} // namespace echo_server

#endif // ECHO_SERVER_EVENT_LOOP_H