/*
 * Wing Connector - Reaper Extension Entry Point
 * Integrates Behringer Wing console with Reaper via OSC
 * 
 * This module handles the Reaper plugin lifecycle:
 * - Plugin loading/unloading
 * - REAPER API initialization
 * - Command registration and keyboard shortcuts
 * - Dialog invocation
 * 
 * REAPER calls REAPER_PLUGIN_ENTRYPOINT() on load, which initializes:
 * 1. Logging system for debug output
 * 2. REAPER API function pointers
 * 3. ReaperExtension singleton for main logic
 * 4. Custom actions and keyboard shortcuts
 */

#define REAPERAPI_IMPLEMENT
#include "reaper_plugin_functions.h"
#include "wingconnector/reaper_extension.h"
#include "internal/logger.h"
#ifdef __APPLE__
#include "internal/wing_connector_dialog_macos.h"
#endif
#include <string>
#include <cstring>
#include <cstdint>
#include <cstdio>
#include <fstream>

// Keyboard modifier flag constants (Windows/REAPER standard)
#define FVIRTKEY  0x01  // Virtual key code (not ASCII)
#define FCONTROL  0x08  // Ctrl modifier
#define FSHIFT    0x04  // Shift modifier
#define FALT      0x10  // Alt modifier

using namespace WingConnector;

// ===== GLOBALS =====
REAPER_PLUGIN_HINSTANCE g_hInst = nullptr;
HWND g_hwndParent = nullptr;
static reaper_plugin_info_t* g_rec = nullptr;

// Command IDs
static int g_cmd_main_dialog = 0;

// ===== COMMAND HOOK (hookcommand2) =====
/**
 * OnAction() - REAPER Command Dispatcher
 * 
 * Called by REAPER whenever an action/command is executed.
 * We filter for our Wing Connector action and launch the main dialog.
 * 
 * Args:
 *   sec     - Keyboard section info (unused)
 *   cmd     - Command ID to check
 *   val     - Command value (unused)
 *   valhw   - Hardware value (unused)
 *   relmode - Relative mode (unused)
 *   hwnd    - Window handle (unused)
 * 
 * Returns: true if we handled the command, false to pass to next handler
 */
static bool OnAction(KbdSectionInfo* sec, int cmd, int val, int valhw, int relmode, HWND hwnd) {
    (void)sec;
    (void)val;
    (void)valhw;
    (void)relmode;
    (void)hwnd;

    Logger::Debug("OnAction() called with cmd=%d, g_cmd_main_dialog=%d", cmd, g_cmd_main_dialog);

    // Check if this is our Wing Connector command
    if (cmd == g_cmd_main_dialog) {
        Logger::Debug("Main dialog command triggered!");
        
        #ifdef __APPLE__
        Logger::Debug("Calling ShowWingConnectorDialog()");
        
        // Show the connection dialog on macOS
        ShowWingConnectorDialog();
        
        Logger::Debug("ShowWingConnectorDialog() returned");
        #endif
        return true;  // Command handled
    }

    return false;  // Not our command, pass to next handler
}

// ===== REGISTRATION =====
/**
 * RegisterCommands() - Register Wing Connector UI Actions
 * 
 * Register with REAPER:
 * 1. hookcommand2 callback to intercept user commands
 * 2. Custom action "Wing: Connect to Behringer Wing"
 * 3. Keyboard shortcut Ctrl+Shift+W for quick access
 * 
 * REAPER will:
 * - Add the action to Extensions → Wing Connector menu
 * - Trigger OnAction() when the action is invoked
 * - Map Ctrl+Shift+W to the command ID
 */
static void RegisterCommands() {
    Logger::Debug("RegisterCommands() called");
    
    if (!g_rec) {
        Logger::Error("ERROR: g_rec is null - cannot register commands");
        return;
    }
    
    Logger::Debug("Registering hook command for action interception");
    
    // Register the command hook - REAPER will call OnAction() for all commands
    g_rec->Register("hookcommand2", (void*)OnAction);
    
    Logger::Debug("Registering custom action");
    
    // Define the Wing Connector action
    custom_action_register_t action;
    memset(&action, 0, sizeof(action));
    
    action.uniqueSectionId = 0;           // Main action section
    action.idStr = "_WING_MAIN_DIALOG";   // Unique ID for this action
    action.name = "Wing: Connect to Behringer Wing";  // Menu label
    
    // Register and store the action ID for later reference
    int ret = g_rec->Register("custom_action", &action);
    g_cmd_main_dialog = ret;
    
    Logger::Debug("Custom action registered with ID: %d", ret);
    
    // Register keyboard shortcut Ctrl+Shift+W for quick access
    if (g_cmd_main_dialog > 0) {
        Logger::Debug("Registering keyboard shortcut Ctrl+Shift+W");
        gaccel_register_t accel;
        accel.accel.cmd = g_cmd_main_dialog;     // Bind to our action
        accel.accel.key = 'W';                   // Key: W
        accel.accel.fVirt = FVIRTKEY | FCONTROL | FSHIFT;  // Modifiers: Ctrl+Shift
        accel.desc = "";  // Empty desc - don't show duplicate in actions list
        g_rec->Register("gaccel", &accel);
    }
    
    Logger::Debug("RegisterCommands() complete");
}

// ===== ENTRY POINT =====
/**
 * REAPER_PLUGIN_ENTRYPOINT() - Plugin Load/Unload Handler
 * 
 * Called by REAPER when:
 * 1. Plugin is loaded (rec != nullptr)
 * 2. Plugin is unloaded (rec == nullptr)
 * 
 * On load, we:
 * - Initialize logging for debug messages
 * - Get REAPER API function pointers
 * - Initialize ReaperExtension singleton
 * - Register actions and keyboard shortcuts
 * 
 * Args:
 *   hInstance - Plugin DLL/SO handle
 *   rec       - REAPER plugin API interface (nullptr on unload)
 * 
 * Returns: 1 = success, 0 = error/unload
 */
extern "C" {

int REAPER_PLUGIN_DLL_EXPORT REAPER_PLUGIN_ENTRYPOINT(
    REAPER_PLUGIN_HINSTANCE hInstance,
    reaper_plugin_info_t* rec)
{
    // Write marker file to verify plugin loaded
    FILE* f = fopen("/tmp/wing_plugin_loaded.txt", "w");
    if (f) {
        fprintf(f, "PLUGIN LOADED AT: ENTRY POINT\n");
        fflush(f);
        fclose(f);
    }
    
    Logger::Initialize(true);
    Logger::Info("================================================================");
    Logger::Info("REAPER_PLUGIN_ENTRYPOINT called - PLUGIN LOADING");
    Logger::Info("================================================================");
    
    // Handle plugin unload
    if (!rec) {
        Logger::Info("Plugin unloading");
        ReaperExtension::Instance().Shutdown();
        return 0;
    }
    
    Logger::Debug("Plugin loading - setting up API");
    
    // Store global REAPER API context
    g_hInst = hInstance;
    g_rec = rec;
    g_hwndParent = rec->hwnd_main;
    
    // Load REAPER API function pointers from rec->GetFunc
    REAPERAPI_LoadAPI(rec->GetFunc);
    
    // Verify GetFunc is available
    if (!rec->GetFunc) {
        Logger::Error("ERROR: GetFunc not available - REAPER API not initialized");
        return 0;
    }
    
    Logger::Debug("Initializing ReaperExtension singleton");
    
    // Initialize main extension logic (pass rec for MIDI hook registration)
    if (!ReaperExtension::Instance().Initialize(rec)) {
        Logger::Error("ERROR: ReaperExtension::Initialize() failed");
        return 0;
    }
    
    Logger::Debug("Registering commands and keyboard shortcuts");
    
    // Register custom actions and keyboard shortcuts
    RegisterCommands();
    
    Logger::Info("================================================================");
    Logger::Info("Plugin initialization complete!");
    Logger::Info("================================================================");
    
    return 1;  // Success
}

} // extern "C"
