#ifndef REAPER_EXTENSION_H
#define REAPER_EXTENSION_H

// Forward declaration to avoid include ordering issues
struct reaper_plugin_info_t;

#include "wing_config.h"
#include "wing_osc.h"
#include "track_manager.h"
#include <memory>
#include <atomic>
#include <vector>
#include <functional>
#include <thread>

namespace WingConnector {

// Forward declaration for channel selection info
struct ChannelSelectionInfo {
    int channel_number;
    std::string name;
    std::string source_group;
    int source_input;
    std::string partner_name;
    std::string partner_source_group;
    int partner_source_input;
    bool stereo_linked;
    bool selected;
};

class ReaperExtension {
public:
    static ReaperExtension& Instance();
    
    // Extension lifecycle
    bool Initialize(reaper_plugin_info_t* rec = nullptr);
    void Shutdown();
    
    // Connection (called by dialog)
    bool ConnectToWing();  // Returns true if successful
    void DisconnectFromWing();

    // Network discovery: scan for Wing consoles on the LAN
    std::vector<WingInfo> DiscoverWings(int timeout_ms = 1500);
    
    // Channel operations (called by dialog)
    std::vector<ChannelSelectionInfo> GetAvailableChannels();
    void CreateTracksFromSelection(const std::vector<ChannelSelectionInfo>& channels);
    void SetupSoundcheckFromSelection(const std::vector<ChannelSelectionInfo>& channels, bool setup_soundcheck = true);
    bool CheckOutputModeAvailability(const std::string& output_mode, std::string& details) const;
    bool ValidateLiveRecordingSetup(std::string& details);
    void RouteMainLRToCardForSDRecording();
    double ReadCurrentTriggerLevel();
    void SendWarningOscNow();
    void SendStartOscNow();
    void SendStopOscNow();
    void TestWarningFlash();
    void ApplyAutoRecordSettings();
    
    // MIDI action mapping
    void EnableMidiActions(bool enable);
    bool IsMidiActionsEnabled() const { return midi_actions_enabled_; }
    bool AreMidiShortcutsRegistered() const;  // Verify shortcuts exist in reaper-kb.ini
    void EnableWingMidiDevice();
    
    // Legacy actions (keep for backward compatibility)
    void RefreshTracks();
    void ShowSettings();
    void ConfigureVirtualSoundcheck();
    void ToggleSoundcheckMode();
    bool IsSoundcheckModeEnabled() const { return soundcheck_mode_enabled_; }
    
    // Real-time monitoring (deprecated but kept for compatibility)
    void EnableMonitoring(bool enable);
    bool IsMonitoring() const { return monitoring_enabled_; }
    int GetProjectTrackCount() const;
    
    // Status
    bool IsConnected() const { return connected_; }
    std::string GetStatusMessage() const { return status_message_; }
    
    // Configuration
    WingConfig& GetConfig() { return config_; }
    const WingConfig& GetConfig() const { return config_; }
    
    // Access to OSC handler for dialog logging
    WingOSC* GetOSCHandler() { return osc_handler_.get(); }
    
    // Logging callback
    using LogCallback = std::function<void(const std::string&)>;
    void SetLogCallback(LogCallback callback) { log_callback_ = callback; }
    void Log(const std::string& message);
    
    // MIDI input hook (needed by extern wrapper function)
    static bool MidiInputHook(bool is_midi, const unsigned char* data, int len, int dev_id);
    static void MainThreadTimerTick();

private:
    ReaperExtension();
    ~ReaperExtension();
    
    // Singleton - delete copy/move
    ReaperExtension(const ReaperExtension&) = delete;
    ReaperExtension& operator=(const ReaperExtension&) = delete;
    
    // Members
    WingConfig config_;
    std::unique_ptr<WingOSC> osc_handler_;
    std::unique_ptr<TrackManager> track_manager_;
    
    std::atomic<bool> connected_;
    std::atomic<bool> monitoring_enabled_;
    std::atomic<bool> soundcheck_mode_enabled_;
    std::atomic<bool> midi_actions_enabled_;
    std::string status_message_;
    LogCallback log_callback_;
    
    // Static REAPER plugin context (set in Initialize)
    static reaper_plugin_info_t* g_rec_;
    
    // MIDI action registration
    struct MidiAction {
        int command_id;
        const char* description;
        int cc_number;
    };
    static constexpr MidiAction MIDI_ACTIONS[] = {
        {40157, "AUDIOLAB.wing.reaper.virtualsoundcheck: Set Marker", 20},
        {40172, "AUDIOLAB.wing.reaper.virtualsoundcheck: Previous Marker", 21},
        {40173, "AUDIOLAB.wing.reaper.virtualsoundcheck: Next Marker", 22},
        {1013, "AUDIOLAB.wing.reaper.virtualsoundcheck: Record", 23},
        {1016, "AUDIOLAB.wing.reaper.virtualsoundcheck: Stop", 24},
        {1007, "AUDIOLAB.wing.reaper.virtualsoundcheck: Play", 25},
        {1008, "AUDIOLAB.wing.reaper.virtualsoundcheck: Pause", 26}
    };
    
    void RegisterMidiShortcuts();
    void UnregisterMidiShortcuts();
    
    // MIDI input handling  
    void ProcessMidiInput(const unsigned char* data, int len);
    void MonitorAutoRecordLoop();
    void StartAutoRecordMonitor();
    void StopAutoRecordMonitor();
    double GetMaxArmedTrackPeak() const;
    void StartWarningFlash();
    void StopWarningFlash(bool force = false);
    void WarningFlashLoop();
    void SetWarningLayerState();
    void SetRecordingLayerState();
    void ClearLayerState();
    void ApplySDRoutingNoDialog();
    
    // Callbacks
    void OnChannelDataReceived(const ChannelInfo& channel);
    void OnConnectionStatusChanged(bool connected);

    std::atomic<bool> auto_record_monitor_running_{false};
    std::unique_ptr<std::thread> auto_record_monitor_thread_;
    std::atomic<bool> auto_record_started_by_plugin_{false};
    std::atomic<bool> warning_flash_running_{false};
    std::unique_ptr<std::thread> warning_flash_thread_;
    std::atomic<bool> manual_flash_override_{false};
    std::atomic<bool> pending_record_start_{false};
    std::atomic<bool> pending_record_stop_{false};
    std::atomic<int> layer_state_mode_{0}; // 0=idle, 1=warning, 2=recording
};

// Reaper action command IDs
extern "C" {
    void CommandConnectToWing(int command_id);
    void CommandRefreshTracks(int command_id);
    void CommandToggleMonitoring(int command_id);
    void CommandShowSettings(int command_id);
    void CommandConfigureVirtualSoundcheck(int command_id);
    void CommandToggleSoundcheckMode(int command_id);
}

} // namespace WingConnector

#endif // REAPER_EXTENSION_H
