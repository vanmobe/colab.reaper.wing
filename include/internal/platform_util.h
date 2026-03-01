/*
 * Platform Utilities
 * Abstracts platform-specific differences across Windows, macOS, Linux
 */

#ifndef PLATFORM_UTIL_H
#define PLATFORM_UTIL_H

#include <string>
#include <cstdio>
#include <cstdarg>

namespace WingConnector {
namespace Platform {

// Platform-agnostic string formatting
// Returns 0 on success, -1 on formatting error
int StringFormat(char* buffer, size_t buffer_size, const char* format, ...);

// Get the platform-specific plugin configuration directory
// Returns path like ~/Library/Application Support/REAPER/UserPlugins on macOS
//              or %APPDATA%/REAPER/UserPlugins on Windows
// Falls back to ~/.wingconnector/ if primary location unavailable
std::string GetPluginConfigDir();

// Get config file path with fallbacks
// Checks: primary plugin dir -> secondary plugin dir -> user home dir
std::string GetConfigFilePath();

// Platform detection helpers
inline constexpr bool IsWindows() {
#ifdef _WIN32
    return true;
#else
    return false;
#endif
}

inline constexpr bool IsMacOS() {
#ifdef __APPLE__
    return true;
#else
    return false;
#endif
}

inline constexpr bool IsLinux() {
#ifdef __linux__
    return true;
#else
    return false;
#endif
}

} // namespace Platform
} // namespace WingConnector

#endif // PLATFORM_UTIL_H
