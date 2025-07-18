#ifndef ECHO_SERVER_SOCKET_H
#define ECHO_SERVER_SOCKET_H

#include <string>
#include <memory>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>

namespace echo_server {

/**
 * @brief 套接字封装类
 * 
 * 提供TCP套接字的基本操作封装，包括创建、绑定、监听、连接等
 * 支持阻塞和非阻塞模式
 */
class Socket {
public:
    /**
     * @brief 构造函数
     */
    Socket();

    /**
     * @brief 带文件描述符的构造函数
     * @param fd 套接字文件描述符
     */
    explicit Socket(int fd);

    /**
     * @brief 析构函数
     */
    ~Socket();

    // 禁用拷贝构造和赋值
    Socket(const Socket&) = delete;
    Socket& operator=(const Socket&) = delete;

    // 支持移动语义
    Socket(Socket&& other) noexcept;
    Socket& operator=(Socket&& other) noexcept;

    /**
     * @brief 创建套接字
     * @return bool 创建是否成功
     */
    bool create();

    /**
     * @brief 绑定地址和端口
     * @param address IP地址
     * @param port 端口号
     * @return bool 绑定是否成功
     */
    bool bind(const std::string& address, int port);

    /**
     * @brief 开始监听
     * @param backlog 监听队列长度
     * @return bool 监听是否成功
     */
    bool listen(int backlog = SOMAXCONN);

    /**
     * @brief 接受连接
     * @return std::unique_ptr<Socket> 新连接的套接字
     */
    std::unique_ptr<Socket> accept();

    /**
     * @brief 连接到服务器
     * @param address 服务器地址
     * @param port 服务器端口
     * @return bool 连接是否成功
     */
    bool connect(const std::string& address, int port);

    /**
     * @brief 发送数据
     * @param data 数据指针
     * @param size 数据大小
     * @return ssize_t 实际发送的字节数，-1表示错误
     */
    ssize_t send(const void* data, size_t size);

    /**
     * @brief 接收数据
     * @param buffer 接收缓冲区
     * @param size 缓冲区大小
     * @return ssize_t 实际接收的字节数，-1表示错误，0表示连接关闭
     */
    ssize_t receive(void* buffer, size_t size);

    /**
     * @brief 设置非阻塞模式
     * @param non_blocking 是否非阻塞
     * @return bool 设置是否成功
     */
    bool setNonBlocking(bool non_blocking = true);

    /**
     * @brief 设置地址重用
     * @param reuse 是否重用
     * @return bool 设置是否成功
     */
    bool setReuseAddress(bool reuse = true);

    /**
     * @brief 设置TCP_NODELAY选项
     * @param no_delay 是否禁用Nagle算法
     * @return bool 设置是否成功
     */
    bool setTcpNoDelay(bool no_delay = true);

    /**
     * @brief 关闭套接字
     */
    void close();

    /**
     * @brief 获取套接字文件描述符
     * @return int 文件描述符
     */
    int getFd() const { return fd_; }

    /**
     * @brief 检查套接字是否有效
     * @return bool 是否有效
     */
    bool isValid() const { return fd_ != -1; }

    /**
     * @brief 获取对端地址信息
     * @return std::string 地址:端口格式的字符串
     */
    std::string getPeerAddress() const;

    /**
     * @brief 获取本地地址信息
     * @return std::string 地址:端口格式的字符串
     */
    std::string getLocalAddress() const;

private:
    int fd_;  ///< 套接字文件描述符

    /**
     * @brief 格式化地址信息
     * @param addr 地址结构
     * @return std::string 格式化的地址字符串
     */
    std::string formatAddress(const sockaddr_in& addr) const;
};

} // namespace echo_server

#endif // ECHO_SERVER_SOCKET_H