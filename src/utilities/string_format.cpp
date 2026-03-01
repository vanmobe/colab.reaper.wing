/*
 * String Formatting Utilities Implementation
 */

#include "internal/string_format.h"
#include <sstream>
#include <memory>

namespace WingConnector {
namespace Fmt {

// Template Arg implementation
template<typename T>
inline void ArgImpl(Formatter& fmt, const T& value) {
    std::ostringstream oss;
    oss << value;
    fmt.Arg<std::string>(oss.str());
}

// Specialize for string types to avoid double-allocation
template<>
inline void ArgImpl(Formatter& fmt, const std::string& value) {
    fmt.Arg<std::string>(value);
}

std::string Formatter::Format(std::string_view format_str) {
    if (format_str.empty()) return "";
    
    std::string result;
    size_t arg_idx = 0;
    
    // Replace {} placeholders with arguments - no copy needed!
    size_t pos = 0;
    while (pos < format_str.size()) {
        size_t brace_pos = format_str.find("{}", pos);
        if (brace_pos == std::string_view::npos) {
            // No more placeholders, append the rest
            result.append(format_str.substr(pos));
            break;
        }
        
        // Append text before placeholder
        result.append(format_str.substr(pos, brace_pos - pos));
        
        // Append argument if available
        if (arg_idx < args_.size()) {
            result += args_[arg_idx++];
        } else {
            // Not enough arguments, leave placeholder as-is
            result += "{}";
        }
        
        pos = brace_pos + 2; // Skip past {}
    }
    
    return result;
}

// StringBuilder implementation using Pimpl to hide ostringstream
class StringBuilder::Impl {
public:
    std::ostringstream stream;
};

StringBuilder::StringBuilder() : impl_(std::make_unique<Impl>()) {}
StringBuilder::~StringBuilder() = default;

template<typename T>
StringBuilder& StringBuilder::operator<<(const T& value) {
    impl_->stream << value;
    return *this;
}

std::string StringBuilder::str() const {
    return impl_->stream.str();
}

// Explicit template instantiations for common types
template StringBuilder& StringBuilder::operator<<(const char& value);
template StringBuilder& StringBuilder::operator<<(const int& value);
template StringBuilder& StringBuilder::operator<<(const unsigned& value);
template StringBuilder& StringBuilder::operator<<(const long& value);
template StringBuilder& StringBuilder::operator<<(const double& value);
template StringBuilder& StringBuilder::operator<<(const std::string& value);

std::string Join(const std::vector<std::string>& items, std::string_view separator) {
    if (items.empty()) return "";
    if (items.size() == 1) return items[0];
    
    std::ostringstream oss;
    for (size_t i = 0; i < items.size(); ++i) {
        if (i > 0) oss << separator;
        oss << items[i];
    }
    return oss.str();
}

} // namespace Fmt
} // namespace WingConnector
