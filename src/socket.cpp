#include "socket.h"
#include "logger.h"
#include <netinet/tcp.h>
#include <cstring>
#include <errno.h>

namespace echo_server {

Socket::Socket() : fd_(-1) {
}

Socket::Socket(int fd) : fd_(fd) {
}

Socket::~Socket() {
    close();
}

Socket::Socket(Socket&& other) noexcept : fd_(other.fd_) {
    other.fd_ = -1;
}

Socket& Socket::operator=(Socket&& other) noexcept {
    if (this != &other) {
        close();
        fd_ = other.fd_;
        other.fd_ = -1;
    }
    return *this;
}

bool Socket::create() {
    fd_ = ::socket(AF_INET, SOCK_STREAM, 0);
    if (fd_ < 0) {
        LOG_ERROR("Failed to create socket: " + std::string(strerror(errno)));
        return false;
    }
    
    LOG_DEBUG("Socket created with fd: " + std::to_string(fd_));
    return true;
}

bool Socket::bind(const std::string& address, int port) {
    if (!isValid()) {
        LOG_ERROR("Invalid socket for bind operation");
        return false;
    }
    
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(static_cast<uint16_t>(port));
    
    if (address.empty() || address == "0.0.0.0") {
        addr.sin_addr.s_addr = INADDR_ANY;
    } else {
        if (inet_pton(AF_INET, address.c_str(), &addr.sin_addr) <= 0) {
            LOG_ERROR("Invalid address: " + address);
            return false;
        }
    }
    
    if (::bind(fd_, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) {
        LOG_ERROR("Failed to bind to " + address + ":" + std::to_string(port) + 
                  ": " + std::string(strerror(errno)));
        return false;
    }
    
    LOG_INFO("Socket bound to " + address + ":" + std::to_string(port));
    return true;
}

bool Socket::listen(int backlog) {
    if (!isValid()) {
        LOG_ERROR("Invalid socket for listen operation");
        return false;
    }
    
    if (::listen(fd_, backlog) < 0) {
        LOG_ERROR("Failed to listen: " + std::string(strerror(errno)));
        return false;
    }
    
    LOG_INFO("Socket listening with backlog: " + std::to_string(backlog));
    return true;
}

std::unique_ptr<Socket> Socket::accept() {
    if (!isValid()) {
        LOG_ERROR("Invalid socket for accept operation");
        return nullptr;
    }
    
    sockaddr_in client_addr{};
    socklen_t addr_len = sizeof(client_addr);
    
    int client_fd = ::accept(fd_, reinterpret_cast<sockaddr*>(&client_addr), &addr_len);
    if (client_fd < 0) {
        if (errno != EAGAIN && errno != EWOULDBLOCK) {
            LOG_ERROR("Failed to accept connection: " + std::string(strerror(errno)));
        }
        return nullptr;
    }
    
    auto client_socket = std::make_unique<Socket>(client_fd);
    LOG_DEBUG("Accepted connection from " + client_socket->getPeerAddress());
    
    return client_socket;
}

bool Socket::connect(const std::string& address, int port) {
    if (!isValid()) {
        LOG_ERROR("Invalid socket for connect operation");
        return false;
    }
    
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(static_cast<uint16_t>(port));
    
    if (inet_pton(AF_INET, address.c_str(), &addr.sin_addr) <= 0) {
        LOG_ERROR("Invalid address: " + address);
        return false;
    }
    
    if (::connect(fd_, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) {
        LOG_ERROR("Failed to connect to " + address + ":" + std::to_string(port) + 
                  ": " + std::string(strerror(errno)));
        return false;
    }
    
    LOG_INFO("Connected to " + address + ":" + std::to_string(port));
    return true;
}

ssize_t Socket::send(const void* data, size_t size) {
    if (!isValid()) {
        LOG_ERROR("Invalid socket for send operation");
        return -1;
    }
    
    ssize_t bytes_sent = ::send(fd_, data, size, MSG_NOSIGNAL);
    if (bytes_sent < 0) {
        if (errno != EAGAIN && errno != EWOULDBLOCK) {
            LOG_ERROR("Failed to send data: " + std::string(strerror(errno)));
        }
    }
    
    return bytes_sent;
}

ssize_t Socket::receive(void* buffer, size_t size) {
    if (!isValid()) {
        LOG_ERROR("Invalid socket for receive operation");
        return -1;
    }
    
    ssize_t bytes_received = ::recv(fd_, buffer, size, 0);
    if (bytes_received < 0) {
        if (errno != EAGAIN && errno != EWOULDBLOCK) {
            LOG_ERROR("Failed to receive data: " + std::string(strerror(errno)));
        }
    }
    
    return bytes_received;
}

bool Socket::setNonBlocking(bool non_blocking) {
    if (!isValid()) {
        LOG_ERROR("Invalid socket for setNonBlocking operation");
        return false;
    }
    
    int flags = fcntl(fd_, F_GETFL, 0);
    if (flags < 0) {
        LOG_ERROR("Failed to get socket flags: " + std::string(strerror(errno)));
        return false;
    }
    
    if (non_blocking) {
        flags |= O_NONBLOCK;
    } else {
        flags &= ~O_NONBLOCK;
    }
    
    if (fcntl(fd_, F_SETFL, flags) < 0) {
        LOG_ERROR("Failed to set socket flags: " + std::string(strerror(errno)));
        return false;
    }
    
    return true;
}

bool Socket::setReuseAddress(bool reuse) {
    if (!isValid()) {
        LOG_ERROR("Invalid socket for setReuseAddress operation");
        return false;
    }
    
    int opt = reuse ? 1 : 0;
    if (setsockopt(fd_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        LOG_ERROR("Failed to set SO_REUSEADDR: " + std::string(strerror(errno)));
        return false;
    }
    
    return true;
}

bool Socket::setTcpNoDelay(bool no_delay) {
    if (!isValid()) {
        LOG_ERROR("Invalid socket for setTcpNoDelay operation");
        return false;
    }
    
    int opt = no_delay ? 1 : 0;
    if (setsockopt(fd_, IPPROTO_TCP, TCP_NODELAY, &opt, sizeof(opt)) < 0) {
        LOG_ERROR("Failed to set TCP_NODELAY: " + std::string(strerror(errno)));
        return false;
    }
    
    return true;
}

void Socket::close() {
    if (isValid()) {
        LOG_DEBUG("Closing socket fd: " + std::to_string(fd_));
        ::close(fd_);
        fd_ = -1;
    }
}

std::string Socket::getPeerAddress() const {
    if (!isValid()) {
        return "invalid:0";
    }
    
    sockaddr_in addr{};
    socklen_t addr_len = sizeof(addr);
    
    if (getpeername(fd_, reinterpret_cast<sockaddr*>(&addr), &addr_len) < 0) {
        return "unknown:0";
    }
    
    return formatAddress(addr);
}

std::string Socket::getLocalAddress() const {
    if (!isValid()) {
        return "invalid:0";
    }
    
    sockaddr_in addr{};
    socklen_t addr_len = sizeof(addr);
    
    if (getsockname(fd_, reinterpret_cast<sockaddr*>(&addr), &addr_len) < 0) {
        return "unknown:0";
    }
    
    return formatAddress(addr);
}

std::string Socket::formatAddress(const sockaddr_in& addr) const {
    char ip_str[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &addr.sin_addr, ip_str, INET_ADDRSTRLEN);
    return std::string(ip_str) + ":" + std::to_string(ntohs(addr.sin_port));
}

} // namespace echo_server