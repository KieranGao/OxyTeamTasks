#ifndef LOGGER_H
#define LOGGER_H

#include <fstream>
#include <mutex>
#include <queue>
#include <string>
#include <thread>
#include <atomic>
#include <chrono>
#include <ctime>
#include <sstream>
#include <vector>
#include <functional>
#include "Singleton.h"

enum class LogLevel { DEBUG = 0, INFO = 1, WARN = 2, ERROR = 3 };

// C++20 std::format-style {}
template<typename... Args>
std::string format_string(const std::string& fmt, Args&&... args) {
    std::ostringstream result;
    size_t pos = 0;
    size_t placeholder = 0;
    auto replace_one = [&](const auto& val) {
        placeholder = fmt.find("{}", pos);
        if (placeholder != std::string::npos) {
            result << fmt.substr(pos, placeholder - pos);
            result << val;
            pos = placeholder + 2;
        }
    };
    (replace_one(std::forward<Args>(args)), ...);
    result << fmt.substr(pos);
    return result.str();
}

struct LogEntry {
    LogLevel level;
    std::string message;
    int64_t timestamp;
};

using RemoteFlushCallback = std::function<void(const std::vector<LogEntry>&)>;

class Logger : public Singleton<Logger> {
    friend class Singleton<Logger>;
public:
    template<typename... Args>
    void debug(const std::string& fmt, Args&&... args) { log(LogLevel::DEBUG, format_string(fmt, std::forward<Args>(args)...)); }
    template<typename... Args>
    void info(const std::string& fmt, Args&&... args)  { log(LogLevel::INFO,  format_string(fmt, std::forward<Args>(args)...)); }
    template<typename... Args>
    void warn(const std::string& fmt, Args&&... args)  { log(LogLevel::WARN,  format_string(fmt, std::forward<Args>(args)...)); }
    template<typename... Args>
    void error(const std::string& fmt, Args&&... args) { log(LogLevel::ERROR, format_string(fmt, std::forward<Args>(args)...)); }

    void log(LogLevel level, const std::string& msg);
    void setRemoteFlushCallback(RemoteFlushCallback cb);
    std::string getServiceName() const { return service_name_; }
    static std::string levelToString(LogLevel level);
    void stop();
    ~Logger();

private:
    Logger();
    void writeToFile(const std::string& line);
    void rotateIfNeeded();
    void flushLoop();
    static std::string timestamp();

    LogLevel min_level_;
    std::string log_dir_;
    std::string service_name_;
    std::ofstream file_;
    std::string current_date_;
    std::mutex mtx_;

    std::vector<LogEntry> buffer_;
    std::mutex buf_mtx_;
    RemoteFlushCallback remote_flush_;
    std::thread flush_thread_;
    std::atomic<bool> running_{false};
    int flush_interval_;
    int max_buffer_;
    bool skip_remote_;
};

#define LOG_DEBUG(fmt, ...) Logger::getInstance().debug(fmt, ##__VA_ARGS__)
#define LOG_INFO(fmt, ...)  Logger::getInstance().info(fmt,  ##__VA_ARGS__)
#define LOG_WARN(fmt, ...)  Logger::getInstance().warn(fmt,  ##__VA_ARGS__)
#define LOG_ERROR(fmt, ...) Logger::getInstance().error(fmt, ##__VA_ARGS__)

#endif
