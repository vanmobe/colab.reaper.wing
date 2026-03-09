/*
 * Configuration Management
 * Load/save extension configuration
 */

#include "wingconnector/wing_config.h"
#include "internal/platform_util.h"
#include <fstream>
#include <cstdlib>
#include <filesystem>
#include <exception>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

namespace WingConnector {

std::string WingConfig::GetConfigPath() {
    return Platform::GetConfigFilePath();
}

bool WingConfig::LoadFromFile(const std::string& filepath) {
    try {
        std::ifstream file(filepath);
        if (!file.is_open()) {
            return false;
        }
        
        json config;
        file >> config;
        file.close();
        
        // Extract values with defaults
        wing_ip = config.value("wing_ip", "127.0.0.1");
        // Wing OSC port is fixed to 2223; keep ports constant in-memory.
        wing_port = 2223;
        listen_port = 2223;
        channel_count = config.value("channel_count", 48);
        timeout_ms = config.value("timeout", 10) * 1000;  // Convert to ms
        track_prefix = config.value("track_prefix", "");
        include_channels = config.value("include_channels", "");
        exclude_channels = config.value("exclude_channels", "");
        soundcheck_output_mode = config.value("soundcheck_output_mode", "USB");
        create_stereo_pairs = config.value("create_stereo_pairs", true);
        color_tracks = config.value("color_tracks", true);
        configure_midi_actions = config.value("configure_midi_actions", false);
        auto_record_enabled = config.value("auto_record_enabled", false);
        auto_record_warning_only = config.value("auto_record_warning_only", false);
        auto_record_threshold_db = config.value("auto_record_threshold_db", -40.0);
        auto_record_attack_ms = config.value("auto_record_attack_ms", 250);
        auto_record_hold_ms = config.value("auto_record_hold_ms", 3000);
        auto_record_release_ms = config.value("auto_record_release_ms", 2000);
        auto_record_min_record_ms = config.value("auto_record_min_record_ms", 5000);
        auto_record_poll_ms = config.value("auto_record_poll_ms", 50);
        auto_record_monitor_track = config.value("auto_record_monitor_track", 0);
        sd_lr_route_enabled = config.value("sd_lr_route_enabled", false);
        sd_lr_group = config.value("sd_lr_group", "MAIN");
        sd_lr_left_input = config.value("sd_lr_left_input", 1);
        sd_lr_right_input = config.value("sd_lr_right_input", 2);
        sd_auto_record_with_reaper = config.value("sd_auto_record_with_reaper", false);
        osc_warning_path = config.value("osc_warning_path", "/wing/record/warning");
        osc_start_path = config.value("osc_start_path", "/wing/record/start");
        osc_stop_path = config.value("osc_stop_path", "/wing/record/stop");
        warning_flash_cc_enabled = config.value("warning_flash_cc_enabled", true);
        warning_flash_cc_layer = config.value("warning_flash_cc_layer", 1);
        warning_flash_cc_color = config.value("warning_flash_cc_color", 9);
        
        // Extract color if present
        if (config.contains("default_track_color") && config["default_track_color"].is_object()) {
            const auto& color_obj = config["default_track_color"];
            default_color.r = color_obj.value("r", 80);
            default_color.g = color_obj.value("g", 80);
            default_color.b = color_obj.value("b", 80);
        }
        
        return true;
    }
    catch (const std::exception&) {
        return false;
    }
}

bool WingConfig::SaveToFile(const std::string& filepath) {
    namespace fs = std::filesystem;
    fs::path config_path(filepath);

    try {
        fs::path parent = config_path.parent_path();
        if (!parent.empty() && !fs::exists(parent)) {
            fs::create_directories(parent);
        }
        
        // Create JSON object
        json config;
        config["wing_ip"] = wing_ip;
        config["channel_count"] = channel_count;
        config["timeout"] = timeout_ms / 1000;  // Convert to seconds
        config["track_prefix"] = track_prefix;
        config["color_tracks"] = color_tracks;
        config["create_stereo_pairs"] = create_stereo_pairs;
        config["soundcheck_output_mode"] = soundcheck_output_mode;
        config["include_channels"] = include_channels;
        config["exclude_channels"] = exclude_channels;
        config["configure_midi_actions"] = configure_midi_actions;
        config["auto_record_enabled"] = auto_record_enabled;
        config["auto_record_warning_only"] = auto_record_warning_only;
        config["auto_record_threshold_db"] = auto_record_threshold_db;
        config["auto_record_attack_ms"] = auto_record_attack_ms;
        config["auto_record_hold_ms"] = auto_record_hold_ms;
        config["auto_record_release_ms"] = auto_record_release_ms;
        config["auto_record_min_record_ms"] = auto_record_min_record_ms;
        config["auto_record_poll_ms"] = auto_record_poll_ms;
        config["auto_record_monitor_track"] = auto_record_monitor_track;
        config["sd_lr_route_enabled"] = sd_lr_route_enabled;
        config["sd_lr_group"] = sd_lr_group;
        config["sd_lr_left_input"] = sd_lr_left_input;
        config["sd_lr_right_input"] = sd_lr_right_input;
        config["sd_auto_record_with_reaper"] = sd_auto_record_with_reaper;
        config["osc_warning_path"] = osc_warning_path;
        config["osc_start_path"] = osc_start_path;
        config["osc_stop_path"] = osc_stop_path;
        config["warning_flash_cc_enabled"] = warning_flash_cc_enabled;
        config["warning_flash_cc_layer"] = warning_flash_cc_layer;
        config["warning_flash_cc_color"] = warning_flash_cc_color;
        config["default_track_color"] = {
            {"r", (int)default_color.r},
            {"g", (int)default_color.g},
            {"b", (int)default_color.b}
        };
        
        // Write with nice formatting (2-space indent)
        std::ofstream file(config_path);
        if (!file.is_open()) {
            return false;
        }
        
        file << config.dump(2) << std::endl;
        file.close();
        return true;
    }
    catch (const std::exception&) {
        return false;
    }
}

} // namespace WingConnector
