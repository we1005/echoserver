#include "connection.h"
#include "buffer.h"
#include "logger.h"
#include <errno.h>
#include <cstring>

namespace echo_server {

std::atomic<uint64_t> Connection::connection_id_(0);

Connection::Connection(EventLoop* loop, std::unique_ptr<Socket> socket, const std::string& name)
    : loop_(loop),
      socket_(std::move(socket)),
      handler_(std::make_shared<EventHandler>(socket_->getFd())),
      name_(name),
      state_(ConnectionState::CONNECTING),
      input_buffer_(std::make_unique<Buffer>()),
      output_buffer_(std::make_unique<Buffer>()) {
    
    // 设置事件回调
    handler_->setReadCallback([this]() { handleRead(); });
    handler_->setWriteCallback([this]() { handleWrite(); });
    handler_->setCloseCallback([this]() { handleClose(); });
    handler_->setErrorCallback([this]() { handleError(); });
    
    // 设置套接字选项
    socket_->setTcpNoDelay(true);
    
    LOG_DEBUG("Connection created: " + name_);
}

Connection::~Connection() {
    LOG_DEBUG("Connection destroyed: " + name_);
}

void Connection::establishConnection() {
    loop_->runInLoop([this]() {
        state_ = ConnectionState::CONNECTED;
        handler_->enableReading();
        loop_->addHandler(handler_);
        
        LOG_INFO("Connection established: " + name_ + " from " + getPeerAddress());
    });
}

void Connection::destroyConnection() {
    loop_->runInLoop([this]() {
        if (state_ == ConnectionState::CONNECTED) {
            state_ = ConnectionState::DISCONNECTED;
            handler_->disableAll();
            loop_->removeHandler(handler_);
            
            if (close_callback_) {
                close_callback_(shared_from_this());
            }
        }
    });
}

void Connection::send(const void* data, size_t len) {
    if (state_ != ConnectionState::CONNECTED) {
        LOG_WARN("Connection not connected, cannot send data: " + name_);
        return;
    }
    
    std::string message(static_cast<const char*>(data), len);
    if (loop_->isInLoopThread()) {
        sendInLoop(message);
    } else {
        loop_->runInLoop([this, message]() { sendInLoop(message); });
    }
}

void Connection::send(const std::string& message) {
    send(message.data(), message.size());
}

void Connection::shutdown() {
    if (state_ == ConnectionState::CONNECTED) {
        state_ = ConnectionState::DISCONNECTING;
        loop_->runInLoop([this]() { shutdownInLoop(); });
    }
}

void Connection::forceClose() {
    if (state_ == ConnectionState::CONNECTED || state_ == ConnectionState::DISCONNECTING) {
        state_ = ConnectionState::DISCONNECTING;
        loop_->runInLoop([this]() { forceCloseInLoop(); });
    }
}

std::string Connection::getLocalAddress() const {
    return socket_->getLocalAddress();
}

std::string Connection::getPeerAddress() const {
    return socket_->getPeerAddress();
}

int Connection::getFd() const {
    return socket_->getFd();
}

void Connection::handleRead() {
    int saved_errno = 0;
    ssize_t n = input_buffer_->readFd(socket_->getFd(), &saved_errno);
    
    if (n > 0) {
        if (message_callback_) {
            message_callback_(shared_from_this(), input_buffer_.get());
        }
    } else if (n == 0) {
        handleClose();
    } else {
        errno = saved_errno;
        LOG_ERROR("Connection::handleRead error: " + std::string(strerror(errno)));
        handleError();
    }
}

void Connection::handleWrite() {
    if (handler_->getEvents() & static_cast<uint32_t>(EventType::WRITE)) {
        ssize_t n = socket_->send(output_buffer_->peek(), output_buffer_->readableBytes());
        if (n > 0) {
            output_buffer_->retrieve(n);
            if (output_buffer_->readableBytes() == 0) {
                handler_->disableWriting();
                loop_->updateHandler(handler_);
                
                if (write_complete_callback_) {
                    loop_->runInLoop([this]() {
                        write_complete_callback_(shared_from_this());
                    });
                }
                
                if (state_ == ConnectionState::DISCONNECTING) {
                    shutdownInLoop();
                }
            }
        } else {
            LOG_ERROR("Connection::handleWrite error");
        }
    }
}

void Connection::handleClose() {
    LOG_INFO("Connection closed: " + name_);
    state_ = ConnectionState::DISCONNECTED;
    handler_->disableAll();
    
    auto guard_this = shared_from_this();
    if (close_callback_) {
        close_callback_(guard_this);
    }
}

void Connection::handleError() {
    int err = 0;
    socklen_t err_len = sizeof(err);
    if (getsockopt(socket_->getFd(), SOL_SOCKET, SO_ERROR, &err, &err_len) < 0) {
        err = errno;
    }
    
    LOG_ERROR("Connection error: " + name_ + ", error: " + std::string(strerror(err)));
    
    if (error_callback_) {
        error_callback_(shared_from_this());
    }
}

void Connection::sendInLoop(const std::string& data) {
    if (state_ != ConnectionState::CONNECTED) {
        LOG_WARN("Connection disconnected, give up writing");
        return;
    }
    
    ssize_t nwrote = 0;
    size_t remaining = data.size();
    bool fault_error = false;
    
    // 如果输出缓冲区为空，尝试直接发送
    if (output_buffer_->readableBytes() == 0) {
        nwrote = socket_->send(data.data(), data.size());
        if (nwrote >= 0) {
            remaining = data.size() - nwrote;
            if (remaining == 0 && write_complete_callback_) {
                loop_->runInLoop([this]() {
                    write_complete_callback_(shared_from_this());
                });
            }
        } else {
            nwrote = 0;
            if (errno != EWOULDBLOCK) {
                LOG_ERROR("Connection::sendInLoop error");
                if (errno == EPIPE || errno == ECONNRESET) {
                    fault_error = true;
                }
            }
        }
    }
    
    // 如果还有数据未发送完，添加到输出缓冲区
    if (!fault_error && remaining > 0) {
        output_buffer_->append(data.data() + nwrote, remaining);
        if (!(handler_->getEvents() & static_cast<uint32_t>(EventType::WRITE))) {
            handler_->enableWriting();
            loop_->updateHandler(handler_);
        }
    }
}

void Connection::shutdownInLoop() {
    if (!(handler_->getEvents() & static_cast<uint32_t>(EventType::WRITE))) {
        // 没有数据要写，可以关闭写端
        if (::shutdown(socket_->getFd(), SHUT_WR) < 0) {
            LOG_ERROR("Connection::shutdownInLoop error: " + std::string(strerror(errno)));
        }
    }
}

void Connection::forceCloseInLoop() {
    if (state_ == ConnectionState::CONNECTED || state_ == ConnectionState::DISCONNECTING) {
        handleClose();
    }
}

} // namespace echo_server