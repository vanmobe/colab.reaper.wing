/*
 * OSC Address Builder
 * Efficient zero-copy OSC address construction using string_view
 */

#ifndef OSC_BUILDER_H
#define OSC_BUILDER_H

#include <string>
#include <string_view>

namespace WingConnector {

// Efficient OSC address builder using compile-time strings and minimal allocations
class OscBuilder {
public:
    // Format: "/ch/01/name" - builds path with channel number
    // Returns pre-formatted address string
    static std::string ChannelPath(int channel_num, std::string_view suffix);
    
    // Format: "/ch/01/in/conn/grp" - builds routing path
    static std::string ChannelRoutingPath(int channel_num, std::string_view routing_type);
    
    // Format: "/io/out/USB/01/grp" or "/io/in/USB/01/name" etc
    static std::string UsbPath(int usb_num, std::string_view io_type, std::string_view suffix);
    
    // Format: "/io/out/CRD/01/grp" - Card routing paths
    static std::string CardPath(int card_num, std::string_view suffix);
    
private:
    // Format a channel/port number as zero-padded 2-digit string (01-32, etc)
    static std::string FormatNumber(int num);
};

} // namespace WingConnector

#endif // OSC_BUILDER_H
