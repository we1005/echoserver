#include "buffer.h"
#include "logger.h"
#include <sys/uio.h>
#include <unistd.h>
#include <errno.h>
#include <cstring>

namespace echo_server {

const char Buffer::kCRLF[] = "\r\n";

Buffer::Buffer(size_t initial_size)
    : buffer_(kCheapPrepend + initial_size),
      read_index_(kCheapPrepend),
      write_index_(kCheapPrepend) {
}

const char* Buffer::findCRLF() const {
    const char* crlf = std::search(peek(), beginWrite(), kCRLF, kCRLF + 2);
    return crlf == beginWrite() ? nullptr : crlf;
}

const char* Buffer::findCRLF(const char* start) const {
    if (start < peek() || start > beginWrite()) {
        return nullptr;
    }
    const char* crlf = std::search(start, beginWrite(), kCRLF, kCRLF + 2);
    return crlf == beginWrite() ? nullptr : crlf;
}

const char* Buffer::findEOL() const {
    const void* eol = memchr(peek(), '\n', readableBytes());
    return static_cast<const char*>(eol);
}

const char* Buffer::findEOL(const char* start) const {
    if (start < peek() || start > beginWrite()) {
        return nullptr;
    }
    const void* eol = memchr(start, '\n', beginWrite() - start);
    return static_cast<const char*>(eol);
}

void Buffer::retrieve(size_t len) {
    if (len < readableBytes()) {
        read_index_ += len;
    } else {
        retrieveAll();
    }
}

void Buffer::retrieveUntil(const char* end) {
    if (end >= peek() && end <= beginWrite()) {
        retrieve(end - peek());
    }
}

std::string Buffer::retrieveAsString(size_t len) {
    if (len > readableBytes()) {
        len = readableBytes();
    }
    std::string result(peek(), len);
    retrieve(len);
    return result;
}

void Buffer::append(const char* data, size_t len) {
    ensureWritableBytes(len);
    std::copy(data, data + len, beginWrite());
    hasWritten(len);
}

void Buffer::ensureWritableBytes(size_t len) {
    if (writableBytes() < len) {
        makeSpace(len);
    }
}

void Buffer::prepend(const void* data, size_t len) {
    if (len > prependableBytes()) {
        LOG_ERROR("Buffer prepend: not enough prepend space");
        return;
    }
    read_index_ -= len;
    const char* d = static_cast<const char*>(data);
    std::copy(d, d + len, begin() + read_index_);
}

void Buffer::shrink(size_t reserve) {
    Buffer other;
    other.ensureWritableBytes(readableBytes() + reserve);
    other.append(peek(), readableBytes());
    buffer_.swap(other.buffer_);
    read_index_ = other.read_index_;
    write_index_ = other.write_index_;
}

void Buffer::makeSpace(size_t len) {
    if (writableBytes() + prependableBytes() < len + kCheapPrepend) {
        // 需要扩容
        buffer_.resize(write_index_ + len);
    } else {
        // 移动数据到前面
        size_t readable = readableBytes();
        std::copy(begin() + read_index_,
                  begin() + write_index_,
                  begin() + kCheapPrepend);
        read_index_ = kCheapPrepend;
        write_index_ = read_index_ + readable;
    }
}

ssize_t Buffer::readFd(int fd, int* saved_errno) {
    // 使用栈上的额外缓冲区，避免频繁的内存分配
    char extrabuf[65536];
    struct iovec vec[2];
    const size_t writable = writableBytes();
    
    vec[0].iov_base = begin() + write_index_;
    vec[0].iov_len = writable;
    vec[1].iov_base = extrabuf;
    vec[1].iov_len = sizeof(extrabuf);
    
    // 当缓冲区足够大时，不使用额外缓冲区
    const int iovcnt = (writable < sizeof(extrabuf)) ? 2 : 1;
    const ssize_t n = ::readv(fd, vec, iovcnt);
    
    if (n < 0) {
        *saved_errno = errno;
    } else if (static_cast<size_t>(n) <= writable) {
        write_index_ += n;
    } else {
        write_index_ = buffer_.size();
        append(extrabuf, n - writable);
    }
    
    return n;
}

} // namespace echo_server