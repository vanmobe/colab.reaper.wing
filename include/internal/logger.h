#ifndef WING_LOGGER_H
#define WING_LOGGER_H

#include <string>
#include <cstdarg>

namespace WingConnector {

enum class LogLevel {
    DEBUG = 0,
    INFO = 1,
    WARN = 2,
    ERROR = 3
};

class Logger {
public:
    // Initialize logger (call once at startup)
    static void Initialize(bool enable_console = true);
    
    // Set minimum log level
    static void SetLogLevel(LogLevel level);
    
    // Log messages with printf-style formatting
    static void Debug(const char* fmt, ...);
    static void Info(const char* fmt, ...);
    static void Warn(const char* fmt, ...);
    static void Error(const char* fmt, ...);
    
    // Log messages with explicit level
    static void Log(LogLevel level, const char* fmt, ...);
    
    // Log C++ exceptions safely
    static void LogException(const char* context, const std::exception& e);
    
    // Shutdown logger gracefully
    static void Shutdown();
    
private:
    Logger() = default;
    static void LogInternal(LogLevel level, const char* fmt, va_list args);
    static const char* LevelToString(LogLevel level);
    
    static bool console_enabled_;
    static LogLevel min_level_;
};

} // namespace WingConnector

#endif // WING_LOGGER_H
