#ifndef OSC_ROUTING_H
#define OSC_ROUTING_H

#include <string_view>
#include <functional>

namespace WingConnector {

// Efficient OSC address pattern matching and routing
class OscRouter {
public:
    enum class AddressType {
        CHANNEL,           // /ch/...
        USB_INPUT,         // /io/in/USR/...
        ANALOG_INPUT,      // /io/in/A/...
        DIGITAL_INPUT,     // /io/in/D/...
        CONSOLE_INFO,      // /info, /xinfo
        UNKNOWN
    };
    
    // Classify OSC address with minimal overhead (single pass)
    static AddressType ClassifyAddress(std::string_view address);
    
    // Check if address matches a prefix (inline for optimization)
    static inline bool StartsWith(std::string_view str, std::string_view prefix) {
        return str.size() >= prefix.size() && 
               str.compare(0, prefix.size(), prefix) == 0;
    }
    
    // Check for exact match (inline for optimization)
    static inline bool IsExact(std::string_view str, std::string_view target) {
        return str.size() == target.size() && str == target;
    }
};

} // namespace WingConnector

#endif // OSC_ROUTING_H
