/*
 * Reaper Extension Main Class Implementation
 */

#include "wingconnector/reaper_extension.h"
#include "reaper_plugin_functions.h"
#ifdef __APPLE__
#include "internal/settings_dialog_macos.h"
#endif
#include <chrono>
#include <thread>
#include <cstdlib>
#include <cstring>
#include <cstdio>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <set>
#include <ctime>
#include <utime.h>

// C-style wrapper function for MIDI hook (REAPER requires a C function, not a member function)
extern "C" bool WingMidiInputHookWrapper(bool is_midi, const unsigned char* data, int len, int dev_id) {
    return WingConnector::ReaperExtension::MidiInputHook(is_midi, data, len, dev_id);
}

namespace WingConnector {

namespace {
constexpr int kChannelQueryAttempts = 2;
constexpr int kQueryResponseWaitMs = 600;  // Wait time for OSC responses after sending all queries
}  // namespace

// Static member definition
reaper_plugin_info_t* ReaperExtension::g_rec_ = nullptr;

// Helper function to parse channel selection (e.g., "1,3,5-7,10")
std::set<int> ParseChannelSelection(const std::string& selection_str, int max_channels) {
    std::set<int> selected;
    if (selection_str.empty()) return selected;
    std::istringstream iss(selection_str);
    std::string token;
    
    while (std::getline(iss, token, ',')) {
        // Trim whitespace
        token.erase(0, token.find_first_not_of(" \t"));
        token.erase(token.find_last_not_of(" \t") + 1);
        
        if (token.empty()) continue;
        
        // Check if it's a range (e.g., "5-7")
        size_t dash_pos = token.find('-');
        if (dash_pos != std::string::npos && dash_pos > 0 && dash_pos < token.length() - 1) {
            try {
                int start = std::stoi(token.substr(0, dash_pos));
                int end = std::stoi(token.substr(dash_pos + 1));
                for (int i = start; i <= end && i <= max_channels; ++i) {
                    if (i > 0) selected.insert(i);
                }
            } catch (...) {
                // Skip invalid ranges
            }
        } else {
            // Single number
            try {
                int ch = std::stoi(token);
                if (ch > 0 && ch <= max_channels) {
                    selected.insert(ch);
                }
            } catch (...) {
                // Skip invalid numbers
            }
        }
    }
    
    return selected;
}

ReaperExtension::ReaperExtension()
    : connected_(false)
    , monitoring_enabled_(false)
    , soundcheck_mode_enabled_(false)
    , midi_actions_enabled_(false)
    , status_message_("Not connected")
    , log_callback_(nullptr)
{
}

ReaperExtension::~ReaperExtension() {
    Shutdown();
}

ReaperExtension& ReaperExtension::Instance() {
    static ReaperExtension instance;
    return instance;
}

void ReaperExtension::Log(const std::string& message) {
    if (log_callback_) {
        log_callback_(message);
    }
}

bool ReaperExtension::Initialize(reaper_plugin_info_t* rec) {
    // Store g_rec context for later use in EnableMidiActions
    if (rec) {
        g_rec_ = rec;
        FILE* log_f = fopen("/tmp/wing_connector_debug.log", "a");
        if (log_f) {
            fprintf(log_f, "🔧 [WING] Stored REAPER plugin context (g_rec_ = %p)\n", (void*)rec);
            fprintf(log_f, "🔧 [WING] g_rec_->Register = %p\n", (void*)rec->Register);
            fflush(log_f);
            fclose(log_f);
        }
    } else {
        FILE* log_f = fopen("/tmp/wing_connector_debug.log", "a");
        if (log_f) {
            fprintf(log_f, "🔧 [WING] WARNING: g_rec parameter is nullptr!\n");
            fflush(log_f);
            fclose(log_f);
        }
    }
    
    // Load configuration
    std::string config_path = WingConfig::GetConfigPath();
    fprintf(stderr, "🔧 [WING] Loading config from: %s\n", config_path.c_str());
    fflush(stderr);
    
    bool loaded_user_config = config_.LoadFromFile(config_path);
    if (!loaded_user_config) {
        fprintf(stderr, "🔧 [WING] User config not found, trying install directory\n");
        fflush(stderr);
        // Try loading from install directory
        if (!config_.LoadFromFile("config.json")) {
            fprintf(stderr, "🔧 [WING] Using default configuration\n");
            fflush(stderr);
            Log("Wing Connector: Using default configuration\n");
        }
    } else {
        fprintf(stderr, "🔧 [WING] Configuration loaded successfully\n");
        fflush(stderr);
        Log("Wing Connector: Configuration loaded\n");
        
        bool config_updated = false;
        
        // Migrate legacy default listen port (2224 -> 2223)
        if (config_.listen_port == 2224) {
            config_.listen_port = 2223;
            config_updated = true;
            Log("Wing Connector: Updated listener port to 2223\n");
        }
        
        // Auto-enable MIDI actions if not configured
        if (!config_.configure_midi_actions) {
            config_.configure_midi_actions = true;
            config_updated = true;
            Log("Wing Connector: Auto-enabled MIDI action mapping for Wing buttons\n");
        }
        
        // Save updated config
        if (config_updated && config_.SaveToFile(config_path)) {
            fprintf(stderr, "🔧 [WING] Configuration auto-updated and saved\n");
            fflush(stderr);
        }
    }
    
    fprintf(stderr, "🔧 [WING] Creating track manager\n");
    fflush(stderr);
    
    // Create track manager
    track_manager_ = std::make_unique<TrackManager>(config_);
    
    fprintf(stderr, "🔧 [WING] Enabling Wing MIDI device\n");
    fflush(stderr);
    
    // Enable Wing MIDI device in REAPER settings
    EnableWingMidiDevice();
    
    fprintf(stderr, "🔧 [WING] MIDI actions enabled: %d\n", config_.configure_midi_actions);
    fflush(stderr);
    
    // Enable MIDI actions if configured
    if (config_.configure_midi_actions) {
        fprintf(stderr, "🔧 [WING] Enabling MIDI actions\n");
        fflush(stderr);
        EnableMidiActions(true);
    }
    
    fprintf(stderr, "🔧 [WING] ReaperExtension::Initialize() complete\n");
    fflush(stderr);
    
    return true;
}

void ReaperExtension::Shutdown() {
    EnableMidiActions(false);
    DisconnectFromWing();
    track_manager_.reset();
}

// New version: Returns success/failure, doesn't auto-create tracks
bool ReaperExtension::ConnectToWing() {
    if (connected_) {
        Log("Wing Connector: Already connected\n");
        return true;
    }
    
    Log("Wing Connector: Connecting to Wing...\n");
    status_message_ = "Connecting...";
    
    // Create OSC handler
    osc_handler_ = std::make_unique<WingOSC>(
        config_.wing_ip,
        config_.wing_port,
        config_.listen_port
    );
    
    // Set callback
    osc_handler_->SetChannelCallback(
        [this](const ChannelInfo& channel) {
            OnChannelDataReceived(channel);
        }
    );
    
    // Start OSC server
    if (!osc_handler_->Start()) {
        Log("Wing Connector: Failed to start OSC server. Port may be in use.\n");
        osc_handler_.reset();
        status_message_ = "Failed to start";
        return false;
    }
    
    // Test connection
    if (!osc_handler_->TestConnection()) {
        Log("Wing Connector: Could not connect to Wing console. Check IP and OSC settings.\n");
        osc_handler_->Stop();
        osc_handler_.reset();
        status_message_ = "Connection failed";
        return false;
    }
    
    Log("Wing Connector: Connected!\n");
    connected_ = true;
    status_message_ = "Connected";
    
    // Query console info
    const auto& wing_info = osc_handler_->GetWingInfo();
    if (!wing_info.model.empty()) {
        char info_msg[256];
        snprintf(info_msg, sizeof(info_msg),
                 "Wing Connector: Detected %s (%s) FW %s\n",
                 wing_info.model.c_str(),
                 wing_info.name.empty() ? "Unnamed" : wing_info.name.c_str(),
                 wing_info.firmware.empty() ? "unknown" : wing_info.firmware.c_str());
        Log(info_msg);
    }
    
    return true;
}

// Get available channels with sources assigned
std::vector<ChannelSelectionInfo> ReaperExtension::GetAvailableChannels() {
    std::vector<ChannelSelectionInfo> result;
    
    if (!connected_ || !osc_handler_) {
        Log("Wing Connector: Not connected. Cannot query channels.\n");
        return result;
    }
    
    Log("Wing Connector: Querying channels...\n");
    
    // Query all channels with retries
    const auto query_delay = std::chrono::milliseconds(kQueryResponseWaitMs);
    for (int attempt = 1; attempt <= kChannelQueryAttempts; ++attempt) {
        char attempt_msg[128];
        snprintf(attempt_msg, sizeof(attempt_msg),
                 "Wing Connector: Querying channels (attempt %d/%d)\n",
                 attempt, kChannelQueryAttempts);
        Log(attempt_msg);
        osc_handler_->QueryAllChannels(config_.channel_count);
        std::this_thread::sleep_for(query_delay);
        if (!osc_handler_->GetChannelData().empty()) {
            break;
        }
        if (attempt < kChannelQueryAttempts) {
            Log("Wing Connector: No channel data yet, retrying...\n");
        }
    }
    
    // Get channel data
    const auto& channel_data = osc_handler_->GetChannelData();
    if (channel_data.empty()) {
        Log("Wing Connector: No channel data received. Check timeout settings.\n");
        return result;
    }

    // Query USR routing so popup can display resolved sources (e.g. USR:25 -> A:8)
    osc_handler_->QueryUserSignalInputs(48);

    // Query source labels for A-inputs used by selected channels (for name fallback).
    std::set<std::pair<std::string, int>> source_endpoints;
    for (const auto& pair : channel_data) {
        const ChannelInfo& ch = pair.second;
        if (ch.primary_source_group.empty() || ch.primary_source_input <= 0) {
            continue;
        }
        auto resolved = osc_handler_->ResolveRoutingChain(ch.primary_source_group, ch.primary_source_input);
        source_endpoints.insert(resolved);
    }
    osc_handler_->QueryInputSourceNames(source_endpoints);
    
    Log("Wing Connector: Processing channel data...\n");
    
    // Build list of channels with sources.
    // Stereo pairs are represented as a single row (anchor = lower channel number)
    // and include both source endpoints for clear popup display.
    std::set<int> processed_channels;

    for (const auto& pair : channel_data) {
        const ChannelInfo& ch = pair.second;

        if (processed_channels.count(ch.channel_number)) {
            continue;
        }
        if (ch.primary_source_group.empty()) {
            continue;
        }

        ChannelSelectionInfo info;
        info.partner_name.clear();
        info.partner_source_group.clear();
        info.partner_source_input = 0;

        if (ch.stereo_linked) {
            int partner_num = -1;
            auto it_plus = channel_data.find(ch.channel_number + 1);
            if (it_plus != channel_data.end() && it_plus->second.stereo_linked &&
                !it_plus->second.primary_source_group.empty()) {
                partner_num = ch.channel_number + 1;
            } else {
                auto it_minus = channel_data.find(ch.channel_number - 1);
                if (it_minus != channel_data.end() && it_minus->second.stereo_linked &&
                    !it_minus->second.primary_source_group.empty()) {
                    partner_num = ch.channel_number - 1;
                }
            }

            if (partner_num != -1) {
                int anchor_num = std::min(ch.channel_number, partner_num);
                if (ch.channel_number != anchor_num) {
                    continue;
                }

                const ChannelInfo& left = ch;
                const ChannelInfo& right = channel_data.at(partner_num);

                info.channel_number = anchor_num;

                if (!left.name.empty() && !right.name.empty() && left.name != right.name) {
                    info.name = left.name + " / " + right.name;
                } else if (!left.name.empty()) {
                    info.name = left.name;
                } else if (!right.name.empty()) {
                    info.name = right.name;
                } else {
                    info.name = "Stereo";
                }

                info.source_group = left.primary_source_group;
                info.source_input = left.primary_source_input;
                info.partner_name = right.name;
                info.partner_source_group = right.primary_source_group;
                info.partner_source_input = right.primary_source_input;

                auto resolved_left = osc_handler_->ResolveRoutingChain(info.source_group, info.source_input);
                info.source_group = resolved_left.first;
                info.source_input = resolved_left.second;

                auto resolved_right = osc_handler_->ResolveRoutingChain(info.partner_source_group, info.partner_source_input);
                info.partner_source_group = resolved_right.first;
                info.partner_source_input = resolved_right.second;

                if (left.name.empty() && right.name.empty()) {
                    std::string left_src_name = osc_handler_->GetInputSourceName(info.source_group, info.source_input);
                    std::string right_src_name = osc_handler_->GetInputSourceName(info.partner_source_group, info.partner_source_input);
                    if (!left_src_name.empty() && !right_src_name.empty()) {
                        info.name = (left_src_name == right_src_name) ? left_src_name : (left_src_name + " / " + right_src_name);
                    } else if (!left_src_name.empty()) {
                        info.name = left_src_name;
                    } else if (!right_src_name.empty()) {
                        info.name = right_src_name;
                    }
                }

                if (info.name == "Stereo") {
                    info.name = "Stereo " + info.source_group + std::to_string(info.source_input) +
                                "/" + info.partner_source_group + std::to_string(info.partner_source_input);
                }

                info.stereo_linked = true;
                info.selected = (!info.name.empty() && info.name.find("Stereo ") != 0);

                result.push_back(info);
                processed_channels.insert(ch.channel_number);
                processed_channels.insert(partner_num);
                continue;
            }
        }

        info.channel_number = ch.channel_number;
        info.name = ch.name.empty() ? ("CH" + std::to_string(ch.channel_number)) : ch.name;
        info.source_group = ch.primary_source_group;
        info.source_input = ch.primary_source_input;

        auto resolved = osc_handler_->ResolveRoutingChain(info.source_group, info.source_input);
        info.source_group = resolved.first;
        info.source_input = resolved.second;

        if (ch.name.empty()) {
            std::string src_name = osc_handler_->GetInputSourceName(info.source_group, info.source_input);
            if (!src_name.empty()) {
                info.name = src_name;
            }
        }

        info.stereo_linked = ch.stereo_linked;
        info.selected = !info.name.empty() && info.name.rfind("CH", 0) != 0;

        result.push_back(info);
        processed_channels.insert(ch.channel_number);
    }
    
    char msg[128];
    snprintf(msg, sizeof(msg), "Wing Connector: Found %d channels with sources\n", 
             (int)result.size());
    Log(msg);
    
    return result;
}

// Create tracks from selected channels
void ReaperExtension::CreateTracksFromSelection(const std::vector<ChannelSelectionInfo>& channels) {
    if (!connected_ || !osc_handler_) {
        Log("Wing Connector: Not connected\n");
        return;
    }
    
    Log("Wing Connector: Creating tracks from selection...\n");
    
    // Filter to only selected channels
    std::vector<ChannelSelectionInfo> selected;
    for (const auto& ch : channels) {
        if (ch.selected) {
            selected.push_back(ch);
        }
    }
    
    if (selected.empty()) {
        Log("Wing Connector: No channels selected\n");
        return;
    }
    
    // Get full channel data from OSC handler
    const auto& channel_data = osc_handler_->GetChannelData();
    
    // Build filtered channel data map
    std::map<int, ChannelInfo> filtered_data;
    for (const auto& sel : selected) {
        auto it = channel_data.find(sel.channel_number);
        if (it != channel_data.end()) {
            ChannelInfo ch_info = it->second;
            if (ch_info.name.empty()) {
                if (!sel.name.empty()) {
                    ch_info.name = sel.name;
                } else {
                    ch_info.name = "CH" + std::to_string(sel.channel_number);
                }
            }
            filtered_data[sel.channel_number] = ch_info;
        }
    }
    
    // Create tracks
    int track_count = track_manager_->CreateTracksFromChannelData(filtered_data);
    
    char msg[128];
    snprintf(msg, sizeof(msg), "Wing Connector: Created %d tracks\n", track_count);
    Log(msg);
}

bool ReaperExtension::CheckOutputModeAvailability(const std::string& output_mode, std::string& details) const {
    const std::string mode = (output_mode == "CARD") ? "CARD" : "USB";
    const int required_channels = (mode == "CARD") ? 32 : 48;

    const int available_inputs = GetNumAudioInputs();
    const int available_outputs = GetNumAudioOutputs();

    if (available_inputs < required_channels || available_outputs < required_channels) {
        details = "Selected mode " + mode + " may not be fully available in REAPER device I/O. "
                  "Required (full bank): " + std::to_string(required_channels) + " in / " +
                  std::to_string(required_channels) + " out, available: " +
                  std::to_string(available_inputs) + " in / " + std::to_string(available_outputs) + " out.";
        return false;
    }

    details = mode + " mode available in REAPER device I/O (" +
              std::to_string(available_inputs) + " in / " +
              std::to_string(available_outputs) + " out).";
    return true;
}

// Setup virtual soundcheck from selected channels
void ReaperExtension::SetupSoundcheckFromSelection(const std::vector<ChannelSelectionInfo>& channels) {
    if (!connected_ || !osc_handler_) {
        Log("Wing Connector: Not connected\n");
        return;
    }
    
    Log("Wing Connector: Setting up Virtual Soundcheck...\n");
    
    // Filter to only selected channels
    std::vector<ChannelInfo> selected_channels;
    const auto& channel_data = osc_handler_->GetChannelData();
    
    for (const auto& sel : channels) {
        if (sel.selected) {
            auto it = channel_data.find(sel.channel_number);
            if (it != channel_data.end()) {
                ChannelInfo ch_info = it->second;
                if (ch_info.name.empty()) {
                    if (!sel.name.empty()) {
                        ch_info.name = sel.name;
                    } else {
                        ch_info.name = "CH" + std::to_string(sel.channel_number);
                    }
                }
                selected_channels.push_back(ch_info);
            }
        }
    }
    
    if (selected_channels.empty()) {
        Log("Wing Connector: No channels selected\n");
        return;
    }
    
    // Refresh stereo link status from Wing console BEFORE calculating allocation
    Log("Refreshing channel stereo link status from Wing...\n");
    for (const auto& ch : selected_channels) {
        osc_handler_->QueryChannel(ch.channel_number);
    }
    // Allow time for responses to arrive
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    
    // Get updated channel data with fresh stereo_linked status
    selected_channels.clear();
    const auto& updated_channel_data = osc_handler_->GetChannelData();
    for (const auto& sel : channels) {
        if (sel.selected) {
            auto it = updated_channel_data.find(sel.channel_number);
            if (it != updated_channel_data.end()) {
                ChannelInfo ch_info = it->second;
                if (ch_info.name.empty()) {
                    if (!sel.name.empty()) {
                        ch_info.name = sel.name;
                    } else {
                        ch_info.name = "CH" + std::to_string(sel.channel_number);
                    }
                }
                selected_channels.push_back(ch_info);
            }
        }
    }
    
    // Calculate USB allocation
    auto allocations = osc_handler_->CalculateUSBAllocation(selected_channels);
    
    // Get output mode for display
    std::string output_mode = config_.soundcheck_output_mode;
    std::string output_type = (output_mode == "CARD") ? "CARD" : "USB";

    int required_io_channels = 0;
    for (const auto& alloc : allocations) {
        if (!alloc.allocation_note.empty() && alloc.allocation_note.find("ERROR") != std::string::npos) {
            continue;
        }
        required_io_channels = std::max(required_io_channels, alloc.usb_end);
    }

    const int available_inputs = GetNumAudioInputs();
    const int available_outputs = GetNumAudioOutputs();
    if (available_inputs < required_io_channels || available_outputs < required_io_channels) {
        std::ostringstream err;
        err << "Wing Connector: REAPER audio device does not expose enough channels for "
            << output_type << " soundcheck. Required by current selection: "
            << required_io_channels << " in / " << required_io_channels << " out, available: "
            << available_inputs << " in / " << available_outputs << " out.\n";
        Log(err.str());

        std::ostringstream msg;
        msg << "Selected " << output_type << " routing requires at least "
            << required_io_channels << " REAPER inputs and outputs.\n\n"
            << "Available now: " << available_inputs << " inputs / "
            << available_outputs << " outputs.\n\n"
            << "Please switch REAPER audio device/range or choose fewer channels.";
        ShowMessageBox(msg.str().c_str(), "Wing Connector - Audio I/O Not Available", 0);
        return;
    }
    
    // Show what will be configured
    Log("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n");
    Log("CONFIGURING WING CONSOLE FOR VIRTUAL SOUNDCHECK\n");
    Log("Output Mode: " + output_type + "\n");
    Log("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n");
    Log("\nChannels to configure:\n");
    for (const auto& alloc : allocations) {
        std::string line = "  CH" + std::to_string(alloc.channel_number);
        if (alloc.is_stereo) {
            line += " (stereo) → " + output_type + " " + std::to_string(alloc.usb_start) + "-" + std::to_string(alloc.usb_end);
        } else {
            line += " (mono) → " + output_type + " " + std::to_string(alloc.usb_start);
        }
        line += "\n";
        Log(line.c_str());
    }
    Log("\n");
    
    // Apply output allocation
    Log("Step 1/2: Configuring Wing " + output_type + " outputs and ALT sources...\n");
    // Query USR routing data before configuring (to resolve routing chains)
    osc_handler_->QueryUserSignalInputs(48);
    osc_handler_->ApplyUSBAllocationAsAlt(allocations, selected_channels, output_mode);
    
    Log("\nStep 2/2: Creating REAPER tracks...\n");
    
    // Create tracks
    std::map<int, ChannelInfo> channel_map;
    for (const auto& ch : selected_channels) {
        channel_map[ch.channel_number] = ch;
    }
    
    Undo_BeginBlock();
    
    int track_index = 0;
    int created_tracks = 0;
    for (const auto& alloc : allocations) {
        if (!alloc.allocation_note.empty() && alloc.allocation_note.find("ERROR") != std::string::npos) {
            continue;
        }
        
        auto it = channel_map.find(alloc.channel_number);
        if (it == channel_map.end()) {
            continue;
        }
        const ChannelInfo& ch_info = it->second;
        const std::string track_name = ch_info.name.empty() ?
            ("CH" + std::to_string(alloc.channel_number)) : ch_info.name;
        
        MediaTrack* track = nullptr;
        if (alloc.is_stereo) {
            track = track_manager_->CreateStereoTrack(track_index, track_name, ch_info.color);
            if (track) {
                track_manager_->SetTrackInput(track, alloc.usb_start - 1, 2);
                track_manager_->SetTrackHardwareOutput(track, alloc.usb_start - 1, 2);
                SetMediaTrackInfo_Value(track, "B_MAINSEND", 0);
                std::string msg = "  ✓ Track " + std::to_string(created_tracks + 1) + 
                                 ": " + track_name + " (stereo) IN " + output_type + " " +
                                 std::to_string(alloc.usb_start) + "-" + std::to_string(alloc.usb_end) +
                                 " / OUT " + output_type + " " +
                                 std::to_string(alloc.usb_start) + "-" + std::to_string(alloc.usb_end) + "\n";
                Log(msg.c_str());
            }
        } else {
            track = track_manager_->CreateTrack(track_index, track_name, ch_info.color);
            if (track) {
                track_manager_->SetTrackInput(track, alloc.usb_start - 1, 1);
                track_manager_->SetTrackHardwareOutput(track, alloc.usb_start - 1, 1);
                SetMediaTrackInfo_Value(track, "B_MAINSEND", 0);
                std::string msg = "  ✓ Track " + std::to_string(created_tracks + 1) + 
                                 ": " + track_name + " (mono) IN " + output_type + " " +
                                 std::to_string(alloc.usb_start) + " / OUT " + output_type + " " +
                                 std::to_string(alloc.usb_start) + "\n";
                Log(msg.c_str());
            }
        }
        
        if (track) {
            track_index++;
            created_tracks++;
        }
    }
    
    Undo_EndBlock("Wing Connector: Configure Virtual Soundcheck", UNDO_STATE_TRACKCFG);
    
    Log("\n━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n");
    Log("✓ CONFIGURATION COMPLETE\n");
    Log("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n");
    std::string final_msg = "Created " + std::to_string(created_tracks) + " REAPER tracks\n";
    Log(final_msg.c_str());
    Log("\nUse 'Toggle Soundcheck Mode' to enable/disable ALT sources.\n");
    Log("When enabled, channels receive audio from REAPER via USB.\n\n");
}

void ReaperExtension::DisconnectFromWing() {
    if (!connected_) {
        return;
    }
    
    Log("Wing Connector: Disconnecting...\n");
    
    if (osc_handler_) {
        osc_handler_->Stop();
        osc_handler_.reset();
    }
    
    connected_ = false;
    monitoring_enabled_ = false;
    status_message_ = "Disconnected";
    
    Log("Wing Connector: Disconnected\n");
}

void ReaperExtension::RefreshTracks() {
    if (!connected_ || !osc_handler_) {
        ShowMessageBox(
            "Not connected to Wing console.\n"
            "Please connect first.",
            "Wing Connector",
            0
        );
        return;
    }
    
    Log("Wing Connector: Refreshing tracks...\n");
    
    // Re-query channels
    osc_handler_->QueryAllChannels(config_.channel_count);
    std::this_thread::sleep_for(std::chrono::milliseconds(300));  // Wait for OSC responses
    
    // Update existing tracks or create new ones
    const auto& channel_data = osc_handler_->GetChannelData();
    int track_count = track_manager_->CreateTracksFromChannelData(channel_data);
    
    char msg[256];
    snprintf(msg, sizeof(msg), "Refreshed %d tracks", track_count);
    Log(msg);
    Log("\n");
}

void ReaperExtension::ShowSettings() {
    FILE* dbg = fopen("/tmp/wing_debug.log", "a");
    fprintf(dbg, "[ShowSettings] Called\n");
    fflush(dbg);
    
    #ifdef __APPLE__
    // Use native macOS dialog for settings
    fprintf(dbg, "[ShowSettings] Apple platform\n");
    fflush(dbg);
    
    char ip_buffer[256];
    char port_buffer[16];
    
    strncpy(ip_buffer, config_.wing_ip.c_str(), sizeof(ip_buffer) - 1);
    ip_buffer[sizeof(ip_buffer) - 1] = '\0';
    
    snprintf(port_buffer, sizeof(port_buffer), "%u", config_.listen_port);
    fprintf(dbg, "[ShowSettings] Buffers prepared, calling ShowSettingsDialog\n");
    fflush(dbg);
    
    // Show native Cocoa dialog
    if (ShowSettingsDialog(config_.wing_ip.c_str(), config_.listen_port,
                          ip_buffer, sizeof(ip_buffer),
                          port_buffer, sizeof(port_buffer))) {
        fprintf(dbg, "[ShowSettings] Dialog confirmed\n");
        fflush(dbg);
        // Validate IP
        std::string new_ip = ip_buffer;
        if (new_ip.empty() || new_ip.length() > 15) {
            ShowMessageBox("Invalid IP address.\nPlease use format: 192.168.0.1", 
                          "Wing Connector - Error", 0);
            return;
        }
        
        // Validate port
        int new_port = atoi(port_buffer);
        if (new_port < 1024 || new_port > 65535) {
            ShowMessageBox("Invalid port number.\nPlease enter a value between 1024 and 65535.", 
                          "Wing Connector - Error", 0);
            return;
        }
        
        // Update configuration
        config_.wing_ip = new_ip;
        config_.listen_port = static_cast<uint16_t>(new_port);
        
        // Save to file
        const std::string config_path = WingConfig::GetConfigPath();
        if (config_.SaveToFile(config_path)) {
            char success_msg[256];
            snprintf(success_msg, sizeof(success_msg),
                "Settings saved successfully!\n\n"
                "IP: %s\n"
                "Port: %d\n\n"
                "Changes will apply on next connection.",
                config_.wing_ip.c_str(),
                config_.listen_port);
            
            ShowMessageBox(success_msg, "Wing Connector - Settings Saved", 0);
            Log("Wing Connector: Settings updated from dialog\n");
        } else {
            char error_msg[512];
            snprintf(error_msg, sizeof(error_msg),
                      "Failed to save settings to:\n%s\n\nPlease check file permissions.",
                      config_path.c_str());
            ShowMessageBox(error_msg,
                          "Wing Connector - Error", 0);
        }
    }
    #else
    // Fallback for non-macOS platforms
    char settings_msg[512];
    snprintf(settings_msg, sizeof(settings_msg), 
        "Wing Connector Settings\n\n"
        "Current Configuration:\n"
        "  Wing IP: %s\n"
        "  Listen Port: %u\n"
        "\nEdit config.json to change settings.\n",
        config_.wing_ip.c_str(),
        config_.listen_port);
    
    ShowMessageBox(settings_msg, "Wing Connector - Settings", 0);
    #endif
}

void ReaperExtension::EnableMonitoring(bool enable) {
    monitoring_enabled_ = enable;
    
    if (enable) {
        Log("Wing Connector: Real-time monitoring enabled\n");
        status_message_ = "Monitoring active";
    } else {
        Log("Wing Connector: Real-time monitoring disabled\n");
        status_message_ = "Monitoring inactive";
    }
}

void ReaperExtension::ConfigureVirtualSoundcheck() {
    if (!connected_ || !osc_handler_) {
        ShowMessageBox(
            "Please connect to Wing console first",
            "Wing Connector - Virtual Soundcheck",
            0
        );
        return;
    }
    
    Log("Wing Connector: Configuring Virtual Soundcheck...\n");
    
    // Get all channel data
    const auto& channels = osc_handler_->GetChannelData();
    if (channels.empty()) {
        ShowMessageBox(
            "No channel data available.\nPlease refresh tracks first.",
            "Wing Connector - Virtual Soundcheck",
            0
        );
        return;
    }
    
    // Build list of channels with names and sources (these are the "useful" ones)
    std::vector<ChannelInfo> all_channels;
    std::vector<ChannelInfo> selectable_channels;  // Only channels with name AND source
    std::set<int> included_channel_numbers;  // Track which channels are included
    
    for (const auto& pair : channels) {
        all_channels.push_back(pair.second);
        // Only include if has both name and source
        if (!pair.second.name.empty() && !pair.second.primary_source_group.empty()) {
            selectable_channels.push_back(pair.second);
            included_channel_numbers.insert(pair.second.channel_number);
        }
    }
    
    // For stereo-linked channels, ensure BOTH partners are included
    // even if one doesn't have its own name
    std::vector<ChannelInfo> additional_channels;
    for (const auto& ch : selectable_channels) {
        if (ch.stereo_linked) {
            int partner_num = -1;
            
            if (ch.channel_number % 2 == 1) {
                // Odd channel: partner is next even channel
                partner_num = ch.channel_number + 1;
            } else {
                // Even channel: partner is previous odd channel
                partner_num = ch.channel_number - 1;
            }
            
            // Check if partner exists and isn't already included
            if (included_channel_numbers.find(partner_num) == included_channel_numbers.end()) {
                // Find the partner in all_channels
                for (const auto& pair : channels) {
                    if (pair.second.channel_number == partner_num) {
                        additional_channels.push_back(pair.second);
                        included_channel_numbers.insert(partner_num);
                        ShowConsoleMsg("Wing Connector: Auto-including stereo partner CH");
                        char buf[10];
                        snprintf(buf, sizeof(buf), "%d", partner_num);
                        ShowConsoleMsg(buf);
                        ShowConsoleMsg(" for CH");
                        snprintf(buf, sizeof(buf), "%d", ch.channel_number);
                        ShowConsoleMsg(buf);
                        ShowConsoleMsg("\n");
                        break;
                    }
                }
            }
        }
    }
    
    // Add stereo partners to selectable list
    selectable_channels.insert(selectable_channels.end(), additional_channels.begin(), additional_channels.end());
    
    if (selectable_channels.empty()) {
        ShowMessageBox(
            "No channels with both name and source found.\n\n"
            "To proceed:\n"
            "1. Assign names to Wing channels\n"
            "2. Assign input sources to Wing channels\n"
            "3. Refresh tracks and try again",
            "Wing Connector - No Channels Available",
            0
        );
        return;
    }
    
    // Build display of available channels
    std::ostringstream channel_list;
    channel_list << "Available channels:\n\n";
    
    for (const auto& ch : selectable_channels) {
        channel_list << "  CH" << std::setw(2) << std::setfill('0') << ch.channel_number 
                    << ": " << std::setfill(' ') << std::left << std::setw(25) << ch.name 
                    << " (" << ch.primary_source_group 
                    << std::to_string(ch.primary_source_input) << ")\n";
    }
    
    channel_list << "\nTotal: " << selectable_channels.size() << " channels\n"
                << "\nCustomize channel selection?";
    
    int result = ShowMessageBox(
        channel_list.str().c_str(),
        "Wing Connector - Channel Selection",
        4  // Yes/No/Cancel
    );
    
    if (result == 0) {
        Log("Wing Connector: Virtual Soundcheck configuration cancelled\n");
        return;
    }
    
    std::vector<ChannelInfo> selected_channels = selectable_channels;
    
    // Apply include/exclude filtering from config
    if (!config_.include_channels.empty() || !config_.exclude_channels.empty()) {
        std::vector<ChannelInfo> filtered_channels;
        
        // Parse include and exclude lists
        std::set<int> included_set = ParseChannelSelection(config_.include_channels, 48);
        std::set<int> excluded_set = ParseChannelSelection(config_.exclude_channels, 48);
        
        for (const auto& ch : selected_channels) {
            bool should_include = true;
            
            // If include list exists, only include channels in the list
            if (!included_set.empty()) {
                should_include = (included_set.find(ch.channel_number) != included_set.end());
            }
            
            // Exclude any channels in the exclude list
            if (excluded_set.find(ch.channel_number) != excluded_set.end()) {
                should_include = false;
            }
            
            if (should_include) {
                filtered_channels.push_back(ch);
            }
        }
        
        selected_channels = filtered_channels;
        
        if (selected_channels.empty()) {
            Log("Wing Connector: No channels remain after filtering. Using all channels.\n");
            selected_channels = selectable_channels;
        }
    }
    
    // If user wants to customize (clicked "Yes"=6)
    if (result == 6) {
        Log("\n=== CHANNEL SELECTION ===\n");
        Log("Available channels:\n");
        for (const auto& ch : selectable_channels) {
            Log("  CH");
            char ch_buf[10];
            snprintf(ch_buf, sizeof(ch_buf), "%02d", ch.channel_number);
            Log(ch_buf);
            Log(": ");
            Log(ch.name.c_str());
            Log(" (");
            Log(ch.primary_source_group.c_str());
            char in_buf[10];
            snprintf(in_buf, sizeof(in_buf), "%d", ch.primary_source_input);
            Log(in_buf);
            Log(")\n");
        }
        
        Log("\nTo proceed with only specific channels, use the following format:\n");
        Log("  INCLUDE: 1,3-5,7 (or leave blank for all)\n");
        Log("  EXCLUDE: 2,4,6   (or leave blank to exclude none)\n");
        Log("\nPlease edit the config.json file and set:\n");
        Log("  \"include_channels\": \"...\"\n");
        Log("  \"exclude_channels\": \"...\"\n");
        Log("\nThen restart the virtual soundcheck process.\n\n");
        
        // For now, use all channels if user chose customize
        // (they can edit config and re-run)
        Log("Proceeding with all available channels.\n");
        Log("(Edit config.json to customize channel selection)\n\n");
    }
    
    // Calculate USB allocation with gap backfilling for selected channels
    auto allocations = osc_handler_->CalculateUSBAllocation(selected_channels);
    
    // Show what will be configured
    Log("Channels to configure:\n");
    for (const auto& alloc : allocations) {
        std::string line = "  CH" + std::to_string(alloc.channel_number);
        if (alloc.is_stereo) {
            line += " (stereo) → USB " + std::to_string(alloc.usb_start) + "-" + std::to_string(alloc.usb_end);
        } else {
            line += " (mono) → USB " + std::to_string(alloc.usb_start);
        }
        line += "\n";
        Log(line.c_str());
    }
    Log("\n");
    
    // Proceed directly to configuration (no confirmation dialog)
    // Create a progress message
    Log("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n");
    Log("CONFIGURING WING CONSOLE FOR VIRTUAL SOUNDCHECK\n");
    Log("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n");
    Log("\n");
    
    // Apply the USB allocation: configure USB outputs (Wing -> REAPER) and ALT sources (REAPER -> Wing)
    Log("Step 1/2: Configuring Wing USB outputs and ALT sources...\n");
    // Query USR routing data before configuring USB (to resolve routing chains)
    osc_handler_->QueryUserSignalInputs(48);
    osc_handler_->ApplyUSBAllocationAsAlt(allocations, selected_channels);
    
    Log("\n");
    Log("Step 2/2: Creating REAPER tracks...\n");
    
    // Create REAPER tracks for recording/playback
    // Build lookup map from channel number to channel info
    std::map<int, ChannelInfo> channel_map;
    for (const auto& ch : selected_channels) {
        channel_map[ch.channel_number] = ch;
    }
    
    // Clear existing tracks and create new ones
    Undo_BeginBlock();
    
    int track_index = 0;
    int created_tracks = 0;
    for (const auto& alloc : allocations) {
        if (!alloc.allocation_note.empty() && alloc.allocation_note.find("ERROR") != std::string::npos) {
            continue;  // Skip errored allocations
        }
        
        // Find channel info
        auto it = channel_map.find(alloc.channel_number);
        if (it == channel_map.end()) {
            continue;
        }
        const ChannelInfo& ch_info = it->second;
        
        // Create track (mono or stereo)
        MediaTrack* track = nullptr;
        if (alloc.is_stereo) {
            track = track_manager_->CreateStereoTrack(track_index, ch_info.name, ch_info.color);
            // Set stereo USB input (USB channels are 1-based, REAPER inputs are 0-based)
            if (track) {
                track_manager_->SetTrackInput(track, alloc.usb_start - 1, 2);
                std::string msg = "  ✓ Track " + std::to_string(created_tracks + 1) + 
                                 ": " + ch_info.name + " (stereo) → USB " + 
                                 std::to_string(alloc.usb_start) + "-" + std::to_string(alloc.usb_end) + "\n";
                Log(msg.c_str());
            }
        } else {
            track = track_manager_->CreateTrack(track_index, ch_info.name, ch_info.color);
            // Set mono USB input
            if (track) {
                track_manager_->SetTrackInput(track, alloc.usb_start - 1, 1);
                std::string msg = "  ✓ Track " + std::to_string(created_tracks + 1) + 
                                 ": " + ch_info.name + " (mono) → USB " + 
                                 std::to_string(alloc.usb_start) + "\n";
                Log(msg.c_str());
            }
        }
        
        if (track) {
            track_index++;
            created_tracks++;
        }
    }
    
    Undo_EndBlock("Wing Connector: Configure Virtual Soundcheck", UNDO_STATE_TRACKCFG);
        
        Log("\n");
        Log("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n");
        Log("✓ CONFIGURATION COMPLETE\n");
        Log("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n");
        std::string final_msg = "Created " + std::to_string(created_tracks) + " REAPER tracks\n";
        Log(final_msg.c_str());
        Log("\nNext: Use 'Toggle Soundcheck Mode' to enable/disable ALT sources.\n");
        Log("When enabled, channels receive audio from REAPER via USB.\n");
        Log("\n");
        
        // Build success message
        std::ostringstream success_msg;
        success_msg << "Virtual Soundcheck configured successfully!\n\n"
                    << "Created " << created_tracks << " REAPER tracks.\n\n"
                    << "Use 'Toggle Soundcheck Mode' to enable/disable ALT sources.\n"
                    << "When enabled, channels receive audio from REAPER via USB.\n\n"
                    << "Details are shown in the REAPER console.";
        
        ShowMessageBox(
            success_msg.str().c_str(),
            "Wing Connector - Success",
            0
        );
}

void ReaperExtension::ToggleSoundcheckMode() {
    if (!connected_ || !osc_handler_) {
        ShowMessageBox(
            "Please connect to Wing console first",
            "Wing Connector - Soundcheck Mode",
            0
        );
        return;
    }
    
    // Toggle the state
    soundcheck_mode_enabled_ = !soundcheck_mode_enabled_;
    
    // Apply to all channels
    osc_handler_->SetAllChannelsAltEnabled(soundcheck_mode_enabled_);
    
    if (soundcheck_mode_enabled_) {
        Log("Wing Connector: Soundcheck Mode ENABLED - Channels using USB input from REAPER\n");
        ShowMessageBox(
            "Soundcheck Mode ENABLED\n\n"
            "Channels are now receiving audio from REAPER via USB.\n"
            "You can now perform a virtual soundcheck.",
            "Wing Connector - Soundcheck Mode",
            0
        );
        status_message_ = "Soundcheck Mode ON";
    } else {
        Log("Wing Connector: Soundcheck Mode DISABLED - Channels using primary sources\n");
        ShowMessageBox(
            "Soundcheck Mode DISABLED\n\n"
            "Channels have returned to their primary input sources.\n"
            "Ready for live recording.",
            "Wing Connector - Soundcheck Mode",
            0
        );
        status_message_ = "Soundcheck Mode OFF";
    }
}

void ReaperExtension::OnChannelDataReceived(const ChannelInfo& channel) {
    if (monitoring_enabled_) {
        // In monitoring mode, update tracks in real-time
        auto existing_tracks = track_manager_->FindExistingWingTracks();
        
        // Find track for this channel and update it
        if ((size_t)channel.channel_number <= existing_tracks.size()) {
            MediaTrack* track = existing_tracks[channel.channel_number - 1];
            track_manager_->UpdateTrack(track, channel);
        }
    }
}

// ============================================================================
// MIDI Action Mapping
// ============================================================================

void ReaperExtension::EnableWingMidiDevice() {
    int num_inputs = GetNumMIDIInputs();
    bool found_wing = false;
    
    char msg[1024];
    snprintf(msg, sizeof(msg), 
             "Checking for Wing MIDI device...\n"
             "Found %d MIDI input device(s):\n", num_inputs);
    Log(msg);
    
    for (int i = 0; i < num_inputs; i++) {
        char device_name[256];
        
        if (GetMIDIInputName(i, device_name, sizeof(device_name))) {
            // Log all devices
            snprintf(msg, sizeof(msg), "  [%d] %s\n", i, device_name);
            Log(msg);
            
            // Look for Wing device (case-insensitive search)
            std::string name_lower = device_name;
            std::transform(name_lower.begin(), name_lower.end(), name_lower.begin(), ::tolower);
            
            if (name_lower.find("wing") != std::string::npos) {
                found_wing = true;
                snprintf(msg, sizeof(msg), 
                         "\n✓ Wing MIDI device detected: %s\n"
                         "⚠ Make sure it's ENABLED in: Preferences → Audio → MIDI Devices\n"
                         "  (Enable 'Input' checkbox for this device)\n\n", 
                         device_name);
                Log(msg);
            }
        }
    }
    
    if (!found_wing) {
        Log("\n⚠ Warning: No Wing MIDI device found!\n"
            "For MIDI actions to work:\n"
            "1. Connect Wing to computer via USB or network MIDI\n"
            "2. Enable device in: Preferences → Audio → MIDI Devices\n\n");
    }
}

void ReaperExtension::RegisterMidiShortcuts() {
    Log("\n=== Wing MIDI Actions Configuration ===\n\n");
    Log("APPROACH: Programmatic MIDI shortcut assignment\n\n");
    
    // Get path to reaper-kb.ini
    const char* resource_path = GetResourcePath();
    if (!resource_path) {
        Log("✗ ERROR: Could not get REAPER resource path\n");
        Log("GetResourcePath() returned nullptr - REAPER may not be fully initialized\n");
        return;
    }
    
    std::string kb_ini_path = std::string(resource_path) + "/reaper-kb.ini";
    
    char debug_msg[512];
    snprintf(debug_msg, sizeof(debug_msg), "Resource path: %s\n", resource_path);
    Log(debug_msg);
    snprintf(debug_msg, sizeof(debug_msg), "KB.ini path: %s\n", kb_ini_path.c_str());
    Log(debug_msg);
    
    // Check if file exists and is writable
    std::ofstream test_file(kb_ini_path, std::ios::app);
    if (!test_file.is_open()) {
        snprintf(debug_msg, sizeof(debug_msg), "✗ ERROR: Cannot open %s for writing\n", kb_ini_path.c_str());
        Log(debug_msg);
        return;
    }
    test_file.close();
    
    Log("\nConfigured mappings:\n");
    for (const auto& action : MIDI_ACTIONS) {
        char msg[256];
        snprintf(msg, sizeof(msg), 
                 "  CC#%d (Ch 1) → %s\n", 
                 action.cc_number, action.description + 6);  // Skip "Wing: " prefix
        Log(msg);
        
        // Add MIDI shortcut to reaper-kb.ini
        // Format: KEY <status_byte> <data1_encoded> <action_id> <param>
        // Status byte: 0xB0 (176) = CC on channel 1
        // Data1 is stored with +128 offset by REAPER: (cc_number + 128)
        // Example: KEY 176 148 40157 0  (CC#20 → Insert marker, stored as 20+128=148)
        
        int cc_encoded = action.cc_number + 128;  // REAPER stores CC numbers with +128 offset
        std::string shortcut_line = "KEY 176 " + std::to_string(cc_encoded) + 
                                   " " + std::to_string(action.command_id) + " 0\n";
        
        // Append to reaper-kb.ini
        std::ofstream kb_file(kb_ini_path, std::ios::app);
        if (kb_file.is_open()) {
            kb_file << shortcut_line;
            kb_file.close();
            snprintf(debug_msg, sizeof(debug_msg), "  ✓ Wrote: %s", shortcut_line.c_str());
            Log(debug_msg);
        } else {
            snprintf(debug_msg, sizeof(debug_msg), "  ✗ Failed to write CC#%d mapping\n", action.cc_number);
            Log(debug_msg);
        }
    }
    
    Log("\n✓ MIDI shortcuts written to reaper-kb.ini\n");
    
    // Touch the file to update modification time
    // This triggers REAPER's file watcher to reload keyboard shortcuts
    // without requiring a restart
    time_t now = time(nullptr);
    struct utimbuf times = {now, now};
    if (utime(kb_ini_path.c_str(), &times) == 0) {
        Log("✓ Updated reaper-kb.ini modification time\n");
        Log("✓ REAPER should reload shortcuts automatically\n\n");
    } else {
        Log("⚠ Could not update file modification time (REAPER will reload on next window focus)\n\n");
    }
    
    Log("To verify:\n");
    Log("  1. The Wing MIDI buttons should work immediately\n");
    Log("  2. Or: Enable Wing MIDI in Preferences → MIDI Devices\n");
    Log("  3. Press Wing buttons - actions should execute\n\n");
}

void ReaperExtension::UnregisterMidiShortcuts() {
    const char* resource_path = GetResourcePath();
    if (!resource_path) return;
    
    std::string kb_ini_path = std::string(resource_path) + "/reaper-kb.ini";
    std::ifstream in_file(kb_ini_path);
    std::string content;
    std::string line;
    
    // Read file and remove Wing MIDI lines
    while (std::getline(in_file, line)) {
        bool is_wing_midi = false;
        for (const auto& action : MIDI_ACTIONS) {
            int cc_encoded = action.cc_number + 128;  // Use same encoding as register
            std::string wing_line = "KEY 176 " + std::to_string(cc_encoded);
            if (line.find(wing_line) != std::string::npos) {
                is_wing_midi = true;
                break;
            }
        }
        if (!is_wing_midi) {
            content += line + "\n";
        }
    }
    in_file.close();
    
    // Write back
    std::ofstream out_file(kb_ini_path);
    out_file << content;
    out_file.close();
    
    // Touch the file to update modification time
    // This triggers REAPER's file watcher to reload keyboard shortcuts
    time_t now = time(nullptr);
    struct utimbuf times = {now, now};
    if (utime(kb_ini_path.c_str(), &times) == 0) {
        Log("✓ MIDI shortcuts removed and reloader triggered\n");
    } else {
        Log("MIDI shortcuts removed from reaper-kb.ini\n");
    }
}

void ReaperExtension::EnableMidiActions(bool enable) {
    if (midi_actions_enabled_ == enable) {
        return;  // Already in desired state
    }
    
    midi_actions_enabled_ = enable;
    
    if (enable) {
        RegisterMidiShortcuts();
    } else {
        UnregisterMidiShortcuts();
    }
    
    //Update config
    config_.configure_midi_actions = enable;
    config_.SaveToFile(WingConfig::GetConfigPath());
}

bool ReaperExtension::AreMidiShortcutsRegistered() const {
    const char* resource_path = GetResourcePath();
    if (!resource_path) return false;
    
    std::string kb_ini_path = std::string(resource_path) + "/reaper-kb.ini";
    std::ifstream kb_file(kb_ini_path);
    if (!kb_file.is_open()) return false;
    
    std::string line;
    int found_count = 0;
    
    // Check for Wing MIDI shortcuts (CC 20-26 encoded as 148-154)
    while (std::getline(kb_file, line)) {
        // Look for our MIDI shortcuts with the +128 offset
        if (line.find("KEY 176 148") != std::string::npos ||  // CC 20
            line.find("KEY 176 149") != std::string::npos ||  // CC 21
            line.find("KEY 176 150") != std::string::npos ||  // CC 22
            line.find("KEY 176 151") != std::string::npos ||  // CC 23
            line.find("KEY 176 152") != std::string::npos ||  // CC 24
            line.find("KEY 176 153") != std::string::npos ||  // CC 25
            line.find("KEY 176 154") != std::string::npos) {  // CC 26
            found_count++;
        }
    }
    kb_file.close();
    
    // Return true if we found all 7 shortcuts
    return found_count >= 7;
}

bool ReaperExtension::MidiInputHook(bool is_midi, const unsigned char* data, int len, int dev_id) {
    // Get instance
    auto& ext = ReaperExtension::Instance();
    
    // Log EVERY call to both file and instance log for debugging
    static int call_count = 0;
    call_count++;
    
    FILE* log_f = fopen("/tmp/wing_connector_debug.log", "a");
    if (log_f) {
        fprintf(log_f, "[HOOK CALLED #%d] is_midi=%d, len=%d, dev_id=%d\n", 
                call_count, is_midi, len, dev_id);
        fflush(log_f);
        fclose(log_f);
    }
    
    if (call_count <= 5 || call_count % 100 == 0) {  // Log first 5 calls to UI, then every 100th
        char debug[128];
        snprintf(debug, sizeof(debug), "[HOOK CALLED #%d] is_midi=%d, len=%d, dev_id=%d\n", 
                 call_count, is_midi, len, dev_id);
        ext.Log(debug);
    }
    
    // Only process MIDI messages
    if (!is_midi || len < 3) {
        return false;  // Pass through
    }
    
    if (!ext.midi_actions_enabled_) {
        return false;  // Pass through
    }
    
    ext.ProcessMidiInput(data, len);
    return false;  // Always pass through (don't consume the MIDI)
}

void ReaperExtension::ProcessMidiInput(const unsigned char* data, int len) {
    if (len < 3) return;
    
    unsigned char status = data[0];
    unsigned char cc_num = data[1];
    unsigned char value = data[2];
    
    // Debug: Log ALL MIDI messages to help diagnose issues
    if (log_callback_) {
        char debug_msg[256];
        snprintf(debug_msg, sizeof(debug_msg), 
                 "[MIDI DEBUG] Status: 0x%02X, CC: %d, Value: %d\n", 
                 status, cc_num, value);
        Log(debug_msg);
    }
    
    // Check for Control Change on Channel 1 (0xB0)
    if (status != 0xB0) {
        return;  // Not CC on channel 1
    }
    
    // Only respond to button press (value > 0), ignore release
    if (value == 0) {
        return;
    }
    
    // Map CC numbers to REAPER command IDs
    int command_id = 0;
    const char* action_name = nullptr;
    
    switch (cc_num) {
        case 20:  // Set Marker
            command_id = 40157;  // Markers: Insert marker at current position
            action_name = "Set Marker";
            break;
        case 21:  // Previous Marker
            command_id = 40172;  // Markers: Go to previous marker/project start
            action_name = "Previous Marker";
            break;
        case 22:  // Next Marker
            command_id = 40173;  // Markers: Go to next marker/project end
            action_name = "Next Marker";
            break;
        case 23:  // Record
            command_id = 1013;   // Transport: Record
            action_name = "Record";
            break;
        case 24:  // Stop
            command_id = 1016;   // Transport: Stop
            action_name = "Stop";
            break;
        case 25:  // Play
            command_id = 1007;   // Transport: Play
            action_name = "Play";
            break;
        case 26:  // Pause
            command_id = 1008;   // Transport: Pause
            action_name = "Pause";
            break;
        default:
            return;  // Not one of our mapped CCs
    }
    
    // Execute REAPER command
    if (command_id != 0) {
        Main_OnCommand(command_id, 0);
        
        // Log action (only if callback is set to avoid spam)
        if (log_callback_) {
            char msg[128];
            snprintf(msg, sizeof(msg), "MIDI Action: %s (CC#%d)\n", action_name, cc_num);
            Log(msg);
        }
    }
}

}  // namespace WingConnector
