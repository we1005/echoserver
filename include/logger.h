#ifndef ECHO_SERVER_LOGGER_H
#define ECHO_SERVER_LOGGER_H

#include <string>
#include <fstream>
#include <mutex>
#include <memory>
#include <sstream>
#include <chrono>
#include <iomanip>

namespace echo_server {

/**
 * @brief 日志级别枚举
 */
enum class LogLevel {
    DEBUG = 0,
    INFO = 1,
    WARN = 2,
    ERROR = 3,
    FATAL = 4
};

/**
 * @brief 线程安全的日志系统
 * 
 * 提供多级别日志记录功能，支持控制台和文件输出
 * 采用单例模式，确保全局唯一性
 */
class Logger {
public:
    /**
     * @brief 获取Logger单例实例
     * @return Logger& 单例引用
     */
    static Logger& getInstance();

    /**
     * @brief 设置日志级别
     * @param level 日志级别
     */
    void setLogLevel(LogLevel level);

    /**
     * @brief 设置日志文件路径
     * @param filename 文件路径
     * @return bool 设置是否成功
     */
    bool setLogFile(const std::string& filename);

    /**
     * @brief 记录日志
     * @param level 日志级别
     * @param message 日志消息
     */
    void log(LogLevel level, const std::string& message);

    /**
     * @brief 记录DEBUG级别日志
     * @param message 日志消息
     */
    void debug(const std::string& message);

    /**
     * @brief 记录INFO级别日志
     * @param message 日志消息
     */
    void info(const std::string& message);

    /**
     * @brief 记录WARN级别日志
     * @param message 日志消息
     */
    void warn(const std::string& message);

    /**
     * @brief 记录ERROR级别日志
     * @param message 日志消息
     */
    void error(const std::string& message);

    /**
     * @brief 记录FATAL级别日志
     * @param message 日志消息
     */
    void fatal(const std::string& message);

private:
    Logger() = default;
    ~Logger();
    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;

    /**
     * @brief 获取当前时间戳字符串
     * @return std::string 格式化的时间戳
     */
    std::string getCurrentTimestamp() const;

    /**
     * @brief 获取日志级别字符串
     * @param level 日志级别
     * @return std::string 级别字符串
     */
    std::string getLevelString(LogLevel level) const;

    LogLevel current_level_ = LogLevel::INFO;  ///< 当前日志级别
    std::unique_ptr<std::ofstream> log_file_;  ///< 日志文件流
    mutable std::mutex mutex_;                 ///< 线程安全互斥锁
};

// 便捷宏定义
#define LOG_DEBUG(msg) echo_server::Logger::getInstance().debug(msg)
#define LOG_INFO(msg) echo_server::Logger::getInstance().info(msg)
#define LOG_WARN(msg) echo_server::Logger::getInstance().warn(msg)
#define LOG_ERROR(msg) echo_server::Logger::getInstance().error(msg)
#define LOG_FATAL(msg) echo_server::Logger::getInstance().fatal(msg)

} // namespace echo_server

#endif // ECHO_SERVER_LOGGER_H