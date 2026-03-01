/*
 * Track Manager Implementation
 * Creates and manages Reaper tracks based on Wing channel data
 * 
 * This module handles the core track creation workflow:
 * 1. Receives Wing channel information (names, colors, routing)
 * 2. Creates corresponding Reaper tracks
 * 3. Configures track properties (names, colors, input/output routing)
 * 4. Optionally creates stereo pairs for linked channels
 * 
 * Key responsibilities:
 * - MediaTrack creation and configuration
 * - Color mapping from Wing palette to REAPER native colors
 * - Input/output routing setup
 * - Stereo channel pairing logic
 * - Track cache management
 */

#include "wingconnector/track_manager.h"
#include "reaper_plugin_functions.h"
#include <algorithm>

namespace WingConnector {

/**
 * TrackManager Constructor
 * 
 * Args:
 *   config - Reference to WingConfig with user preferences
 */
TrackManager::TrackManager(const WingConfig& config)
    : config_(config)
{
}

/**
 * TrackManager Destructor
 * Cleans up track reference cache
 */
TrackManager::~TrackManager() {
    created_tracks_.clear();
}

/**
 * GetCurrentProject() - Retrieve active REAPER project
 * 
 * Returns: Pointer to current ReaProject (EnumProjects -1 = current project)
 */
ReaProject* TrackManager::GetCurrentProject() {
    return EnumProjects(-1, nullptr, 0);
}

/**
 * RGBToReaperColor() - Convert RGB values to REAPER native color format
 * 
 * REAPER uses OS-dependent color encoding. This function converts RGB values
 * to the correct format for the current platform (macOS, Windows, Linux).
 * 
 * Args:
 *   r, g, b - Red, green, blue values (0-255)
 * 
 * Returns: REAPER native color value
 */
int TrackManager::RGBToReaperColor(uint8_t r, uint8_t g, uint8_t b) {
    // Use REAPER's ColorToNative for correct OS-dependent encoding
    return ColorToNative((int)r, (int)g, (int)b);
}

/**
 * WingColorToReaperColor() - Map Wing color ID to REAPER color value
 * 
 * Wing console has 48 predefined colors. This function looks up the Wing color
 * palette entry and converts it to the REAPER native format.
 * 
 * Args:
 *   wing_color_id - Color index from Wing (0-47)
 * 
 * Returns: REAPER native color, or default color if invalid index
 */
int TrackManager::WingColorToReaperColor(int wing_color_id) {
    if (wing_color_id >= 0 && wing_color_id < NUM_WING_COLORS) {
        const auto& c = WING_COLORS[wing_color_id];
        return RGBToReaperColor(c.r, c.g, c.b);
    }
    
    // Default color if Wing color ID is invalid
    return RGBToReaperColor(
        config_.default_color.r,
        config_.default_color.g,
        config_.default_color.b
    );
}

/**
 * SetTrackColor() - Apply custom color to a REAPER track
 * 
 * Sets the track's custom color from the Wing palette.
 * Note: REAPER custom colors require the 0x01000000 flag to enable.
 * 
 * Args:
 *   track            - Target MediaTrack pointer
 *   wing_color_id    - Color index from Wing console
 */
void TrackManager::SetTrackColor(MediaTrack* track, int wing_color_id) {
    if (!track) return;
    
    int color = WingColorToReaperColor(wing_color_id);
    // I_CUSTOMCOLOR: The 0x01000000 flag enables the custom color display
    SetMediaTrackInfo_Value(track, "I_CUSTOMCOLOR", color | 0x01000000);
}

/**
 * SetTrackName() - Set track name with optional prefix
 * 
 * Sets the REAPER track name from Wing channel name, optionally prepending
 * a user-configured prefix for organization.
 * 
 * Args:
 *   track - Target MediaTrack pointer
 *   name  - Wing channel name (e.g., "Kick", "Main L")
 */
void TrackManager::SetTrackName(MediaTrack* track, const std::string& name) {
    if (!track) return;
    
    // Prepend prefix if configured
    std::string full_name = config_.track_prefix.empty() ? name : config_.track_prefix + " " + name;
    GetSetMediaTrackInfo_String(track, "P_NAME", (char*)full_name.c_str(), true);
}

/**
 * SetTrackInput() - Configure track recording input
 * 
 * Routes the track to a specific Wing output.
 * 
 * REAPER input encoding:
 * - Bits 0-9:   Input device index
 * - Bits 10+:   Channel count - 1 (for multi-channel inputs)
 * 
 * Example: USB input 1, 2 channels → (1 & 0x3FF) | ((2-1) << 10)
 * 
 * Args:
 *   track         - Target MediaTrack pointer
 *   input_index   - Device/input number (0-based)
 *   num_channels  - Number of channels (1 = mono, 2 = stereo)
 */
void TrackManager::SetTrackInput(MediaTrack* track, int input_index, int num_channels) {
    if (!track) return;
    
    // Encode input device and channel count in REAPER's I_RECINPUT format
    // Lower 10 bits = device index, upper bits = (channel_count - 1)
    int input_value = (input_index & 0x3FF) | ((num_channels - 1) << 10);
    SetMediaTrackInfo_Value(track, "I_RECINPUT", input_value);
    
    // Set recording mode to "input" (not output)
    SetMediaTrackInfo_Value(track, "I_RECMODE", 0);
}

/**
 * ClearTrackHardwareOutputs() - Remove all hardware output sends
 * 
 * Clears any existing hardware output routing (send type 1).
 * Useful for resetting tracks before reconfiguring outputs.
 * 
 * Args:
 *   track - Target MediaTrack pointer
 */
void TrackManager::ClearTrackHardwareOutputs(MediaTrack* track) {
    if (!track) return;

    // Get count of hardware output sends (send type 1)
    int hw_out_count = GetTrackNumSends(track, 1);
    
    // Remove all sends in reverse order (to avoid index shifting)
    for (int i = hw_out_count - 1; i >= 0; --i) {
        RemoveTrackSend(track, 1, i);
    }
}

/**
 * SetTrackHardwareOutput() - Route track to hardware output
 * 
 * Creates a hardware output send (send type 1) rout the track to
 * a specific physical output or auxiliary bus.
 * 
 * Supports both mono and stereo routing:
 * - Mono:   Channel 0 → output_index | 1024 (mono flag)
 * - Stereo: Channels 0+1 → output_index (pair of outputs)
 * 
 * Args:
 *   track         - Target MediaTrack pointer
 *   output_index  - Physical output/bus number
 *   num_channels  - 1 for mono, 2+ for stereo
 * 
 * Returns: true if successful, false on error
 */
bool TrackManager::SetTrackHardwareOutput(MediaTrack* track, int output_index, int num_channels) {
    if (!track || output_index < 0) {
        return false;
    }

    // Clear existing outputs first
    ClearTrackHardwareOutputs(track);

    // Create new hardware output send
    int send_idx = CreateTrackSend(track, nullptr);
    if (send_idx < 0) {
        return false;
    }

    if (num_channels >= 2) {
        // Stereo: Source channels 0+1 → output_index and output_index+1
        SetTrackSendInfo_Value(track, 1, send_idx, "I_SRCCHAN", 0);
        SetTrackSendInfo_Value(track, 1, send_idx, "I_DSTCHAN", output_index);
    } else {
        // Mono: Source channel 0 → output_index (with 1024 mono flag)
        int src_chan_mono = (1 << 10);
        int dst_chan_mono = output_index | 1024;
        SetTrackSendInfo_Value(track, 1, send_idx, "I_SRCCHAN", src_chan_mono);
        SetTrackSendInfo_Value(track, 1, send_idx, "I_DSTCHAN", dst_chan_mono);
    }

    SetTrackSendInfo_Value(track, 1, send_idx, "D_VOL", 1.0);
    SetTrackSendInfo_Value(track, 1, send_idx, "B_MUTE", 0.0);
    return true;
}

void TrackManager::ArmTrackForRecording(MediaTrack* track, bool arm) {
    if (!track) return;
    
    SetMediaTrackInfo_Value(track, "I_RECARM", arm ? 1 : 0);
}

MediaTrack* TrackManager::CreateTrack(int index, const std::string& name, int color_id) {
    ReaProject* proj = GetCurrentProject();
    if (!proj) return nullptr;
    
    // Insert track at index
    InsertTrackAtIndex(index, false);
    
    // Get the newly created track
    MediaTrack* track = GetTrack(proj, index);
    if (!track) return nullptr;
    
    // Configure track
    SetTrackName(track, name);
    
    if (config_.color_tracks) {
        SetTrackColor(track, color_id);
    }
    
    // Set input and arm for recording
    SetTrackInput(track, index, 1);
    ArmTrackForRecording(track, true);
    
    // Store in our list
    created_tracks_.push_back(track);
    
    return track;
}

MediaTrack* TrackManager::CreateStereoTrack(int index, const std::string& name, int color_id) {
    MediaTrack* track = CreateTrack(index, name + " (Stereo)", color_id);
    
    if (track) {
        // Set to stereo (2 channels)
        SetMediaTrackInfo_Value(track, "I_NCHAN", 2);
        
        // Set stereo input
        SetTrackInput(track, index, 2);
    }
    
    return track;
}

int TrackManager::CreateTracksFromChannelData(const std::map<int, ChannelInfo>& channels) {
    if (channels.empty()) {
        return 0;
    }
    
    // Clear previous tracks list
    created_tracks_.clear();
    
    // Begin undo block
    Undo_BeginBlock();
    
    int track_index = 0;
    bool skip_next = false;
    
    for (const auto& pair : channels) {
        const int ch_num = pair.first;
        const ChannelInfo& ch_info = pair.second;
        
        // Skip if this is the second channel of a stereo pair
        if (skip_next) {
            skip_next = false;
            continue;
        }
        
        // Check for stereo pairing
        if (config_.create_stereo_pairs && (ch_num % 2 == 1)) {
            // Check if next channel exists
            auto next_it = channels.find(ch_num + 1);
            if (next_it != channels.end()) {
                // Create stereo track
                CreateStereoTrack(track_index, ch_info.name, ch_info.color);
                skip_next = true;
            } else {
                // No pair, create mono
                CreateTrack(track_index, ch_info.name, ch_info.color);
            }
        } else {
            // Create mono track
            CreateTrack(track_index, ch_info.name, ch_info.color);
        }
        
        track_index++;
    }
    
    // End undo block
    Undo_EndBlock("Create Wing Tracks", UNDO_STATE_TRACKCFG);
    
    // Update arrange view
    UpdateArrange();
    
    return (int)created_tracks_.size();
}

void TrackManager::UpdateTrack(MediaTrack* track, const ChannelInfo& channel) {
    if (!track) return;
    
    // Update track properties
    SetTrackName(track, channel.name);
    
    if (config_.color_tracks) {
        SetTrackColor(track, channel.color);
    }
}

std::vector<MediaTrack*> TrackManager::FindExistingWingTracks() {
    std::vector<MediaTrack*> wing_tracks;
    
    ReaProject* proj = GetCurrentProject();
    if (!proj) return wing_tracks;
    
    int track_count = CountTracks(proj);
    
    for (int i = 0; i < track_count; ++i) {
        MediaTrack* track = GetTrack(proj, i);
        if (!track) continue;
        
        char name_buf[512];
        if (GetSetMediaTrackInfo_String(track, "P_NAME", name_buf, false)) {
            std::string track_name = name_buf;
            
            // Check if it starts with our prefix
            if (track_name.find(config_.track_prefix) == 0) {
                wing_tracks.push_back(track);
            }
        }
    }
    
    return wing_tracks;
}

void TrackManager::ClearAllTracks() {
    ReaProject* proj = GetCurrentProject();
    if (!proj) return;
    
    Undo_BeginBlock();
    
    // Delete all tracks in reverse order
    int track_count = CountTracks(proj);
    for (int i = track_count - 1; i >= 0; --i) {
        MediaTrack* track = GetTrack(proj, i);
        if (track) {
            DeleteTrack(track);
        }
    }
    
    Undo_EndBlock("Clear All Tracks", UNDO_STATE_TRACKCFG);
    UpdateArrange();
    
    created_tracks_.clear();
}

} // namespace WingConnector
