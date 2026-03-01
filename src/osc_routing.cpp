/*
 * OSC Address Routing and Matching
 * Efficient address pattern classification
 */

#include "osc_routing.h"

namespace WingConnector {

OscRouter::AddressType OscRouter::ClassifyAddress(std::string_view address) {
    // Use prefixes in order of likelihood (channel messages are most common)
    // Each check is O(1) since we're just comparing prefix bytes
    
    if (StartsWith(address, "/ch/")) {
        return AddressType::CHANNEL;
    }
    
    if (StartsWith(address, "/io/in/USR/")) {
        return AddressType::USB_INPUT;
    }
    
    if (StartsWith(address, "/io/in/A/")) {
        return AddressType::ANALOG_INPUT;
    }
    
    if (StartsWith(address, "/io/in/D/")) {
        return AddressType::DIGITAL_INPUT;
    }
    
    if (IsExact(address, "/info") || IsExact(address, "/xinfo")) {
        return AddressType::CONSOLE_INFO;
    }
    
    return AddressType::UNKNOWN;
}

} // namespace WingConnector
