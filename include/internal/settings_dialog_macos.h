/*
 * macOS Native Settings Dialog Header
 */

#ifndef SETTINGS_DIALOG_MACOS_H
#define SETTINGS_DIALOG_MACOS_H

#ifdef __APPLE__

extern "C" {
    // Show native macOS settings dialog
    // Returns true if user confirmed, false if cancelled
    bool ShowSettingsDialog(const char* current_ip, int current_port, 
                           char* ip_out, int ip_out_size,
                           char* port_out, int port_out_size);
}

#endif

#endif // SETTINGS_DIALOG_MACOS_H

