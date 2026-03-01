/*
 * Platform Utilities Implementation
 */

#include "internal/platform_util.h"
#include <cstdlib>
#include <fstream>

#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#endif

namespace WingConnector {
namespace Platform {

int StringFormat(char* buffer, size_t buffer_size, const char* format, ...) {
    va_list args;
    va_start(args, format);
    
#ifdef _WIN32
    // Windows: use _vsnprintf_s for safe formatting
    int result = _vsnprintf_s(buffer, buffer_size, buffer_size - 1, format, args);
#else
    // POSIX: use vsnprintf
    int result = vsnprintf(buffer, buffer_size, format, args);
#endif
    
    va_end(args);
    return result;
}

std::string GetPluginConfigDir() {
    const char* home = std::getenv("HOME");
    if (!home) {
        home = std::getenv("USERPROFILE"); // Fallback for Windows
    }
    
    if (!home) {
        return "."; // Last resort
    }
    
#ifdef __APPLE__
    // macOS: ~/Library/Application Support/REAPER/UserPlugins
    return std::string(home) + "/Library/Application Support/REAPER/UserPlugins";
#elif defined(_WIN32)
    // Windows: %USERPROFILE%/AppData/Roaming/REAPER/UserPlugins
    std::string config_path = std::string(home) + "/AppData/Roaming/REAPER/UserPlugins";
    
    // Also check APPDATA env var
    const char* appdata = std::getenv("APPDATA");
    if (appdata) {
        // Use APPDATA if available for more reliability
        return std::string(appdata) + "/REAPER/UserPlugins";
    }
    
    return config_path;
#elif defined(__linux__)
    // Linux: ~/.config/REAPER/UserPlugins
    return std::string(home) + "/.config/REAPER/UserPlugins";
#else
    // Fallback for unknown platforms
    return std::string(home) + "/.reaper_plugins";
#endif
}

std::string GetConfigFilePath() {
    std::string plugin_dir = GetPluginConfigDir();
    std::string config_path = plugin_dir + "/config.json";
    
    // Check if config exists in primary location
    std::ifstream test(config_path);
    if (test.good()) {
        test.close();
        return config_path;
    }
    test.close();
    
    // Fall back to user home directory
    const char* home = std::getenv("HOME");
    if (!home) {
        home = std::getenv("USERPROFILE");
    }
    
    if (home) {
        std::string home_config = std::string(home) + "/.wingconnector/config.json";
        
        std::ifstream home_test(home_config);
        if (home_test.good()) {
            home_test.close();
            return home_config;
        }
        home_test.close();
        
        // Return the user home config path as default (may not exist yet)
        return home_config;
    }
    
    return "config.json"; // Last resort fallback
}

} // namespace Platform
} // namespace WingConnector
