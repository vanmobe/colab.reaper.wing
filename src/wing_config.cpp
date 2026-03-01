/*
 * Configuration Management
 * Load/save extension configuration
 */

#include "wing_config.h"
#include <fstream>
#include <cstdlib>
#include <filesystem>
#include <exception>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

namespace WingConnector {

std::string WingConfig::GetConfigPath() {
    // Get user config directory
    const char* home = getenv("HOME");
    if (!home) home = getenv("USERPROFILE"); // Windows
    
    if (home) {
        return std::string(home) + "/.wingconnector/config.json";
    }
    
    return "config.json"; // Fallback to current directory
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
        wing_port = config.value("wing_port", 9000);
        listen_port = config.value("listen_port", 9001);
        channel_count = config.value("channel_count", 48);
        timeout_ms = config.value("timeout", 10) * 1000;  // Convert to ms
        track_prefix = config.value("track_prefix", "");
        include_channels = config.value("include_channels", "");
        exclude_channels = config.value("exclude_channels", "");
        create_stereo_pairs = config.value("create_stereo_pairs", true);
        color_tracks = config.value("color_tracks", true);
        configure_midi_actions = config.value("configure_midi_actions", false);
        
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
        config["wing_port"] = wing_port;
        config["listen_port"] = listen_port;
        config["channel_count"] = channel_count;
        config["timeout"] = timeout_ms / 1000;  // Convert to seconds
        config["track_prefix"] = track_prefix;
        config["color_tracks"] = color_tracks;
        config["create_stereo_pairs"] = create_stereo_pairs;
        config["include_channels"] = include_channels;
        config["exclude_channels"] = exclude_channels;
        config["configure_midi_actions"] = configure_midi_actions;
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
