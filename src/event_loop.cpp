#include "event_loop.h"
#include "logger.h"
#include <sys/eventfd.h>
#include <unistd.h>
#include <cstring>
#include <errno.h>
#include <thread>

namespace echo_server {

// EventHandler implementation
EventHandler::EventHandler(int fd) : fd_(fd), events_(0) {
}

void EventHandler::setReadCallback(const EventCallback& callback) {
    read_callback_ = callback;
}

void EventHandler::setWriteCallback(const EventCallback& callback) {
    write_callback_ = callback;
}

void EventHandler::setErrorCallback(const EventCallback& callback) {
    error_callback_ = callback;
}

void EventHandler::setCloseCallback(const EventCallback& callback) {
    close_callback_ = callback;
}

void EventHandler::handleEvents(uint32_t events) {
    if ((events & EPOLLHUP) && !(events & EPOLLIN)) {
        if (close_callback_) {
            close_callback_();
        }
        return;
    }
    
    if (events & EPOLLERR) {
        if (error_callback_) {
            error_callback_();
        }
    }
    
    if (events & (EPOLLIN | EPOLLPRI | EPOLLRDHUP)) {
        if (read_callback_) {
            read_callback_();
        }
    }
    
    if (events & EPOLLOUT) {
        if (write_callback_) {
            write_callback_();
        }
    }
}

// EventLoop implementation
EventLoop::EventLoop()
    : epoll_fd_(::epoll_create1(EPOLL_CLOEXEC)),
      wakeup_fd_(::eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC)),
      running_(false),
      quit_(false),
      active_events_(kMaxEvents),
      loop_thread_id_(std::this_thread::get_id()) {
    
    if (epoll_fd_ < 0) {
        LOG_FATAL("Failed to create epoll fd: " + std::string(strerror(errno)));
    }
    
    if (wakeup_fd_ < 0) {
        LOG_FATAL("Failed to create eventfd: " + std::string(strerror(errno)));
    }
    
    // 添加唤醒文件描述符到epoll
    auto wakeup_handler = std::make_shared<EventHandler>(wakeup_fd_);
    wakeup_handler->setReadCallback([this]() { handleWakeup(); });
    wakeup_handler->enableReading();
    addHandler(wakeup_handler);
    
    LOG_DEBUG("EventLoop created with epoll_fd: " + std::to_string(epoll_fd_));
}

EventLoop::~EventLoop() {
    if (epoll_fd_ >= 0) {
        ::close(epoll_fd_);
    }
    if (wakeup_fd_ >= 0) {
        ::close(wakeup_fd_);
    }
}

void EventLoop::run() {
    running_ = true;
    quit_ = false;
    loop_thread_id_ = std::this_thread::get_id();
    
    LOG_INFO("EventLoop started");
    
    while (!quit_) {
        handleEvents(10000); // 10秒超时
        doPendingTasks();
    }
    
    running_ = false;
    LOG_INFO("EventLoop stopped");
}

void EventLoop::stop() {
    quit_ = true;
    if (!isInLoopThread()) {
        wakeup();
    }
}

void EventLoop::addHandler(std::shared_ptr<EventHandler> handler) {
    int fd = handler->getFd();
    
    if (isInLoopThread()) {
        epoll_event event;
        memset(&event, 0, sizeof(event));
        event.events = handler->getEvents();
        event.data.fd = fd;
        
        if (epoll_ctl(epoll_fd_, EPOLL_CTL_ADD, fd, &event) < 0) {
            LOG_ERROR("Failed to add handler to epoll: " + std::string(strerror(errno)));
            return;
        }
        
        handlers_[fd] = handler;
        LOG_DEBUG("Handler added for fd: " + std::to_string(fd));
    } else {
        runInLoop([this, handler]() { addHandler(handler); });
    }
}

void EventLoop::updateHandler(std::shared_ptr<EventHandler> handler) {
    int fd = handler->getFd();
    
    if (isInLoopThread()) {
        epoll_event event;
        memset(&event, 0, sizeof(event));
        event.events = handler->getEvents();
        event.data.fd = fd;
        
        if (epoll_ctl(epoll_fd_, EPOLL_CTL_MOD, fd, &event) < 0) {
            LOG_ERROR("Failed to update handler in epoll: " + std::string(strerror(errno)));
            return;
        }
        
        LOG_DEBUG("Handler updated for fd: " + std::to_string(fd));
    } else {
        runInLoop([this, handler]() { updateHandler(handler); });
    }
}

void EventLoop::removeHandler(std::shared_ptr<EventHandler> handler) {
    int fd = handler->getFd();
    
    if (isInLoopThread()) {
        if (epoll_ctl(epoll_fd_, EPOLL_CTL_DEL, fd, nullptr) < 0) {
            LOG_ERROR("Failed to remove handler from epoll: " + std::string(strerror(errno)));
        }
        
        handlers_.erase(fd);
        LOG_DEBUG("Handler removed for fd: " + std::to_string(fd));
    } else {
        runInLoop([this, handler]() { removeHandler(handler); });
    }
}

void EventLoop::runInLoop(const std::function<void()>& task) {
    if (isInLoopThread()) {
        task();
    } else {
        {
            std::lock_guard<std::mutex> lock(tasks_mutex_);
            pending_tasks_.push_back(task);
        }
        wakeup();
    }
}

bool EventLoop::isInLoopThread() const {
    return loop_thread_id_ == std::this_thread::get_id();
}

void EventLoop::handleEvents(int timeout_ms) {
    int num_events = epoll_wait(epoll_fd_, &*active_events_.begin(),
                               static_cast<int>(active_events_.size()), timeout_ms);
    
    if (num_events > 0) {
        LOG_DEBUG("EventLoop got " + std::to_string(num_events) + " events");
        
        for (int i = 0; i < num_events; ++i) {
            int fd = active_events_[i].data.fd;
            uint32_t events = active_events_[i].events;
            
            auto it = handlers_.find(fd);
            if (it != handlers_.end()) {
                it->second->handleEvents(events);
            }
        }
        
        if (static_cast<size_t>(num_events) == active_events_.size()) {
            active_events_.resize(active_events_.size() * 2);
        }
    } else if (num_events == 0) {
        // 超时，正常情况
    } else {
        if (errno != EINTR) {
            LOG_ERROR("epoll_wait error: " + std::string(strerror(errno)));
        }
    }
}

void EventLoop::doPendingTasks() {
    std::vector<std::function<void()>> tasks;
    {
        std::lock_guard<std::mutex> lock(tasks_mutex_);
        tasks.swap(pending_tasks_);
    }
    
    for (const auto& task : tasks) {
        task();
    }
}

void EventLoop::wakeup() {
    uint64_t one = 1;
    ssize_t n = ::write(wakeup_fd_, &one, sizeof(one));
    if (n != sizeof(one)) {
        LOG_ERROR("EventLoop::wakeup() writes " + std::to_string(n) + " bytes instead of 8");
    }
}

void EventLoop::handleWakeup() {
    uint64_t one = 1;
    ssize_t n = ::read(wakeup_fd_, &one, sizeof(one));
    if (n != sizeof(one)) {
        LOG_ERROR("EventLoop::handleWakeup() reads " + std::to_string(n) + " bytes instead of 8");
    }
}

} // namespace echo_server