#include "echo_server.h"
#include "logger.h"
#include "buffer.h"
#include <sstream>

namespace echo_server {

EchoServer::EchoServer(const std::string& address, int port, int thread_num)
    : address_(address),
      port_(port),
      started_(false),
      thread_num_(thread_num),
      next_conn_id_(1),
      main_loop_(std::make_unique<EventLoop>()),
      acceptor_socket_(std::make_unique<Socket>()),
      next_loop_index_(0) {
    
    // 设置默认回调函数
    connection_callback_ = [this](std::shared_ptr<Connection> conn) {
        defaultConnectionCallback(conn);
    };
    
    message_callback_ = [this](std::shared_ptr<Connection> conn, Buffer* buffer) {
        defaultMessageCallback(conn, buffer);
    };
    
    LOG_INFO("EchoServer created on " + address_ + ":" + std::to_string(port_));
}

EchoServer::~EchoServer() {
    stop();
    LOG_INFO("EchoServer destroyed");
}

void EchoServer::start() {
    if (started_.exchange(true)) {
        LOG_WARN("EchoServer already started");
        return;
    }
    
    // 创建并配置监听套接字
    if (!acceptor_socket_->create()) {
        LOG_FATAL("Failed to create acceptor socket");
        return;
    }
    
    acceptor_socket_->setReuseAddress(true);
    acceptor_socket_->setNonBlocking(true);
    
    if (!acceptor_socket_->bind(address_, port_)) {
        LOG_FATAL("Failed to bind acceptor socket");
        return;
    }
    
    if (!acceptor_socket_->listen()) {
        LOG_FATAL("Failed to listen on acceptor socket");
        return;
    }
    
    // 创建工作线程
    if (thread_num_ > 0) {
        loops_.reserve(thread_num_);
        threads_.reserve(thread_num_);
        
        for (int i = 0; i < thread_num_; ++i) {
            auto loop = std::make_unique<EventLoop>();
            auto thread = std::make_unique<std::thread>([this, loop_ptr = loop.get()]() {
                threadFunc(loop_ptr);
            });
            
            loops_.push_back(std::move(loop));
            threads_.push_back(std::move(thread));
        }
    }
    
    // 设置接受连接的事件处理器
    acceptor_handler_ = std::make_shared<EventHandler>(acceptor_socket_->getFd());
    acceptor_handler_->setReadCallback([this]() { handleNewConnection(); });
    acceptor_handler_->enableReading();
    
    main_loop_->addHandler(acceptor_handler_);
    
    LOG_INFO("EchoServer started on " + getServerAddress() + 
             " with " + std::to_string(thread_num_) + " worker threads");
    
    // 启动主事件循环
    main_loop_->run();
}

void EchoServer::stop() {
    if (!started_.exchange(false)) {
        return;
    }
    
    LOG_INFO("Stopping EchoServer...");
    
    // 停止主事件循环
    main_loop_->stop();
    
    // 停止工作线程
    for (auto& loop : loops_) {
        loop->stop();
    }
    
    // 等待工作线程结束
    for (auto& thread : threads_) {
        if (thread->joinable()) {
            thread->join();
        }
    }
    
    // 关闭所有连接
    {
        std::lock_guard<std::mutex> lock(connections_mutex_);
        for (auto& pair : connections_) {
            pair.second->forceClose();
        }
        connections_.clear();
    }
    
    LOG_INFO("EchoServer stopped");
}

void EchoServer::setThreadNum(int num_threads) {
    if (started_) {
        LOG_WARN("Cannot set thread number after server started");
        return;
    }
    thread_num_ = num_threads;
}

void EchoServer::handleNewConnection() {
    auto client_socket = acceptor_socket_->accept();
    if (!client_socket) {
        return;
    }
    
    // 设置客户端套接字为非阻塞
    client_socket->setNonBlocking(true);
    client_socket->setTcpNoDelay(true);
    
    // 生成连接名称
    std::ostringstream oss;
    oss << "Connection-" << next_conn_id_++;
    std::string conn_name = oss.str();
    
    // 选择事件循环
    EventLoop* io_loop = getNextLoop();
    
    // 创建连接对象
    auto conn = std::make_shared<Connection>(io_loop, std::move(client_socket), conn_name);
    
    // 设置回调函数
    conn->setMessageCallback(message_callback_);
    conn->setWriteCompleteCallback(write_complete_callback_);
    conn->setCloseCallback([this](std::shared_ptr<Connection> c) {
        removeConnection(c);
    });
    conn->setErrorCallback([this](std::shared_ptr<Connection> c) {
        LOG_ERROR("Connection error: " + c->getName());
        removeConnection(c);
    });
    
    // 添加到连接映射
    {
        std::lock_guard<std::mutex> lock(connections_mutex_);
        connections_[conn_name] = conn;
    }
    
    // 建立连接
    conn->establishConnection();
    
    // 调用连接回调
    if (connection_callback_) {
        connection_callback_(conn);
    }
}

void EchoServer::removeConnection(std::shared_ptr<Connection> conn) {
    main_loop_->runInLoop([this, conn]() {
        removeConnectionInLoop(conn);
    });
}

void EchoServer::removeConnectionInLoop(std::shared_ptr<Connection> conn) {
    {
        std::lock_guard<std::mutex> lock(connections_mutex_);
        connections_.erase(conn->getName());
    }
    
    // 在连接所属的事件循环中销毁连接
    conn->destroyConnection();
    
    LOG_INFO("Connection removed: " + conn->getName());
}

void EchoServer::defaultConnectionCallback(std::shared_ptr<Connection> conn) {
    LOG_INFO("New connection: " + conn->getName() + " from " + conn->getPeerAddress());
}

void EchoServer::defaultMessageCallback(std::shared_ptr<Connection> conn, Buffer* buffer) {
    // Echo功能：将接收到的数据原样发送回去
    std::string message = buffer->retrieveAllAsString();
    
    LOG_DEBUG("Received message from " + conn->getName() + ": " + 
              (message.size() > 100 ? message.substr(0, 100) + "..." : message));
    
    // 发送回客户端
    conn->send(message);
}

void EchoServer::threadFunc(EventLoop* loop) {
    LOG_INFO("Worker thread started");
    loop->run();
    LOG_INFO("Worker thread stopped");
}

EventLoop* EchoServer::getNextLoop() {
    if (loops_.empty()) {
        return main_loop_.get();
    }
    
    int index = next_loop_index_.fetch_add(1) % loops_.size();
    return loops_[index].get();
}

} // namespace echo_server