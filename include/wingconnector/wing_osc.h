#ifndef WING_OSC_H
#define WING_OSC_H

#include <string>
#include <map>
#include <memory>
#include <functional>
#include <cstdint>
#include <thread>
#include <atomic>
#include <mutex>
#include <cstddef>
#include <set>

class UdpListeningReceiveSocket;

namespace WingConnector {

class WingOscListener;

// Channel data structure
struct ChannelInfo {
    int channel_number;
    std::string name;
    int color;
    std::string source;
    std::string icon;
    bool stereo_linked = false;
    bool muted = false;
    int scribble_color = 0;
    
    // Primary routing (current input)
    std::string primary_source_group;  // "LCL", "A", "B", "C", "SC", "USB", etc.
    int primary_source_input;          // 1-48 within the group
    
    // ALT routing (for virtual soundcheck)
    std::string alt_source_group;      // "USB", "OFF", etc.
    int alt_source_input;              // 1-48 within the group
    bool alt_source_enabled;           // Is ALT currently active?
    
    // USB allocation (calculated)
    int usb_output_start = 0;          // USB output number (1-48)
    int usb_output_end = 0;            // For stereo: start+1, for mono: same as start
};

// USB allocation result
struct USBAllocation {
    int channel_number;
    bool is_stereo;
    int usb_start;  // For stereo: odd number (1,3,5...), for mono: any
    int usb_end;    // For stereo: usb_start+1, for mono: same as start
    std::string allocation_note;
};

// Basic metadata returned by the Wing handshake reply
struct WingInfo {
    std::string console_ip;
    std::string name;
    std::string model;
    std::string serial;
    std::string firmware;
};

// Callback for when channel data is received
using ChannelDataCallback = std::function<void(const ChannelInfo&)>;
using ProgressCallback = std::function<void(const std::string&)>;  // For real-time status updates

class WingOSC {
public:
    WingOSC(const std::string& wing_ip, uint16_t wing_port, uint16_t listen_port);
    ~WingOSC();
    
    // Start/stop the OSC listener
    bool Start();
    void Stop();
    
    // Test connection to Wing console
    bool TestConnection();
    
    // Query channel information
    void QueryChannel(int channel_num);
    void QueryAllChannels(int count);
    
    // Subscribe to real-time updates
    void SubscribeToChannel(int channel_num);
    void UnsubscribeFromChannel(int channel_num);
    
    // Get channel data
    const std::map<int, ChannelInfo>& GetChannelData() const { return channel_data_; }
    bool GetChannelInfo(int channel_num, ChannelInfo& info) const;
    
    // Set callback for when channel data is received
    void SetChannelCallback(ChannelDataCallback callback) { channel_callback_ = callback; }
    
    // Wing-specific OSC commands (based on Patrick Gillot's manual)
    void GetChannelName(int channel_num);
    void GetChannelColor(int channel_num);
    void GetChannelConfig(int channel_num);
    void GetChannelIcon(int channel_num);
    void GetChannelScribbleColor(int channel_num);
    
    // Channel routing queries (for virtual soundcheck)
    void GetChannelSourceRouting(int channel_num);     // Query primary source (grp, in)
    void GetChannelAltRouting(int channel_num);        // Query ALT source (altgrp, altin, altsrc)
    void GetChannelStereoLink(int channel_num);        // Query stereo link status
    
    // Channel routing configuration (for virtual soundcheck)
    void SetChannelAltSource(int channel_num, const std::string& grp, int in);
    void EnableChannelAltSource(int channel_num, bool enable);
    void SetAllChannelsAltEnabled(bool enable);  // Batch toggle for soundcheck mode
    
    // USB output routing configuration (for recording from Wing to REAPER)
    void SetUSBOutputSource(int usb_num, const std::string& grp, int in);
    void SetUSBOutputName(int usb_num, const std::string& name);   // Name USB output (source side)
    void SetUSBInputName(int usb_num, const std::string& name);    // Name USB input (REAPER receives from)
    void UnlockUSBOutputs();   // Unlock USB outputs before configuration
    void LockUSBOutputs();     // Lock USB outputs after configuration (optional)
    void ClearUSBOutput(int usb_num);  // Clear USB output routing (set to OFF)
    
    // CARD output routing configuration (alternative to USB)
    void SetCardOutputSource(int card_num, const std::string& grp, int in);
    void SetCardOutputName(int card_num, const std::string& name);
    void SetCardInputName(int card_num, const std::string& name);    // Name CARD input (REAPER sends back to)
    void ClearCardOutput(int card_num);  // Clear CARD output routing (set to OFF)
    
    // USB/CARD input mode configuration (stereo/mono)
    void SetUSBInputMode(int usb_num, const std::string& mode);  // Set USB input mode: "ST" = stereo, "M" = mono
    void SetCardInputMode(int card_num, const std::string& mode);  // Set CARD input mode: "ST" = stereo, "M" = mono
    void ClearUSBInput(int usb_num);  // Clear USB input (reset to mono mode)
    void ClearCardInput(int card_num);  // Clear CARD input (reset to mono mode)
    
    // USB allocation utilities
    std::vector<USBAllocation> CalculateUSBAllocation(const std::vector<ChannelInfo>& channels);
    void ApplyUSBAllocationAsAlt(const std::vector<USBAllocation>& allocations, 
                                  const std::vector<ChannelInfo>& channels,
                                  const std::string& output_mode = "USB");
    
    // User Signal input routing (for resolving indirection through USR inputs)
    void QueryUserSignalInputs(int count);  // Query all USR input sources
    std::pair<std::string, int> ResolveRoutingChain(const std::string& grp, int in);  // Follow routing chain
    void QueryInputSourceNames(const std::set<std::pair<std::string, int>>& sources);
    std::string GetInputSourceName(const std::string& grp, int in) const;
    
    // Additional Wing commands
    void GetConsoleInfo();
    void GetShowName();
    
    // Handle OSC messages (public for listener callback)
    void HandleOscMessage(const std::string& address, const void* data, size_t size);

    // Wing metadata collected during handshake
    const WingInfo& GetWingInfo() const { return wing_info_; }
    
private:
    friend class WingOscListener;

    std::string wing_ip_;
    uint16_t wing_port_;
    uint16_t listen_port_;
    
    std::atomic<bool> running_;
    std::unique_ptr<std::thread> listener_thread_;
    
    std::map<int, ChannelInfo> channel_data_;
    mutable std::mutex data_mutex_;
    
    // User Signal input routing: maps USR input number → (source_group, source_input)
    std::map<int, std::pair<std::string, int>> usr_routing_data_;
    std::map<std::string, std::string> input_source_names_;  // key: "GROUP:IN" -> display name
    
    // Config-based USR routing fallback: maps "USR:N" → "GROUP:M"
    // Used when Wing doesn't respond to /usr/* queries
    std::map<std::string, std::string> usr_routing_config_;
    
    ChannelDataCallback channel_callback_;
    WingInfo wing_info_{};
    bool handshake_complete_ = false;
    mutable std::mutex log_mutex_;
    
    // OSC socket handles (oscpack)
    UdpListeningReceiveSocket* osc_socket_;
    WingOscListener* osc_listener_;
    std::mutex send_mutex_;
    
    // Internal methods
    void ListenerThread();
    void ParseChannelName(int channel_num, const std::string& value);
    void ParseChannelColor(int channel_num, int value);
    void ParseChannelConfig(int channel_num, const std::string& value);
    bool PerformHandshake();
    bool SendRawPacket(const char* data, std::size_t size);
    void Log(const std::string& message) const;
    
    // Format channel number for OSC address (e.g., "01", "02", etc.)
    static std::string FormatChannelNum(int num);
};

} // namespace WingConnector

#endif // WING_OSC_H
