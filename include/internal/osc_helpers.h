#ifndef OSC_HELPERS_H
#define OSC_HELPERS_H

#include "osc/OscOutboundPacketStream.h"
#include "logger.h"
#include <functional>
#include <cstdint>

namespace WingConnector {

// Helper for building and sending OSC packets with minimal boilerplate
// Usage: SendOscPacket([](auto& p) { 
//   p << osc::BeginMessage("/test") << 42 << osc::EndMessage;
// }, wing_osc_instance);

template<typename SendFunc>
inline bool SendOscPacket(std::function<void(osc::OutboundPacketStream&)> builder, SendFunc send_func) {
    // Allocate buffer on stack (sufficient for most OSC messages)
    char buffer[512];
    osc::OutboundPacketStream p(buffer, sizeof(buffer));
    
    try {
        builder(p);
        return send_func(p.Data(), p.Size());
    }
    catch (const std::exception& e) {
        Logger::Error("OSC packet building failed: %s", e.what());
        return false;
    }
}

// Specialized helper for simple single-message packets
template<typename SendFunc>
inline bool SendOscMessage(const char* address, SendFunc send_func) {
    char buffer[256];
    osc::OutboundPacketStream p(buffer, sizeof(buffer));
    
    try {
        p << osc::BeginMessage(address) << osc::EndMessage;
        return send_func(p.Data(), p.Size());
    }
    catch (const std::exception& e) {
        Logger::Error("OSC message send failed for %s: %s", address, e.what());
        return false;
    }
}

} // namespace WingConnector

#endif // OSC_HELPERS_H
