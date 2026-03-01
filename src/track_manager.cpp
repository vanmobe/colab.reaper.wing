/*
 * Track Manager Implementation
 * Creates and manages Reaper tracks based on Wing channel data
 */

#include "wingconnector/track_manager.h"
#include "reaper_plugin_functions.h"
#include <algorithm>

namespace WingConnector {

TrackManager::TrackManager(const WingConfig& config)
    : config_(config)
{
}

TrackManager::~TrackManager() {
    created_tracks_.clear();
}

ReaProject* TrackManager::GetCurrentProject() {
    return EnumProjects(-1, nullptr, 0);
}

int TrackManager::RGBToReaperColor(uint8_t r, uint8_t g, uint8_t b) {
    // Use REAPER's ColorToNative for correct OS-dependent encoding
    return ColorToNative((int)r, (int)g, (int)b);
}

int TrackManager::WingColorToReaperColor(int wing_color_id) {
    if (wing_color_id >= 0 && wing_color_id < NUM_WING_COLORS) {
        const auto& c = WING_COLORS[wing_color_id];
        return RGBToReaperColor(c.r, c.g, c.b);
    }
    
    // Default color
    return RGBToReaperColor(
        config_.default_color.r,
        config_.default_color.g,
        config_.default_color.b
    );
}

void TrackManager::SetTrackColor(MediaTrack* track, int wing_color_id) {
    if (!track) return;
    
    int color = WingColorToReaperColor(wing_color_id);
    // I_CUSTOMCOLOR: 0x01000000 flag enables the custom color
    SetMediaTrackInfo_Value(track, "I_CUSTOMCOLOR", color | 0x01000000);
}

void TrackManager::SetTrackName(MediaTrack* track, const std::string& name) {
    if (!track) return;
    
    std::string full_name = config_.track_prefix.empty() ? name : config_.track_prefix + " " + name;
    GetSetMediaTrackInfo_String(track, "P_NAME", (char*)full_name.c_str(), true);
}

void TrackManager::SetTrackInput(MediaTrack* track, int input_index, int num_channels) {
    if (!track) return;
    
    // Set recording input
    // Format: (index & 0x3FF) | ((num_channels-1) << 10)
    int input_value = (input_index & 0x3FF) | ((num_channels - 1) << 10);
    SetMediaTrackInfo_Value(track, "I_RECINPUT", input_value);
    
    // Set recording mode to input (not output)
    SetMediaTrackInfo_Value(track, "I_RECMODE", 0);
}

void TrackManager::ClearTrackHardwareOutputs(MediaTrack* track) {
    if (!track) return;

    int hw_out_count = GetTrackNumSends(track, 1);
    for (int i = hw_out_count - 1; i >= 0; --i) {
        RemoveTrackSend(track, 1, i);
    }
}

bool TrackManager::SetTrackHardwareOutput(MediaTrack* track, int output_index, int num_channels) {
    if (!track || output_index < 0) {
        return false;
    }

    ClearTrackHardwareOutputs(track);

    int send_idx = CreateTrackSend(track, nullptr);
    if (send_idx < 0) {
        return false;
    }

    if (num_channels >= 2) {
        SetTrackSendInfo_Value(track, 1, send_idx, "I_SRCCHAN", 0);
        SetTrackSendInfo_Value(track, 1, send_idx, "I_DSTCHAN", output_index);
    } else {
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
