#ifndef TRACK_MANAGER_H
#define TRACK_MANAGER_H

#include "wing_osc.h"
#include "wing_config.h"
#include <vector>
#include <string>

// Forward declare Reaper types
struct MediaTrack;
struct ReaProject;

namespace WingConnector {

class TrackManager {
public:
    TrackManager(const WingConfig& config);
    ~TrackManager();
    
    // Create tracks from Wing channel data
    int CreateTracksFromChannelData(const std::map<int, ChannelInfo>& channels);
    
    // Create a single track
    MediaTrack* CreateTrack(int index, const std::string& name, int color_id);
    
    // Create stereo track
    MediaTrack* CreateStereoTrack(int index, const std::string& name, int color_id);
    
    // Update existing track from channel info
    void UpdateTrack(MediaTrack* track, const ChannelInfo& channel);
    
    // Find existing Wing tracks
    std::vector<MediaTrack*> FindExistingWingTracks();
    
    // Clear all tracks (with user confirmation)
    void ClearAllTracks();
    
    // Track configuration
    void SetTrackColor(MediaTrack* track, int wing_color_id);
    void SetTrackName(MediaTrack* track, const std::string& name);
    void SetTrackInput(MediaTrack* track, int input_index, int num_channels = 1);
    bool SetTrackHardwareOutput(MediaTrack* track, int output_index, int num_channels = 1);
    void ClearTrackHardwareOutputs(MediaTrack* track);
    void ArmTrackForRecording(MediaTrack* track, bool arm = true);
    
private:
    // Wing color palette structure
    struct ColorRGB { uint8_t r, g, b; };
    
    // Static color lookup table (Wing console colors)
    static constexpr ColorRGB WING_COLORS[] = {
        { 50, 120, 255},  // 0:  Blue
        {100, 200, 255},  // 1:  Light Blue
        { 20,  40, 180},  // 2:  Dark Blue
        {  0, 160, 190},  // 3:  Sea Blue / Teal
        { 50, 210,  50},  // 4:  Green
        { 20, 130,  20},  // 5:  Dark Green
        {230, 210,  20},  // 6:  Yellow
        {240, 130,  20},  // 7:  Orange
        {220,  40,  40},  // 8:  Red
        {230, 110, 180},  // 9:  Pink
        {160,  30, 230},  // 10: Purple
        { 90,  10, 140},  // 11: Dark Purple
    };
    static constexpr int NUM_WING_COLORS = sizeof(WING_COLORS) / sizeof(ColorRGB);
    
    const WingConfig& config_;
    std::vector<MediaTrack*> created_tracks_;
    
    // Color conversion
    int WingColorToReaperColor(int wing_color_id);
    int RGBToReaperColor(uint8_t r, uint8_t g, uint8_t b);
    
    // Get current Reaper project
    ReaProject* GetCurrentProject();
};

} // namespace WingConnector

#endif // TRACK_MANAGER_H
