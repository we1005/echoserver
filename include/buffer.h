#ifndef ECHO_SERVER_BUFFER_H
#define ECHO_SERVER_BUFFER_H

#include <vector>
#include <string>
#include <algorithm>
#include <cstring>

namespace echo_server {

/**
 * @brief 高效的缓冲区类
 * 
 * 提供动态缓冲区功能，支持高效的数据读写操作
 * 采用vector作为底层存储，支持自动扩容
 */
class Buffer {
public:
    /**
     * @brief 构造函数
     * @param initial_size 初始大小
     */
    explicit Buffer(size_t initial_size = kInitialSize);

    /**
     * @brief 析构函数
     */
    ~Buffer() = default;

    // 支持拷贝和移动
    Buffer(const Buffer& other) = default;
    Buffer& operator=(const Buffer& other) = default;
    Buffer(Buffer&& other) noexcept = default;
    Buffer& operator=(Buffer&& other) noexcept = default;

    /**
     * @brief 获取可读数据大小
     * @return size_t 可读字节数
     */
    size_t readableBytes() const {
        return write_index_ - read_index_;
    }

    /**
     * @brief 获取可写空间大小
     * @return size_t 可写字节数
     */
    size_t writableBytes() const {
        return buffer_.size() - write_index_;
    }

    /**
     * @brief 获取已读取的字节数
     * @return size_t 已读取字节数
     */
    size_t prependableBytes() const {
        return read_index_;
    }

    /**
     * @brief 获取可读数据的起始指针
     * @return const char* 数据指针
     */
    const char* peek() const {
        return begin() + read_index_;
    }

    /**
     * @brief 查找CRLF（\r\n）
     * @return const char* CRLF位置，未找到返回nullptr
     */
    const char* findCRLF() const;

    /**
     * @brief 从指定位置查找CRLF
     * @param start 起始位置
     * @return const char* CRLF位置，未找到返回nullptr
     */
    const char* findCRLF(const char* start) const;

    /**
     * @brief 查找换行符（\n）
     * @return const char* 换行符位置，未找到返回nullptr
     */
    const char* findEOL() const;

    /**
     * @brief 从指定位置查找换行符
     * @param start 起始位置
     * @return const char* 换行符位置，未找到返回nullptr
     */
    const char* findEOL(const char* start) const;

    /**
     * @brief 读取指定长度的数据
     * @param len 读取长度
     */
    void retrieve(size_t len);

    /**
     * @brief 读取到指定位置
     * @param end 结束位置
     */
    void retrieveUntil(const char* end);

    /**
     * @brief 读取所有数据
     */
    void retrieveAll() {
        read_index_ = kCheapPrepend;
        write_index_ = kCheapPrepend;
    }

    /**
     * @brief 读取所有数据并返回字符串
     * @return std::string 所有数据
     */
    std::string retrieveAllAsString() {
        return retrieveAsString(readableBytes());
    }

    /**
     * @brief 读取指定长度数据并返回字符串
     * @param len 读取长度
     * @return std::string 读取的数据
     */
    std::string retrieveAsString(size_t len);

    /**
     * @brief 追加数据
     * @param data 数据指针
     * @param len 数据长度
     */
    void append(const char* data, size_t len);

    /**
     * @brief 追加字符串
     * @param str 字符串
     */
    void append(const std::string& str) {
        append(str.data(), str.size());
    }

    /**
     * @brief 追加数据
     * @param data 数据
     */
    void append(const void* data, size_t len) {
        append(static_cast<const char*>(data), len);
    }

    /**
     * @brief 确保可写空间足够
     * @param len 需要的空间大小
     */
    void ensureWritableBytes(size_t len);

    /**
     * @brief 获取可写区域的起始指针
     * @return char* 可写区域指针
     */
    char* beginWrite() {
        return begin() + write_index_;
    }

    /**
     * @brief 获取可写区域的起始指针（常量版本）
     * @return const char* 可写区域指针
     */
    const char* beginWrite() const {
        return begin() + write_index_;
    }

    /**
     * @brief 标记已写入数据
     * @param len 写入的数据长度
     */
    void hasWritten(size_t len) {
        write_index_ += len;
    }

    /**
     * @brief 撤销写入
     * @param len 撤销的长度
     */
    void unwrite(size_t len) {
        write_index_ -= len;
    }

    /**
     * @brief 前置数据
     * @param data 数据指针
     * @param len 数据长度
     */
    void prepend(const void* data, size_t len);

    /**
     * @brief 收缩缓冲区
     * @param reserve 保留空间
     */
    void shrink(size_t reserve);

    /**
     * @brief 获取内部缓冲区大小
     * @return size_t 缓冲区大小
     */
    size_t internalCapacity() const {
        return buffer_.capacity();
    }

    /**
     * @brief 从文件描述符读取数据
     * @param fd 文件描述符
     * @param saved_errno 错误码输出
     * @return ssize_t 读取的字节数
     */
    ssize_t readFd(int fd, int* saved_errno);

private:
    /**
     * @brief 获取缓冲区起始指针
     * @return char* 起始指针
     */
    char* begin() {
        return &*buffer_.begin();
    }

    /**
     * @brief 获取缓冲区起始指针（常量版本）
     * @return const char* 起始指针
     */
    const char* begin() const {
        return &*buffer_.begin();
    }

    /**
     * @brief 扩容缓冲区
     * @param len 需要的额外空间
     */
    void makeSpace(size_t len);

    std::vector<char> buffer_;  ///< 底层缓冲区
    size_t read_index_;         ///< 读取位置
    size_t write_index_;        ///< 写入位置

    static const char kCRLF[];  ///< CRLF常量
    static const size_t kCheapPrepend = 8;      ///< 预留前置空间
    static const size_t kInitialSize = 1024;    ///< 初始大小
};

} // namespace echo_server

#endif // ECHO_SERVER_BUFFER_H