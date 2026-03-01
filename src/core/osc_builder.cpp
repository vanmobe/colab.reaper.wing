/*
 * OSC Address Builder Implementation
 */

#include "internal/osc_builder.h"
#include <cstdio>

namespace WingConnector {

std::string OscBuilder::FormatNumber(int num) {
    char buf[4];
    snprintf(buf, sizeof(buf), "%02d", num);
    return std::string(buf);
}

std::string OscBuilder::ChannelPath(int channel_num, std::string_view suffix) {
    // Reserve typical size: "/ch/01/" (7) + suffix (avg 5-8) = ~15 bytes
    std::string result;
    result.reserve(32);
    result += "/ch/";
    result += FormatNumber(channel_num);
    result += '/';
    result += suffix;
    return result;
}

std::string OscBuilder::ChannelRoutingPath(int channel_num, std::string_view routing_type) {
    // "/ch/01/in/conn/grp" style paths
    std::string result;
    result.reserve(32);
    result += "/ch/";
    result += FormatNumber(channel_num);
    result += "/in/conn/";
    result += routing_type;
    return result;
}

std::string OscBuilder::UsbPath(int usb_num, std::string_view io_type, std::string_view suffix) {
    // "/io/out/USB/01/grp" or "/io/in/USB/01/name"
    std::string result;
    result.reserve(32);
    result += "/io/";
    result += io_type;  // "out" or "in"
    result += "/USB/";
    result += FormatNumber(usb_num);
    result += '/';
    result += suffix;   // "grp", "in", "name"
    return result;
}

std::string OscBuilder::CardPath(int card_num, std::string_view suffix) {
    // "/io/out/CRD/01/grp"
    std::string result;
    result.reserve(32);
    result += "/io/out/CRD/";
    result += FormatNumber(card_num);
    result += '/';
    result += suffix;
    return result;
}

} // namespace WingConnector
