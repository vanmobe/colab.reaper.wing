/*
 * String Formatting Utilities
 * Provides convenient string formatting similar to fmt library
 */

#ifndef STRING_FORMAT_H
#define STRING_FORMAT_H

#include <string>
#include <string_view>
#include <vector>

namespace WingConnector {
namespace Fmt {

// Simple string builder for convenient formatting
// Usage: StringBuilder() << "Value: " << 42 << " Name: " << "test"
class StringBuilder {
public:
    template<typename T>
    StringBuilder& operator<<(const T& value);
    
    std::string str() const;
    operator std::string() const;
    
private:
    // Declared in .cpp to avoid sstream include
    class Impl;
    std::unique_ptr<Impl> impl_;
    
    StringBuilder();
    ~StringBuilder();
    StringBuilder(const StringBuilder&) = delete;
    StringBuilder& operator=(const StringBuilder&) = delete;
};

// Format function using {} placeholders
// Internal helper class for building formatted strings
class Formatter {
public:
    Formatter() = default;
    
    // Add a value to the format string
    template<typename T>
    Formatter& Arg(const T& value);
    
    // Internal: format the string with added args (using string_view for efficiency)
    std::string Format(std::string_view format_str);
    
private:
    std::vector<std::string> args_;
};

// Recursive template to collect variable arguments
template<typename T>
inline void CollectArgs(Formatter& fmt, const T& arg) {
    fmt.Arg(arg);
}

template<typename T, typename... Rest>
inline void CollectArgs(Formatter& fmt, const T& arg, const Rest&... rest) {
    fmt.Arg(arg);
    CollectArgs(fmt, rest...);
}

// Variadic formatting function - for simple {} placeholder replacement
// Usage: Format("Error: {}, Code: {}", error_msg, 404)
template<typename... Args>
inline std::string Format(std::string_view format_str, const Args&... args) {
    Formatter fmt;
    CollectArgs(fmt, args...);
    return fmt.Format(format_str);
}

// String joining utility
// Join({"one", "two", "three"}, " or ") -> "one or two or three"
std::string Join(const std::vector<std::string>& items, std::string_view separator = ", ");

} // namespace Fmt
} // namespace WingConnector

#endif // STRING_FORMAT_H
