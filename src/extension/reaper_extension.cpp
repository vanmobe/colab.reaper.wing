/*
 * Reaper Extension Main Class Implementation
 */

#include <cstring>

#include "wingconnector/reaper_extension.h"
#include "reaper_plugin_functions.h"
#ifdef __APPLE__
#include "internal/settings_dialog_macos.h"
#endif
#include <chrono>
#include <thread>
#include <cstdlib>
#include <cstdio>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <set>
#include <ctime>
#include <cmath>
#ifdef _WIN32
#include <sys/utime.h>
// windows.h defines min/max macros that break std::min/std::max usage.
#ifdef min
#undef min
#endif
#ifdef max
#undef max
#endif
#else
#include <utime.h>
#endif
#include "osc/OscOutboundPacketStream.h"
#include "ip/UdpSocket.h"

// C-style wrapper function for MIDI hook (REAPER requires a C function, not a member function)
extern "C" bool WingMidiInputHookWrapper(bool is_midi, const unsigned char* data, int len, int dev_id) {
    return WingConnector::ReaperExtension::MidiInputHook(is_midi, data, len, dev_id);
}

namespace WingConnector {

namespace {
constexpr int kChannelQueryAttempts = 2;
constexpr int kQueryResponseWaitMs = 600;  // Wait time for OSC responses after sending all queries
constexpr int kReaperPlayStateRecordingBit = 4;

void SendOscMessage(const std::string& host, int port, const std::string& address, int value = 1) {
    if (host.empty() || port <= 0 || address.empty()) {
        return;
    }
    try {
        char buffer[256];
        osc::OutboundPacketStream p(buffer, 256);
        p << osc::BeginMessage(address.c_str()) << (int32_t)value << osc::EndMessage;
        UdpTransmitSocket tx(IpEndpointName(host.c_str(), static_cast<uint16_t>(port)));
        tx.Send(p.Data(), p.Size());
    } catch (...) {
        // Never let OSC notification failures break transport control flow.
    }
}

void SendOscToWing(const WingConfig& cfg, const std::string& address, int value = 1) {
    SendOscMessage(cfg.wing_ip, 2223, address, value);
}

bool TouchFile(const std::string& path) {
    time_t now = time(nullptr);
#ifdef _WIN32
    struct _utimbuf times = {now, now};
    return _utime(path.c_str(), &times) == 0;
#else
    struct utimbuf times = {now, now};
    return utime(path.c_str(), &times) == 0;
#endif
}
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
            Log("AUDIOLAB.wing.reaper.virtualsoundcheck: Using default configuration\n");
        }
    } else {
        fprintf(stderr, "🔧 [WING] Configuration loaded successfully\n");
        fflush(stderr);
        Log("AUDIOLAB.wing.reaper.virtualsoundcheck: Configuration loaded\n");
        
        bool config_updated = false;
        
        // Migrate legacy default listen port (2224 -> 2223)
        if (config_.listen_port == 2224) {
            config_.listen_port = 2223;
            config_updated = true;
            Log("AUDIOLAB.wing.reaper.virtualsoundcheck: Updated listener port to 2223\n");
        }
        
        // Auto-enable MIDI actions if not configured
        if (!config_.configure_midi_actions) {
            config_.configure_midi_actions = true;
            config_updated = true;
            Log("AUDIOLAB.wing.reaper.virtualsoundcheck: Auto-enabled MIDI action mapping for Wing buttons\n");
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

    if (g_rec_) {
        g_rec_->Register("timer", (void*)ReaperExtension::MainThreadTimerTick);
    }
    
    return true;
}

void ReaperExtension::Shutdown() {
    if (g_rec_) {
        g_rec_->Register("-timer", (void*)ReaperExtension::MainThreadTimerTick);
    }
    StopAutoRecordMonitor();
    EnableMidiActions(false);
    DisconnectFromWing();
    track_manager_.reset();
}

void ReaperExtension::MainThreadTimerTick() {
    auto& ext = ReaperExtension::Instance();
    if (ext.pending_record_start_.exchange(false)) {
        Main_OnCommand(1013, 0);  // Transport: Record (main thread)
    }
    if (ext.pending_record_stop_.exchange(false)) {
        Main_OnCommand(1016, 0);  // Transport: Stop (main thread)
    }
}

// Connects and verifies OSC reachability only; track creation is user-driven.
bool ReaperExtension::ConnectToWing() {
    if (connected_) {
        Log("AUDIOLAB.wing.reaper.virtualsoundcheck: Already connected\n");
        return true;
    }
    
    Log("AUDIOLAB.wing.reaper.virtualsoundcheck: Connecting to Wing...\n");
    status_message_ = "Connecting...";
    
    // Wing OSC is fixed to 2223.
    config_.wing_port = 2223;
    config_.listen_port = 2223;

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
        Log("AUDIOLAB.wing.reaper.virtualsoundcheck: Failed to start OSC server. Port may be in use.\n");
        osc_handler_.reset();
        status_message_ = "Failed to start";
        return false;
    }
    
    // Test connection
    if (!osc_handler_->TestConnection()) {
        Log("AUDIOLAB.wing.reaper.virtualsoundcheck: Could not connect to Wing console. Check IP and OSC settings.\n");
        osc_handler_->Stop();
        osc_handler_.reset();
        status_message_ = "Connection failed";
        return false;
    }
    
    Log("AUDIOLAB.wing.reaper.virtualsoundcheck: Connected!\n");
    connected_ = true;
    status_message_ = "Connected";
    StartAutoRecordMonitor();
    
    // Query console info
    const auto& wing_info = osc_handler_->GetWingInfo();
    if (!wing_info.model.empty()) {
        char info_msg[256];
        snprintf(info_msg, sizeof(info_msg),
                 "AUDIOLAB.wing.reaper.virtualsoundcheck: Detected %s (%s) FW %s\n",
                 wing_info.model.c_str(),
                 wing_info.name.empty() ? "Unnamed" : wing_info.name.c_str(),
                 wing_info.firmware.empty() ? "unknown" : wing_info.firmware.c_str());
        Log(info_msg);
    }

    if (config_.sd_lr_route_enabled) {
        RouteMainLRToCardForSDRecording();
    }
    
    return true;
}

// Get available channels with sources assigned
std::vector<ChannelSelectionInfo> ReaperExtension::GetAvailableChannels() {
    std::vector<ChannelSelectionInfo> result;
    
    if (!connected_ || !osc_handler_) {
        Log("AUDIOLAB.wing.reaper.virtualsoundcheck: Not connected. Cannot query channels.\n");
        return result;
    }
    
    Log("AUDIOLAB.wing.reaper.virtualsoundcheck: Querying channels...\n");
    
    // Query all channels with retries
    const auto query_delay = std::chrono::milliseconds(kQueryResponseWaitMs);
    for (int attempt = 1; attempt <= kChannelQueryAttempts; ++attempt) {
        char attempt_msg[128];
        snprintf(attempt_msg, sizeof(attempt_msg),
                 "AUDIOLAB.wing.reaper.virtualsoundcheck: Querying channels (attempt %d/%d)\n",
                 attempt, kChannelQueryAttempts);
        Log(attempt_msg);
        osc_handler_->QueryAllChannels(config_.channel_count);
        std::this_thread::sleep_for(query_delay);
        if (!osc_handler_->GetChannelData().empty()) {
            break;
        }
        if (attempt < kChannelQueryAttempts) {
            Log("AUDIOLAB.wing.reaper.virtualsoundcheck: No channel data yet, retrying...\n");
        }
    }
    
    // Get channel data
    const auto& channel_data = osc_handler_->GetChannelData();
    if (channel_data.empty()) {
        Log("AUDIOLAB.wing.reaper.virtualsoundcheck: No channel data received. Check timeout settings.\n");
        return result;
    }

    // Query USR routing so popup can display resolved sources (e.g. USR:25 -> A:8)
    osc_handler_->QueryUserSignalInputs(48);
    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    // Query source labels for A-inputs used by selected channels (for name fallback).
    std::set<std::pair<std::string, int>> source_endpoints;
    for (const auto& pair : channel_data) {
        const ChannelInfo& ch = pair.second;
        if (ch.primary_source_group.empty() || ch.primary_source_input <= 0) {
            continue;
        }
        auto resolved = osc_handler_->ResolveRoutingChain(ch.primary_source_group, ch.primary_source_input);
        source_endpoints.insert(resolved);
        // For stereo sources, also fetch the name of the partner input (source_input + 1)
        if (ch.stereo_linked) {
            auto partner_resolved = osc_handler_->ResolveRoutingChain(ch.primary_source_group, ch.primary_source_input + 1);
            source_endpoints.insert(partner_resolved);
        }
    }
    osc_handler_->QueryInputSourceNames(source_endpoints);
    
    Log("AUDIOLAB.wing.reaper.virtualsoundcheck: Processing channel data...\n");
    // stereo_linked is set from /io/in/{grp}/{num}/mode by the second pass in QueryAllChannels.
    // No heuristics needed.
    
    // Build list of channels with sources.
    // On Behringer Wing: stereo is SOURCE-based, not channel-based.
    // A channel is stereo if its source is stereo. That's it - no pairing needed.
    
    for (const auto& pair : channel_data) {
        const ChannelInfo& ch = pair.second;
        
        if (ch.primary_source_group.empty()) {
            continue;
        }

        ChannelSelectionInfo info;
        info.channel_number = ch.channel_number;
        info.name = ch.name;
        info.source_group = ch.primary_source_group;
        info.source_input = ch.primary_source_input;
        
        // stereo if source mode was "ST" or "MS" (set by QueryChannelSourceStereo)
        bool is_stereo = ch.stereo_linked;
        
        // Resolve the source to final endpoint
        auto resolved = osc_handler_->ResolveRoutingChain(info.source_group, info.source_input);
        info.source_group = resolved.first;
        info.source_input = resolved.second;

        // For stereo sources, the partner is always source_input+1 in the same group
        if (is_stereo) {
            info.partner_source_group = info.source_group;
            info.partner_source_input = info.source_input + 1;
        }

        if (ch.name.empty()) {
            std::string src_name = osc_handler_->GetInputSourceName(info.source_group, info.source_input);
            if (!src_name.empty()) {
                info.name = src_name;
            }
        }

        info.stereo_linked = is_stereo;
        info.selected = !info.name.empty() && info.name.rfind("CH", 0) != 0;

        result.push_back(info);
    }
    
    char msg[128];
    snprintf(msg, sizeof(msg), "AUDIOLAB.wing.reaper.virtualsoundcheck: Found %d channels with sources\n", 
             (int)result.size());
    Log(msg);
    
    return result;
}

// Create tracks from selected channels
void ReaperExtension::CreateTracksFromSelection(const std::vector<ChannelSelectionInfo>& channels) {
    if (!connected_ || !osc_handler_) {
        Log("AUDIOLAB.wing.reaper.virtualsoundcheck: Not connected\n");
        return;
    }
    
    Log("AUDIOLAB.wing.reaper.virtualsoundcheck: Creating tracks from selection...\n");
    
    // Filter to only selected channels
    std::vector<ChannelSelectionInfo> selected;
    for (const auto& ch : channels) {
        if (ch.selected) {
            selected.push_back(ch);
        }
    }
    
    if (selected.empty()) {
        Log("AUDIOLAB.wing.reaper.virtualsoundcheck: No channels selected\n");
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
    snprintf(msg, sizeof(msg), "AUDIOLAB.wing.reaper.virtualsoundcheck: Created %d tracks\n", track_count);
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

bool ReaperExtension::ValidateLiveRecordingSetup(std::string& details) {
    if (!connected_ || !osc_handler_) {
        details = "Not connected to Wing.";
        return false;
    }

    // Refresh channel/ALT state from the console before validating.
    osc_handler_->QueryAllChannels(config_.channel_count);
    std::this_thread::sleep_for(std::chrono::milliseconds(kQueryResponseWaitMs));

    const auto& channel_data = osc_handler_->GetChannelData();
    if (channel_data.empty()) {
        details = "No channel data received from Wing.";
        return false;
    }

    const bool card_mode = (config_.soundcheck_output_mode == "CARD");
    std::set<std::string> accepted_alt_groups;
    if (card_mode) {
        accepted_alt_groups.insert("CARD");
        accepted_alt_groups.insert("CRD");
    } else {
        accepted_alt_groups.insert("USB");
    }

    // Gather channels that are wired for live/soundcheck switching.
    std::set<int> expected_track_inputs_1based;
    int routable_channels = 0;
    int alt_configured_channels = 0;
    for (const auto& [ch_num, ch] : channel_data) {
        (void)ch_num;
        if (ch.primary_source_group.empty() || ch.primary_source_group == "OFF" || ch.primary_source_input <= 0) {
            continue;
        }
        routable_channels++;

        if (accepted_alt_groups.count(ch.alt_source_group) > 0 && ch.alt_source_input > 0) {
            alt_configured_channels++;
            expected_track_inputs_1based.insert(ch.alt_source_input);
        }
    }

    if (routable_channels == 0) {
        details = "Wing has no routable input channels.";
        return false;
    }

    if (expected_track_inputs_1based.empty()) {
        details = "Wing ALT sources are not configured for " + std::string(card_mode ? "CARD" : "USB") + ".";
        return false;
    }

    // Validate REAPER tracks against expected I/O mapping:
    // - Track record input should map to ALT input
    // - Track should have a matching hardware output send
    ReaProject* proj = EnumProjects(-1, nullptr, 0);
    if (!proj) {
        details = "No active REAPER project.";
        return false;
    }

    int matching_tracks = 0;
    std::set<int> matched_inputs_1based;
    const int track_count = CountTracks(proj);
    for (int i = 0; i < track_count; ++i) {
        MediaTrack* track = GetTrack(proj, i);
        if (!track) {
            continue;
        }

        int rec_input = (int)GetMediaTrackInfo_Value(track, "I_RECINPUT");
        if (rec_input < 0) {
            continue;
        }

        const int rec_input_index = rec_input & 0x3FF;  // 0-based device channel index
        const int rec_input_1based = rec_input_index + 1;
        if (expected_track_inputs_1based.count(rec_input_1based) == 0) {
            continue;
        }

        bool has_matching_hw_send = false;
        const int hw_send_count = GetTrackNumSends(track, 1);
        for (int s = 0; s < hw_send_count; ++s) {
            const int dst = (int)GetTrackSendInfo_Value(track, 1, s, "I_DSTCHAN");
            const int dst_index = dst & 0x3FF;  // mono/stereo encoded in high bits
            if (dst_index == rec_input_index) {
                has_matching_hw_send = true;
                break;
            }
        }

        if (!has_matching_hw_send) {
            continue;
        }

        matching_tracks++;
        matched_inputs_1based.insert(rec_input_1based);
    }

    if (matching_tracks == 0 || matched_inputs_1based.empty()) {
        details = "No REAPER tracks match Wing ALT input/hardware routing.";
        return false;
    }

    if (matched_inputs_1based.size() < expected_track_inputs_1based.size()) {
        std::ostringstream msg;
        msg << "Partial setup: matched " << matched_inputs_1based.size()
            << " of " << expected_track_inputs_1based.size()
            << " expected ALT-mapped input routes.";
        details = msg.str();
        return false;
    }

    std::ostringstream ok;
    ok << "Validated: " << alt_configured_channels << " Wing channels have ALT routing and "
       << matching_tracks << " REAPER tracks match live I/O routing.";
    details = ok.str();
    return true;
}

// Setup virtual soundcheck from selected channels
void ReaperExtension::SetupSoundcheckFromSelection(const std::vector<ChannelSelectionInfo>& channels, bool setup_soundcheck) {
    if (!connected_ || !osc_handler_) {
        Log("AUDIOLAB.wing.reaper.virtualsoundcheck: Not connected\n");
        return;
    }
    
    Log("AUDIOLAB.wing.reaper.virtualsoundcheck: Setting up Virtual Soundcheck...\n");
    
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
        Log("AUDIOLAB.wing.reaper.virtualsoundcheck: No channels selected\n");
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
        if (alloc.usb_end > required_io_channels) {
            required_io_channels = alloc.usb_end;
        }
    }

    const int available_inputs = GetNumAudioInputs();
    const int available_outputs = GetNumAudioOutputs();
    if (available_inputs < required_io_channels || available_outputs < required_io_channels) {
        std::ostringstream err;
        err << "AUDIOLAB.wing.reaper.virtualsoundcheck: REAPER audio device does not expose enough channels for "
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
        ShowMessageBox(msg.str().c_str(), "AUDIOLAB.wing.reaper.virtualsoundcheck - Audio I/O Not Available", 0);
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
    if (setup_soundcheck) {
        Log("Step 1/2: Configuring Wing " + output_type + " outputs and ALT sources...\n");
    } else {
        Log("Step 1/2: Configuring Wing " + output_type + " outputs (recording only, no soundcheck)...\n");
    }
    // Query USR routing data before configuring (to resolve routing chains)
    osc_handler_->QueryUserSignalInputs(48);
    osc_handler_->ApplyUSBAllocationAsAlt(allocations, selected_channels, output_mode, setup_soundcheck);
    
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
    
    Undo_EndBlock("AUDIOLAB.wing.reaper.virtualsoundcheck: Configure Virtual Soundcheck", UNDO_STATE_TRACKCFG);
    
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
    StopAutoRecordMonitor();
    
    Log("AUDIOLAB.wing.reaper.virtualsoundcheck: Disconnecting...\n");
    
    if (osc_handler_) {
        osc_handler_->Stop();
        osc_handler_.reset();
    }
    
    connected_ = false;
    monitoring_enabled_ = false;
    status_message_ = "Disconnected";
    
    Log("AUDIOLAB.wing.reaper.virtualsoundcheck: Disconnected\n");
}

std::vector<WingInfo> ReaperExtension::DiscoverWings(int timeout_ms) {
    return WingOSC::DiscoverWings(timeout_ms);
}

void ReaperExtension::RefreshTracks() {
    if (!connected_ || !osc_handler_) {
        ShowMessageBox(
            "Not connected to Wing console.\n"
            "Please connect first.",
            "AUDIOLAB.wing.reaper.virtualsoundcheck",
            0
        );
        return;
    }
    
    Log("AUDIOLAB.wing.reaper.virtualsoundcheck: Refreshing tracks...\n");
    
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
    #ifdef __APPLE__
    // Use native macOS dialog for settings
    char ip_buffer[256];
    
    strncpy(ip_buffer, config_.wing_ip.c_str(), sizeof(ip_buffer) - 1);
    ip_buffer[sizeof(ip_buffer) - 1] = '\0';
    
    // Show native Cocoa dialog
    if (ShowSettingsDialog(config_.wing_ip.c_str(),
                          ip_buffer, sizeof(ip_buffer))) {
        // Validate IP
        std::string new_ip = ip_buffer;
        if (new_ip.empty() || new_ip.length() > 15) {
            ShowMessageBox("Invalid IP address.\nPlease use format: 192.168.0.1", 
                          "AUDIOLAB.wing.reaper.virtualsoundcheck - Error", 0);
            return;
        }
        // Update configuration
        config_.wing_ip = new_ip;
        config_.wing_port = 2223;
        config_.listen_port = 2223;
        
        // Save to file
        const std::string config_path = WingConfig::GetConfigPath();
        if (config_.SaveToFile(config_path)) {
            char success_msg[256];
            snprintf(success_msg, sizeof(success_msg),
                "Settings saved successfully!\n\n"
                "IP: %s\n"
                "OSC Port: 2223\n\n"
                "Changes will apply on next connection.",
                config_.wing_ip.c_str());
            
            ShowMessageBox(success_msg, "AUDIOLAB.wing.reaper.virtualsoundcheck - Settings Saved", 0);
            Log("AUDIOLAB.wing.reaper.virtualsoundcheck: Settings updated from dialog\n");
        } else {
            char error_msg[512];
            snprintf(error_msg, sizeof(error_msg),
                      "Failed to save settings to:\n%s\n\nPlease check file permissions.",
                      config_path.c_str());
            ShowMessageBox(error_msg,
                          "AUDIOLAB.wing.reaper.virtualsoundcheck - Error", 0);
        }
    }
    #else
    // Fallback for non-macOS platforms
    char settings_msg[512];
    snprintf(settings_msg, sizeof(settings_msg), 
        "AUDIOLAB.wing.reaper.virtualsoundcheck Settings\n\n"
        "Current Configuration:\n"
        "  Wing IP: %s\n"
        "  OSC Port: 2223\n"
        "\nEdit config.json to change settings.\n",
        config_.wing_ip.c_str());
    
    ShowMessageBox(settings_msg, "AUDIOLAB.wing.reaper.virtualsoundcheck - Settings", 0);
    #endif
}

void ReaperExtension::EnableMonitoring(bool enable) {
    monitoring_enabled_ = enable;
    
    if (enable) {
        Log("AUDIOLAB.wing.reaper.virtualsoundcheck: Real-time monitoring enabled\n");
        status_message_ = "Monitoring active";
    } else {
        Log("AUDIOLAB.wing.reaper.virtualsoundcheck: Real-time monitoring disabled\n");
        status_message_ = "Monitoring inactive";
    }
}

int ReaperExtension::GetProjectTrackCount() const {
    ReaProject* proj = EnumProjects(-1, nullptr, 0);
    return proj ? CountTracks(proj) : 0;
}

void ReaperExtension::StartAutoRecordMonitor() {
    if (!config_.auto_record_enabled || auto_record_monitor_running_) {
        return;
    }
    auto_record_monitor_running_ = true;
    auto_record_monitor_thread_ = std::make_unique<std::thread>(&ReaperExtension::MonitorAutoRecordLoop, this);
    Log("AUDIOLAB.wing.reaper.virtualsoundcheck: Auto-record monitor started\n");
}

void ReaperExtension::StopAutoRecordMonitor() {
    if (!auto_record_monitor_running_) {
        return;
    }
    auto_record_monitor_running_ = false;
    if (auto_record_monitor_thread_ && auto_record_monitor_thread_->joinable()) {
        auto_record_monitor_thread_->join();
    }
    auto_record_monitor_thread_.reset();
    auto_record_started_by_plugin_ = false;
    StopWarningFlash();
    ClearLayerState();
    Log("AUDIOLAB.wing.reaper.virtualsoundcheck: Auto-record monitor stopped\n");
}

void ReaperExtension::ApplyAutoRecordSettings() {
    StopAutoRecordMonitor();
    if (connected_ && config_.auto_record_enabled) {
        StartAutoRecordMonitor();
    }
}

double ReaperExtension::GetMaxArmedTrackPeak() const {
    ReaProject* proj = EnumProjects(-1, nullptr, 0);
    if (!proj) {
        return 0.0;
    }

    const int track_count = CountTracks(proj);
    double max_peak = 0.0;
    int start_i = 0;
    int end_i = track_count;
    if (config_.auto_record_monitor_track > 0) {
        start_i = std::min(track_count, std::max(0, config_.auto_record_monitor_track - 1));
        end_i = std::min(track_count, start_i + 1);
    }

    const bool specific_track_mode = (config_.auto_record_monitor_track > 0);
    for (int i = start_i; i < end_i; ++i) {
        MediaTrack* track = GetTrack(proj, i);
        if (!track) {
            continue;
        }

        if (!specific_track_mode) {
            const int rec_arm = static_cast<int>(GetMediaTrackInfo_Value(track, "I_RECARM"));
            const int rec_mon = static_cast<int>(GetMediaTrackInfo_Value(track, "I_RECMON"));
            if (rec_arm <= 0 || rec_mon <= 0) {
                continue;
            }
        }

        const double peak_l = Track_GetPeakInfo(track, 0);
        const double peak_r = Track_GetPeakInfo(track, 1);
        max_peak = std::max(max_peak, std::max(peak_l, peak_r));
    }
    return max_peak;
}

void ReaperExtension::MonitorAutoRecordLoop() {
    const double threshold_lin = std::pow(10.0, config_.auto_record_threshold_db / 20.0);
    const auto poll_interval = std::chrono::milliseconds(std::max(10, config_.auto_record_poll_ms));
    const auto attack_needed = std::chrono::milliseconds(std::max(50, config_.auto_record_attack_ms));
    const auto hold_needed = std::chrono::milliseconds(std::max(0, config_.auto_record_hold_ms));
    const auto release_needed = std::chrono::milliseconds(std::max(100, config_.auto_record_release_ms));
    const auto min_record_time = std::chrono::milliseconds(std::max(0, config_.auto_record_min_record_ms));

    auto above_since = std::chrono::steady_clock::time_point{};
    auto below_since = std::chrono::steady_clock::time_point{};
    auto record_started_at = std::chrono::steady_clock::time_point{};
    auto last_signal_at = std::chrono::steady_clock::time_point{};
    auto last_warning_event_at = std::chrono::steady_clock::time_point{};

    while (auto_record_monitor_running_) {
        const auto now = std::chrono::steady_clock::now();
        double peak = GetMaxArmedTrackPeak();
        const bool above = peak >= threshold_lin;
        const bool is_recording = (GetPlayState() & kReaperPlayStateRecordingBit) != 0;
        if (above) {
            last_signal_at = now;
        }

        if (!is_recording) {
            if (above) {
                if (above_since.time_since_epoch().count() == 0) {
                    above_since = now;
                } else if (now - above_since >= attack_needed) {
                    if (!warning_flash_running_) {
                        StartWarningFlash();
                        SetWarningLayerState();
                        Log("AUDIOLAB.wing.reaper.virtualsoundcheck: Signal above threshold long enough - starting CC flash warning\n");
                    }
                    if (config_.auto_record_warning_only) {
                        const bool hold_active = last_warning_event_at.time_since_epoch().count() != 0 &&
                                                 (now - last_warning_event_at < hold_needed);
                        if (hold_active) {
                            above_since = {};
                            below_since = {};
                            std::this_thread::sleep_for(poll_interval);
                            continue;
                        }
                        last_warning_event_at = now;
                        Log("AUDIOLAB.wing.reaper.virtualsoundcheck: WARNING trigger (signal above threshold)\n");
                        SendOscToWing(config_, config_.osc_warning_path, 1);
                    } else {
                        if (warning_flash_running_) {
                            Log("AUDIOLAB.wing.reaper.virtualsoundcheck: Recording started - stopping CC flash warning\n");
                            StopWarningFlash();
                        }
                        SetRecordingLayerState();
                        pending_record_start_ = true;
                        auto_record_started_by_plugin_ = true;
                        record_started_at = std::chrono::steady_clock::now();
                        if (config_.sd_auto_record_with_reaper && osc_handler_) {
                            ApplySDRoutingNoDialog();
                            osc_handler_->StartSDRecorder();
                        }
                        SendOscToWing(config_, config_.osc_start_path, 1);
                    }
                    above_since = {};
                    below_since = {};
                    Log("AUDIOLAB.wing.reaper.virtualsoundcheck: Trigger fired (signal above threshold)\n");
                }
            } else {
                above_since = {};
                const bool hold_expired = last_signal_at.time_since_epoch().count() == 0 ||
                                          (now - last_signal_at >= hold_needed);
                if (warning_flash_running_ && hold_expired && !manual_flash_override_) {
                    Log("AUDIOLAB.wing.reaper.virtualsoundcheck: Signal dropped below threshold - stopping CC flash warning\n");
                    StopWarningFlash();
                    ClearLayerState();
                }
            }
        } else {
            StopWarningFlash();
            SetRecordingLayerState();
            if (!auto_record_started_by_plugin_) {
                // Respect manual recording state and avoid auto-stop in this case.
                above_since = {};
                below_since = {};
                std::this_thread::sleep_for(poll_interval);
                continue;
            }

            if (above) {
                below_since = {};
            } else {
                if (below_since.time_since_epoch().count() == 0) {
                    below_since = now;
                } else if (now - below_since >= release_needed &&
                           (last_signal_at.time_since_epoch().count() == 0 || now - last_signal_at >= hold_needed) &&
                           now - record_started_at >= min_record_time) {
                    pending_record_stop_ = true;
                    if (config_.sd_auto_record_with_reaper && osc_handler_) {
                        osc_handler_->StopSDRecorder();
                    }
                    SendOscToWing(config_, config_.osc_stop_path, 0);
                    auto_record_started_by_plugin_ = false;
                    ClearLayerState();
                    above_since = {};
                    below_since = {};
                    Log("AUDIOLAB.wing.reaper.virtualsoundcheck: Auto-record stopped (signal below threshold)\n");
                }
            }
        }

        std::this_thread::sleep_for(poll_interval);
    }
}

void ReaperExtension::SetWarningLayerState() {
    if (!osc_handler_) {
        return;
    }
    if (layer_state_mode_.load() == 1) {
        return;
    }
    layer_state_mode_ = 1;
    const int layer = std::min(16, std::max(1, config_.warning_flash_cc_layer));
    osc_handler_->SetActiveUserControlLayer(layer);
    for (int b = 1; b <= 4; ++b) {
        osc_handler_->QueryUserControlColor(layer, b);
    }
    osc_handler_->QueryUserControlRotaryText(layer, 1);
    osc_handler_->SetUserControlRotaryName(layer, 1, "RECORDING");
    osc_handler_->SetUserControlRotaryName(layer, 2, "NOT ENABLED");
    osc_handler_->SetUserControlRotaryName(layer, 3, "!!");
}

void ReaperExtension::SetRecordingLayerState() {
    if (!osc_handler_) {
        return;
    }
    if (layer_state_mode_.load() == 2) {
        return;
    }
    layer_state_mode_ = 2;
    const int layer = std::min(16, std::max(1, config_.warning_flash_cc_layer));
    osc_handler_->SetActiveUserControlLayer(layer);
    osc_handler_->QueryUserControlRotaryText(layer, 1);
    osc_handler_->SetUserControlRotaryName(layer, 1, "REAPER");
    osc_handler_->SetUserControlRotaryName(layer, 2, "STARTED");
    osc_handler_->SetUserControlRotaryName(layer, 3, "RECORDING");
    osc_handler_->SetUserControlRotaryName(layer, 4, "...");
    const int recording_color = 6; // Force green for recording
    for (int b = 1; b <= 4; ++b) {
        osc_handler_->SetUserControlColor(layer, b, recording_color);
        osc_handler_->SetUserControlLed(layer, b, true); // Steady on
    }
}

void ReaperExtension::ClearLayerState() {
    if (!osc_handler_) {
        return;
    }
    layer_state_mode_ = 0;
    const int layer = std::min(16, std::max(1, config_.warning_flash_cc_layer));
    for (int b = 1; b <= 4; ++b) {
        osc_handler_->SetUserControlLed(layer, b, false);
    }
    osc_handler_->SetUserControlRotaryName(layer, 1, "");
    osc_handler_->SetUserControlRotaryName(layer, 2, "");
    osc_handler_->SetUserControlRotaryName(layer, 3, "");
    osc_handler_->SetUserControlRotaryName(layer, 4, "");
}

void ReaperExtension::StartWarningFlash() {
    if (warning_flash_running_ || !osc_handler_) {
        return;
    }
    warning_flash_running_ = true;
    warning_flash_thread_ = std::make_unique<std::thread>(&ReaperExtension::WarningFlashLoop, this);
}

void ReaperExtension::StopWarningFlash(bool force) {
    if (manual_flash_override_ && !force) {
        return;
    }
    if (!warning_flash_running_) {
        return;
    }
    warning_flash_running_ = false;
    if (warning_flash_thread_ && warning_flash_thread_->joinable()) {
        warning_flash_thread_->join();
    }
    warning_flash_thread_.reset();
    if (osc_handler_) {
        const int layer = std::min(16, std::max(1, config_.warning_flash_cc_layer));
        for (int b = 1; b <= 4; ++b) {
            osc_handler_->SetUserControlLed(layer, b, false);
        }
    }
}

void ReaperExtension::WarningFlashLoop() {
    if (!osc_handler_) {
        warning_flash_running_ = false;
        return;
    }
    const int layer = std::min(16, std::max(1, config_.warning_flash_cc_layer));
    const int color = 9; // Warning is fixed red
    for (int b = 1; b <= 4; ++b) {
        osc_handler_->SetUserControlColor(layer, b, color);
    }

    const int sequence[] = {1, 2, 3, 4, 3, 2};
    size_t seq_idx = 0;
    while (warning_flash_running_) {
        const int active = sequence[seq_idx];
        for (int b = 1; b <= 4; ++b) {
            osc_handler_->SetUserControlLed(layer, b, b == active);
        }
        seq_idx = (seq_idx + 1) % (sizeof(sequence) / sizeof(sequence[0]));
        std::this_thread::sleep_for(std::chrono::milliseconds(120));
    }
}

void ReaperExtension::RouteMainLRToCardForSDRecording() {
    if (!connected_ || !osc_handler_) {
        ShowMessageBox(
            "Not connected to Wing console.\nPlease connect first.",
            "AUDIOLAB.wing.reaper.virtualsoundcheck",
            0
        );
        return;
    }

    const int left_input = std::max(1, config_.sd_lr_left_input);
    const int right_input = std::max(1, config_.sd_lr_right_input);
    const std::string group = config_.sd_lr_group.empty() ? "MAIN" : config_.sd_lr_group;

    Log("AUDIOLAB.wing.reaper.virtualsoundcheck: Routing Main LR to CARD 1/2 for SD recording...\n");
    osc_handler_->SetCardOutputSource(1, group, left_input);
    osc_handler_->SetCardOutputSource(2, group, right_input);
    osc_handler_->SetCardOutputName(1, "Main L");
    osc_handler_->SetCardOutputName(2, "Main R");

    const std::string msg = "Configured CARD outputs: 1=" + group + ":" + std::to_string(left_input) +
                            ", 2=" + group + ":" + std::to_string(right_input) + "\n";
    Log(msg);
    ShowMessageBox(
        "Configured CARD 1/2 from Main LR source.\nUse Wing SD recorder to start capture.",
        "AUDIOLAB.wing.reaper.virtualsoundcheck",
        0
    );
}

void ReaperExtension::ApplySDRoutingNoDialog() {
    if (!osc_handler_) {
        return;
    }
    const int left_input = std::max(1, config_.sd_lr_left_input);
    const int right_input = std::max(1, config_.sd_lr_right_input);
    const std::string group = config_.sd_lr_group.empty() ? "MAIN" : config_.sd_lr_group;
    osc_handler_->SetCardOutputSource(1, group, left_input);
    osc_handler_->SetCardOutputSource(2, group, right_input);
    osc_handler_->SetCardOutputName(1, "Main L");
    osc_handler_->SetCardOutputName(2, "Main R");
}

double ReaperExtension::ReadCurrentTriggerLevel() {
    return GetMaxArmedTrackPeak();
}

void ReaperExtension::SendWarningOscNow() {
    SendOscToWing(config_, config_.osc_warning_path, 1);
}

void ReaperExtension::SendStartOscNow() {
    SendOscToWing(config_, config_.osc_start_path, 1);
}

void ReaperExtension::SendStopOscNow() {
    SendOscToWing(config_, config_.osc_stop_path, 0);
}

void ReaperExtension::TestWarningFlash() {
    if (!osc_handler_) {
        Log("AUDIOLAB.wing.reaper.virtualsoundcheck: Cannot test CC flash while disconnected\n");
        return;
    }
    manual_flash_override_ = true;
    StartWarningFlash();
    Log("AUDIOLAB.wing.reaper.virtualsoundcheck: Test CC flash started\n");
    std::thread([this]() {
        std::this_thread::sleep_for(std::chrono::milliseconds(3000));
        manual_flash_override_ = false;
        StopWarningFlash(true);
        Log("AUDIOLAB.wing.reaper.virtualsoundcheck: Test CC flash stopped\n");
    }).detach();
}


void ReaperExtension::ConfigureVirtualSoundcheck() {
    if (!connected_ || !osc_handler_) {
        ShowMessageBox(
            "Please connect to Wing console first",
            "AUDIOLAB.wing.reaper.virtualsoundcheck - Virtual Soundcheck",
            0
        );
        return;
    }
    
    Log("AUDIOLAB.wing.reaper.virtualsoundcheck: Configuring Virtual Soundcheck...\n");
    
    // Get all channel data
    const auto& channels = osc_handler_->GetChannelData();
    if (channels.empty()) {
        ShowMessageBox(
            "No channel data available.\nPlease refresh tracks first.",
            "AUDIOLAB.wing.reaper.virtualsoundcheck - Virtual Soundcheck",
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
                        ShowConsoleMsg("AUDIOLAB.wing.reaper.virtualsoundcheck: Auto-including stereo partner CH");
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
            "AUDIOLAB.wing.reaper.virtualsoundcheck - No Channels Available",
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
        "AUDIOLAB.wing.reaper.virtualsoundcheck - Channel Selection",
        4  // Yes/No/Cancel
    );
    
    if (result == 0) {
        Log("AUDIOLAB.wing.reaper.virtualsoundcheck: Virtual Soundcheck configuration cancelled\n");
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
            Log("AUDIOLAB.wing.reaper.virtualsoundcheck: No channels remain after filtering. Using all channels.\n");
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
    
    Undo_EndBlock("AUDIOLAB.wing.reaper.virtualsoundcheck: Configure Virtual Soundcheck", UNDO_STATE_TRACKCFG);
        
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
            "AUDIOLAB.wing.reaper.virtualsoundcheck - Success",
            0
        );
}

void ReaperExtension::ToggleSoundcheckMode() {
    if (!connected_ || !osc_handler_) {
        ShowMessageBox(
            "Please connect to Wing console first",
            "AUDIOLAB.wing.reaper.virtualsoundcheck - Soundcheck Mode",
            0
        );
        return;
    }
    
    // Toggle the state
    soundcheck_mode_enabled_ = !soundcheck_mode_enabled_;
    
    // Apply to all channels
    osc_handler_->SetAllChannelsAltEnabled(soundcheck_mode_enabled_);
    
    if (soundcheck_mode_enabled_) {
        Log("AUDIOLAB.wing.reaper.virtualsoundcheck: Soundcheck Mode ENABLED - Channels using USB input from REAPER\n");
        status_message_ = "Soundcheck Mode ON";
    } else {
        Log("AUDIOLAB.wing.reaper.virtualsoundcheck: Soundcheck Mode DISABLED - Channels using primary sources\n");
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
        const char* label = action.description;
        const char* sep = strstr(action.description, ": ");
        if (sep && *(sep + 2) != '\0') {
            label = sep + 2;
        }

        char msg[256];
        snprintf(msg, sizeof(msg), 
                 "  CC#%d (Ch 1) → %s\n", 
                 action.cc_number, label);
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
    if (TouchFile(kb_ini_path)) {
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
    if (TouchFile(kb_ini_path)) {
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
