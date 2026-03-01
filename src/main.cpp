/*
 * Wing Connector - Reaper Extension Entry Point
 * Integrates Behringer Wing console with Reaper via OSC
 */

#define REAPERAPI_IMPLEMENT
#include "reaper_plugin_functions.h"
#include "reaper_extension.h"
#include "logger.h"
#ifdef __APPLE__
#include "wing_connector_dialog_macos.h"
#endif
#include <string>
#include <cstring>
#include <cstdint>
#include <cstdio>
#include <fstream>

// Accelerator flags (from Windows API)
#define FVIRTKEY  0x01
#define FCONTROL  0x08
#define FSHIFT    0x04
#define FALT      0x10

using namespace WingConnector;

// ===== GLOBALS =====
REAPER_PLUGIN_HINSTANCE g_hInst = nullptr;
HWND g_hwndParent = nullptr;
static reaper_plugin_info_t* g_rec = nullptr;

// Command IDs
static int g_cmd_main_dialog = 0;

// ===== COMMAND HOOK (hookcommand2) =====
static bool OnAction(KbdSectionInfo* sec, int cmd, int val, int valhw, int relmode, HWND hwnd) {
    (void)sec;
    (void)val;
    (void)valhw;
    (void)relmode;
    (void)hwnd;

    Logger::Debug("OnAction() called with cmd=%d, g_cmd_main_dialog=%d", cmd, g_cmd_main_dialog);

    if (cmd == g_cmd_main_dialog) {
        Logger::Debug("Main dialog command triggered!");
        
        #ifdef __APPLE__
        Logger::Debug("Calling ShowWingConnectorDialog()");
        
        ShowWingConnectorDialog();
        
        Logger::Debug("ShowWingConnectorDialog() returned");
        #endif
        return true;
    }

    return false;
}

// ===== REGISTRATION =====
static void RegisterCommands() {
    Logger::Debug("RegisterCommands() called");
    
    if (!g_rec) {
        Logger::Error("ERROR: g_rec is null");
        return;
    }
    
    Logger::Debug("Registering hook command");
    
    // Register the command hook
    g_rec->Register("hookcommand2", (void*)OnAction);
    
    Logger::Debug("Registering custom action");
    
    // Register actions
    custom_action_register_t action;
    memset(&action, 0, sizeof(action));
    
    // Main consolidated dialog
    action.uniqueSectionId = 0;
    action.idStr = "_WING_MAIN_DIALOG";
    action.name = "Wing: Connect to Behringer Wing";
    
    int ret = g_rec->Register("custom_action", &action);
    g_cmd_main_dialog = ret;
    
    Logger::Debug("Custom action registered with ID: %d", ret);
    
    // Register keyboard shortcut (this should NOT create a duplicate action)
    if (g_cmd_main_dialog > 0) {
        Logger::Debug("Registering keyboard shortcut Ctrl+Shift+W");
        gaccel_register_t accel;
        accel.accel.cmd = g_cmd_main_dialog;
        accel.accel.key = 'W';
        accel.accel.fVirt = FVIRTKEY | FCONTROL | FSHIFT;
        accel.desc = "";  // Empty desc - don't show in actions list
        g_rec->Register("gaccel", &accel);
    }
    
    Logger::Debug("RegisterCommands() complete");
}

// ===== ENTRY POINT =====
extern "C" {

int REAPER_PLUGIN_DLL_EXPORT REAPER_PLUGIN_ENTRYPOINT(
    REAPER_PLUGIN_HINSTANCE hInstance,
    reaper_plugin_info_t* rec)
{
    // MARKER FILE - most reliable test
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
    
    if (!rec) {
        // Unloading
        Logger::Info("Plugin unloading");
        ReaperExtension::Instance().Shutdown();
        return 0;
    }
    
    Logger::Debug("Plugin loading - setting up API");
    
    g_hInst = hInstance;
    g_rec = rec;
    g_hwndParent = rec->hwnd_main;
    
    // Load Reaper API
    REAPERAPI_LoadAPI(rec->GetFunc);
    
    // Check that GetFunc is available
    if (!rec->GetFunc) {
        Logger::Error("ERROR: GetFunc not available");
        return 0;
    }
    
    Logger::Debug("Initializing ReaperExtension");
    
    // Initialize our extension (pass rec context for MIDI hook registration)
    if (!ReaperExtension::Instance().Initialize(rec)) {
        Logger::Error("ERROR: ReaperExtension::Initialize() failed");
        return 0;
    }
    
    Logger::Debug("Registering commands");
    
    // Register commands and actions
    RegisterCommands();
    
    Logger::Info("================================================================");
    Logger::Info("Plugin initialization complete!");
    Logger::Info("================================================================");
    
    return 1;
}

} // extern "C"
