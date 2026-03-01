/*
 * Unified Logging System for Wing Connector
 */

#include "internal/logger.h"
#include "internal/platform_util.h"
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <mutex>

namespace WingConnector {

bool Logger::console_enabled_ = true;
LogLevel Logger::min_level_ = LogLevel::DEBUG;

static std::mutex g_log_mutex;

void Logger::Initialize(bool enable_console) {
    std::lock_guard<std::mutex> lock(g_log_mutex);
    console_enabled_ = enable_console;
}

void Logger::SetLogLevel(LogLevel level) {
    std::lock_guard<std::mutex> lock(g_log_mutex);
    min_level_ = level;
}

const char* Logger::LevelToString(LogLevel level) {
    switch (level) {
        case LogLevel::DEBUG: return "DEBUG";
        case LogLevel::INFO:  return "INFO";
        case LogLevel::WARN:  return "WARN";
        case LogLevel::ERROR: return "ERROR";
        default: return "UNKNOWN";
    }
}

void Logger::LogInternal(LogLevel level, const char* fmt, va_list args) {
    if (level < min_level_) {
        return;
    }
    
    std::lock_guard<std::mutex> lock(g_log_mutex);
    
    if (!console_enabled_) {
        return;
    }
    
    const char* level_str = LevelToString(level);
    
    // Get timestamp
    time_t now = time(nullptr);
    struct tm* timeinfo = localtime(&now);
    char timestamp[32];
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", timeinfo);
    
    // Format the message
    char buffer[2048];
    Platform::StringFormat(buffer, sizeof(buffer), "[%s] [%s] ", timestamp, level_str);
    size_t prefix_len = strlen(buffer);
    
    int written = vsnprintf(buffer + prefix_len, sizeof(buffer) - prefix_len, fmt, args);
    if (written < 0) {
        return;  // Formatting error
    }
    
    // Output to stderr only
    fprintf(stderr, "%s\n", buffer);
    fflush(stderr);
}

void Logger::Debug(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    LogInternal(LogLevel::DEBUG, fmt, args);
    va_end(args);
}

void Logger::Info(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    LogInternal(LogLevel::INFO, fmt, args);
    va_end(args);
}

void Logger::Warn(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    LogInternal(LogLevel::WARN, fmt, args);
    va_end(args);
}

void Logger::Error(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    LogInternal(LogLevel::ERROR, fmt, args);
    va_end(args);
}

void Logger::Log(LogLevel level, const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    LogInternal(level, fmt, args);
    va_end(args);
}

void Logger::LogException(const char* context, const std::exception& e) {
    Error("%s: %s", context, e.what());
}

void Logger::Shutdown() {
    // Could perform cleanup here if needed in the future
}

} // namespace WingConnector
