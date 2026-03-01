/*
 * macOS Native Wing Connector Dialog Header
 * Consolidated dialog for all Wing Connector operations
 */

#ifndef WING_CONNECTOR_DIALOG_MACOS_H
#define WING_CONNECTOR_DIALOG_MACOS_H

#ifdef __APPLE__

#include <vector>
#include <string>
#include "reaper_extension.h"  // For ChannelSelectionInfo (from wingconnector/)

// Dialog result enum
enum class DialogAction {
    None,
    Connect,
    Disconnect,
    GetChannels,
    SetupSoundcheck,
    SaveSettings,
    Close
};

// Main Wing Connector Dialog
// This is a modeless window that stays open
extern "C" {
    // Show the main Wing Connector dialog
    // Returns the action the user wants to perform
    void ShowWingConnectorDialog();
}

// Channel Selection Dialog
extern "C" {
    // Show channel selection dialog
    // channels: List of available channels (will be modified with selection state)
    // Returns true if user confirmed, false if cancelled
    bool ShowChannelSelectionDialog(std::vector<WingConnector::ChannelSelectionInfo>& channels,
                                   const char* title,
                                   const char* description);
}

#endif // __APPLE__

#endif // WING_CONNECTOR_DIALOG_MACOS_H
