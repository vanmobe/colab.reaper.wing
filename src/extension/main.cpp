/*
 * AUDIOLAB.wing.reaper.virtualsoundcheck - Reaper Extension Entry Point
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

#include <cstring>

#define REAPERAPI_IMPLEMENT
#include "reaper_plugin_functions.h"
#include "wingconnector/reaper_extension.h"
#include "internal/logger.h"
#include "internal/dialog_bridge.h"
#include <string>
#include <cstdint>
#include <cstdio>
#include <fstream>

// Keyboard modifier flag constants (Windows/REAPER standard)
#define FVIRTKEY  0x01  // Virtual key code (not ASCII)
#define FCONTROL  0x08  // Ctrl modifier
#define FSHIFT    0x04  // Shift modifier
#define FALT      0x10  // Alt modifier
#ifndef FCOMMAND
#define FCOMMAND  0x20  // Cmd modifier on macOS (fallback when not provided by headers)
#endif

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
 * We filter for our AUDIOLAB.wing.reaper.virtualsoundcheck action and launch the main dialog.
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

    // Check if this is our AUDIOLAB.wing.reaper.virtualsoundcheck command
    if (cmd == g_cmd_main_dialog) {
        Logger::Debug("Main dialog command triggered!");

        ShowMainDialog();
        return true;  // Command handled
    }

    return false;  // Not our command, pass to next handler
}

// ===== REGISTRATION =====
/**
 * RegisterCommands() - Register AUDIOLAB.wing.reaper.virtualsoundcheck UI Actions
 * 
 * Register with REAPER:
 * 1. hookcommand2 callback to intercept user commands
 * 2. Custom action "Behringer Wing: Configure Virtual Soundcheck/Recording"
 * 3. Optional keyboard shortcut Ctrl+Shift+W
 *
 * REAPER will:
 * - expose the custom action in the action system
 * - trigger OnAction() when the action is invoked
 * - map Ctrl+Shift+W to the command ID (if registration succeeds)
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
    
    // Define the AUDIOLAB.wing.reaper.virtualsoundcheck action
    custom_action_register_t action;
    memset(&action, 0, sizeof(action));
    
    action.uniqueSectionId = 0;           // Main action section
    action.idStr = "_AUDIOLAB_VIRTUALSOUNDCHECK_MAIN_DIALOG";   // Unique ID for this action
    action.name = "Behringer Wing: Configure Virtual Soundcheck/Recording";  // Menu label
    
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
        accel.accel.fVirt = FVIRTKEY | FCONTROL | FSHIFT;
        accel.desc = "";
        g_rec->Register("gaccel", &accel);
#ifdef FCOMMAND
        gaccel_register_t accel_cmd;
        accel_cmd.accel.cmd = g_cmd_main_dialog;
        accel_cmd.accel.key = 'W';
        accel_cmd.accel.fVirt = FVIRTKEY | FCOMMAND | FSHIFT;  // Cmd+Shift on macOS
        accel_cmd.desc = "";
        g_rec->Register("gaccel", &accel_cmd);

        gaccel_register_t accel_cmd_lower;
        accel_cmd_lower.accel.cmd = g_cmd_main_dialog;
        accel_cmd_lower.accel.key = 'w';
        accel_cmd_lower.accel.fVirt = FVIRTKEY | FCOMMAND | FSHIFT;
        accel_cmd_lower.desc = "";
        g_rec->Register("gaccel", &accel_cmd_lower);
#endif
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
