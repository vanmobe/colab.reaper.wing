#ifndef WING_CONFIG_H
#define WING_CONFIG_H

#include <string>
#include <cstdint>

namespace WingConnector {

struct WingConfig {
    std::string wing_ip = "192.168.1.100";
    // Wing OSC is fixed to 2223 on supported firmware.
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

    // Auto-record trigger (in REAPER, fed by Wing inputs)
    bool auto_record_enabled = false;
    bool auto_record_warning_only = false;
    double auto_record_threshold_db = -40.0;
    int auto_record_attack_ms = 250;
    int auto_record_hold_ms = 3000;
    int auto_record_release_ms = 2000;
    int auto_record_min_record_ms = 5000;
    int auto_record_poll_ms = 50;
    int auto_record_monitor_track = 0;  // 0 = auto (all armed+monitored), otherwise 1-based track index

    // Wing CARD routing helper for SD LR recording
    bool sd_lr_route_enabled = false;
    std::string sd_lr_group = "MAIN";
    int sd_lr_left_input = 1;
    int sd_lr_right_input = 2;
    bool sd_auto_record_with_reaper = false;

    // OSC paths sent to WING (wing_ip:2223)
    std::string osc_warning_path = "/wing/record/warning";
    std::string osc_start_path = "/wing/record/start";
    std::string osc_stop_path = "/wing/record/stop";

    // WING warning light behavior (Custom Controls)
    bool warning_flash_cc_enabled = true;
    int warning_flash_cc_layer = 1;   // /$ctl/user/<layer>/...
    int warning_flash_cc_color = 9;   // WING color index, 9 = red
    
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
