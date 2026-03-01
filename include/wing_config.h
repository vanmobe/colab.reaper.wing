#ifndef WING_CONFIG_H
#define WING_CONFIG_H

#include <string>
#include <cstdint>

namespace WingConnector {

struct WingConfig {
    std::string wing_ip = "192.168.1.100";
    uint16_t wing_port = 2223;
    uint16_t listen_port = 2223;
    int channel_count = 48;
    int timeout_ms = 2000;
    bool create_stereo_pairs = false;
    bool color_tracks = true;
    std::string track_prefix = "Wing";
    
    // Soundcheck output mode: "USB" or "CARD"
    std::string soundcheck_output_mode = "USB";
    
    // MIDI action mapping
    bool configure_midi_actions = true;  // Default enabled - Wing buttons work automatically
    
    // Channel selection for virtual soundcheck
    // Format: comma-separated channel numbers, ranges allowed (e.g., "1,3-5,7")
    std::string include_channels = "";  // Empty = include all
    std::string exclude_channels = "";  // Empty = exclude none
    
    // Default track color (RGB)
    struct {
        uint8_t r = 100;
        uint8_t g = 150;
        uint8_t b = 200;
    } default_color;
    
    // Load configuration from JSON file
    bool LoadFromFile(const std::string& filepath);
    
    // Save configuration to JSON file
    bool SaveToFile(const std::string& filepath);
    
    // Get configuration file path
    static std::string GetConfigPath();
};

} // namespace WingConnector

#endif // WING_CONFIG_H
