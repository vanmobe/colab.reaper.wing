/*
 * macOS Native Settings Dialog Implementation
 * Provides native Cocoa dialogs for editing Wing Connector settings
 */

#ifdef __APPLE__

#import <Cocoa/Cocoa.h>
#include <string>
#include <cstring>
#include <cstdlib>

// Returns true if user confirmed, false if cancelled
// On success, ip_out and port_out contain the new values
extern "C" {

bool ShowSettingsDialog(const char* current_ip, int current_port, 
                       char* ip_out, int ip_out_size,
                       char* port_out, int port_out_size) {
    @autoreleasepool {
        // Create alert dialog
        NSAlert* alert = [[NSAlert alloc] init];
        [alert setMessageText:@"Wing Connector Settings"];
        [alert setInformativeText:@"Edit your Wing Console settings.\nRemember to enable OSC on the Wing (Setup > Network > OSC) and keep the OSC listener port at 2223."];
        [alert setAlertStyle:NSAlertStyleInformational];
        
        // Create view for input fields
        NSView* accessoryView = [[NSView alloc] initWithFrame:NSMakeRect(0, 0, 300, 90)];
        
        // IP Address label and text field
        NSTextField* ipLabel = [[NSTextField alloc] initWithFrame:NSMakeRect(0, 60, 80, 20)];
        [ipLabel setStringValue:@"Wing IP:"];
        [ipLabel setEditable:NO];
        [ipLabel setDrawsBackground:NO];
        [ipLabel setBordered:NO];
        [accessoryView addSubview:ipLabel];
        
        NSTextField* ipField = [[NSTextField alloc] initWithFrame:NSMakeRect(100, 60, 200, 24)];
        [ipField setStringValue:[NSString stringWithUTF8String:current_ip]];
        [accessoryView addSubview:ipField];
        
        // Port label and text field
        NSTextField* portLabel = [[NSTextField alloc] initWithFrame:NSMakeRect(0, 30, 140, 20)];
        [portLabel setStringValue:@"Listener Port (default 2223):"];
        [portLabel setEditable:NO];
        [portLabel setDrawsBackground:NO];
        [portLabel setBordered:NO];
        [accessoryView addSubview:portLabel];
        
        NSTextField* portField = [[NSTextField alloc] initWithFrame:NSMakeRect(160, 30, 140, 24)];
        [portField setStringValue:[NSString stringWithFormat:@"%d", current_port]];
        [accessoryView addSubview:portField];
        
        [alert setAccessoryView:accessoryView];
        
        // Add buttons
        [alert addButtonWithTitle:@"OK"];
        [alert addButtonWithTitle:@"Cancel"];
        
        // Show dialog
        NSInteger result = [alert runModal];
        
        // Process result
        if (result == NSAlertFirstButtonReturn) {
            // User clicked OK
            const char* ip_str = [[ipField stringValue] UTF8String];
            const char* port_str = [[portField stringValue] UTF8String];
            
            if (ip_str && port_str) {
                strncpy(ip_out, ip_str, ip_out_size - 1);
                ip_out[ip_out_size - 1] = '\0';
                
                strncpy(port_out, port_str, port_out_size - 1);
                port_out[port_out_size - 1] = '\0';
                
                return true;
            }
        }
        
        return false;
    }
}

} // extern "C"

#endif

