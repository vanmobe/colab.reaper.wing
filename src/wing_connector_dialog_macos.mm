/*
 * macOS Native Wing Connector Dialog Implementation
 * Provides consolidated dialog for all Wing operations
 */

#ifdef __APPLE__

#import <Cocoa/Cocoa.h>
#include "wing_connector_dialog_macos.h"
#include "reaper_extension.h"
#include <string>
#include <vector>

using namespace WingConnector;

// ===== CHANNEL SELECTION DIALOG =====

extern "C" {

bool ShowChannelSelectionDialog(std::vector<WingConnector::ChannelSelectionInfo>& channels,
                                const char* title,
                                const char* description) {
    @autoreleasepool {
        NSAlert* alert = [[NSAlert alloc] init];
        [alert setMessageText:[NSString stringWithUTF8String:title]];
        [alert setInformativeText:[NSString stringWithUTF8String:description]];
        [alert setAlertStyle:NSAlertStyleInformational];
        
        // Calculate height needed for all channels
        int numChannels = (int)channels.size();
        int rowHeight = 24;
        int maxHeight = 400;
        int scrollHeight = std::min(numChannels * rowHeight + 20, maxHeight);
        
        // Create scrollable view for checkboxes
        NSScrollView* scrollView = [[NSScrollView alloc] initWithFrame:NSMakeRect(0, 0, 500, scrollHeight)];
        [scrollView setHasVerticalScroller:YES];
        [scrollView setHasHorizontalScroller:NO];
        [scrollView setBorderType:NSBezelBorder];
        
        // Document view to hold all checkboxes
        NSView* documentView = [[NSView alloc] initWithFrame:NSMakeRect(0, 0, 480, numChannels * rowHeight)];
        
        // Create checkbox array to track user selections
        NSMutableArray* checkboxes = [NSMutableArray arrayWithCapacity:numChannels];
        
        // Add checkbox for each channel
        int yPos = numChannels * rowHeight - rowHeight;
        for (int i = 0; i < numChannels; i++) {
            const auto& ch = channels[i];
            
            // Create title showing channel info
            NSString* title = nil;
            if (ch.stereo_linked && !ch.partner_source_group.empty()) {
                title = [NSString stringWithFormat:@"CH%02d: %s (%s%d / %s%d) [Stereo]",
                         ch.channel_number,
                         ch.name.c_str(),
                         ch.source_group.c_str(),
                         ch.source_input,
                         ch.partner_source_group.c_str(),
                         ch.partner_source_input];
            } else {
                title = [NSString stringWithFormat:@"CH%02d: %s (%s%d)%s",
                         ch.channel_number,
                         ch.name.c_str(),
                         ch.source_group.c_str(),
                         ch.source_input,
                         ch.stereo_linked ? " [Stereo]" : ""];
            }
            
            NSButton* checkbox = [[NSButton alloc] initWithFrame:NSMakeRect(10, yPos, 460, 20)];
            [checkbox setButtonType:NSButtonTypeSwitch];
            [checkbox setTitle:title];
            [checkbox setState:ch.selected ? NSControlStateValueOn : NSControlStateValueOff];
            
            [documentView addSubview:checkbox];
            [checkboxes addObject:checkbox];
            
            yPos -= rowHeight;
        }
        
        [scrollView setDocumentView:documentView];
        [alert setAccessoryView:scrollView];
        
        // Add buttons
        [alert addButtonWithTitle:@"OK"];
        [alert addButtonWithTitle:@"Cancel"];
        
        // Show dialog
        NSInteger result = [alert runModal];
        
        if (result == NSAlertFirstButtonReturn) {
            // OK clicked - update selection states
            for (int i = 0; i < numChannels; i++) {
                NSButton* checkbox = [checkboxes objectAtIndex:i];
                channels[i].selected = ([checkbox state] == NSControlStateValueOn);
            }
            return true;
        }
        
        // Cancel clicked
        return false;
    }
}

} // extern "C"

// ===== MAIN WING CONNECTOR WINDOW =====

@interface WingConnectorWindowController : NSWindowController <NSWindowDelegate>
{
    // UI Elements
    NSTextField* ipField;
    NSTextField* portField;
    NSTextField* statusLabel;
    NSButton* connectButton;
    NSButton* getChannelsButton;
    NSButton* setupSoundcheckButton;
    NSButton* toggleSoundcheckButton;
    NSTextView* activityLogView;
    NSScrollView* logScrollView;
    NSSegmentedControl* outputModeControl;
    NSButton* midiActionsCheckbox;
    
    BOOL isConnected;
}

- (instancetype)init;
- (void)setupUI;
- (void)updateConnectionStatus;
- (void)appendToLog:(NSString*)message;

- (void)onConnectClicked:(id)sender;
- (void)onGetChannelsClicked:(id)sender;
- (void)onSetupSoundcheckClicked:(id)sender;
- (void)onToggleSoundcheckClicked:(id)sender;
- (void)onOutputModeChanged:(id)sender;
- (void)onMidiActionsToggled:(id)sender;

- (void)runGetChannelsFlow;
- (void)runSetupSoundcheckFlow;
- (void)runToggleSoundcheckModeFlow;

@end

@implementation WingConnectorWindowController

- (instancetype)init {
    // Create the window with modern styling
    NSWindow* window = [[NSWindow alloc] initWithContentRect:NSMakeRect(0, 0, 700, 780)
                                                     styleMask:(NSWindowStyleMaskTitled |
                                                               NSWindowStyleMaskClosable |
                                                               NSWindowStyleMaskMiniaturizable)
                                                       backing:NSBackingStoreBuffered
                                                         defer:NO];
    [window setTitle:@"Wing Connector"];
    [window center];
    
    self = [super initWithWindow:window];
    if (!self) {
        return nil;
    }
    
    [window setDelegate:self];
    isConnected = NO;
    
    // MUST call setupUI FIRST to initialize activityLogView!
    [self setupUI];
    [self updateConnectionStatus];
    
    // NOW we can use appendToLog
    [self appendToLog:@"════════════════════════════════════════════════════════════════\n"];
    [self appendToLog:@"🔧 DIALOG INIT COMPLETE - Widget setup done\n"];
    [self appendToLog:@"════════════════════════════════════════════════════════════════\n"];
    
    // Set up log callback to capture C++ Log() calls  
    [self appendToLog:@"🔧 Setting up C++ log callback...\n"];
    auto log_lambda = [self](const std::string& msg) {
        NSString* nsMsg = [NSString stringWithUTF8String:msg.c_str()];
        dispatch_async(dispatch_get_main_queue(), ^{
            [self appendToLog:nsMsg];
        });
    };
    ReaperExtension::Instance().SetLogCallback(log_lambda);
    [self appendToLog:@"✓ C++ log callback registered successfully\n"];
    
    [self appendToLog:@"\nWing Connector ready. Configure settings and click Connect.\n"];
    [self appendToLog:@"════════════════════════════════════════════════════════════════\n"];
    
    return self;
}

- (void)setupUI {
    NSView* contentView = [[self window] contentView];
    int yPos = 700;
    
    // ===== HEADER WITH LOGO =====
    NSBox* headerBox = [[NSBox alloc] initWithFrame:NSMakeRect(0, yPos - 10, 700, 70)];
    [headerBox setBoxType:NSBoxCustom];
    [headerBox setFillColor:[NSColor colorWithWhite:0.95 alpha:1.0]];
    [headerBox setBorderWidth:0];
    [contentView addSubview:headerBox];
    
    // App Icon
    NSImageView* iconView = [[NSImageView alloc] initWithFrame:NSMakeRect(20, yPos, 40, 40)];
    NSImage* appIcon = [NSImage imageNamed:NSImageNameApplicationIcon];
    [iconView setImage:appIcon];
    [contentView addSubview:iconView];
    
    // Title
    NSTextField* titleLabel = [[NSTextField alloc] initWithFrame:NSMakeRect(70, yPos + 20, 400, 24)];
    [titleLabel setStringValue:@"Behringer Wing Connector"];
    [titleLabel setFont:[NSFont systemFontOfSize:18 weight:NSFontWeightMedium]];
    [titleLabel setBezeled:NO];
    [titleLabel setEditable:NO];
    [titleLabel setSelectable:NO];
    [titleLabel setBackgroundColor:[NSColor clearColor]];
    [titleLabel setTextColor:[NSColor labelColor]];
    [contentView addSubview:titleLabel];
    
    // Subtitle
    NSTextField* subtitleLabel = [[NSTextField alloc] initWithFrame:NSMakeRect(70, yPos, 400, 18)];
    [subtitleLabel setStringValue:@"Connect and manage Wing console integration"];
    [subtitleLabel setFont:[NSFont systemFontOfSize:12]];
    [subtitleLabel setBezeled:NO];
    [subtitleLabel setEditable:NO];
    [subtitleLabel setSelectable:NO];
    [subtitleLabel setBackgroundColor:[NSColor clearColor]];
    [subtitleLabel setTextColor:[NSColor secondaryLabelColor]];
    [contentView addSubview:subtitleLabel];
    
    yPos -= 85;
    
    // ===== CONNECTION SETTINGS =====
    NSTextField* settingsLabel = [[NSTextField alloc] initWithFrame:NSMakeRect(20, yPos, 300, 20)];
    [settingsLabel setStringValue:@"Connection Settings"];
    [settingsLabel setFont:[NSFont systemFontOfSize:13 weight:NSFontWeightSemibold]];
    [settingsLabel setBezeled:NO];
    [settingsLabel setEditable:NO];
    [settingsLabel setSelectable:NO];
    [settingsLabel setBackgroundColor:[NSColor clearColor]];
    [settingsLabel setTextColor:[NSColor labelColor]];
    [contentView addSubview:settingsLabel];
    yPos -= 35;
    
    // IP Address
    NSTextField* ipLabel = [[NSTextField alloc] initWithFrame:NSMakeRect(20, yPos, 100, 20)];
    [ipLabel setStringValue:@"Wing IP:"];
    [ipLabel setFont:[NSFont systemFontOfSize:12]];
    [ipLabel setBezeled:NO];
    [ipLabel setEditable:NO];
    [ipLabel setSelectable:NO];
    [ipLabel setBackgroundColor:[NSColor clearColor]];
    [ipLabel setTextColor:[NSColor secondaryLabelColor]];
    [contentView addSubview:ipLabel];
    
    ipField = [[NSTextField alloc] initWithFrame:NSMakeRect(130, yPos, 180, 24)];
    [ipField setStringValue:@"192.168.10.210"];
    [ipField setFont:[NSFont systemFontOfSize:12]];
    [contentView addSubview:ipField];
    yPos -= 30;
    
    // Listen Port
    NSTextField* portLabel = [[NSTextField alloc] initWithFrame:NSMakeRect(20, yPos, 100, 20)];
    [portLabel setStringValue:@"Listen Port:"];
    [portLabel setFont:[NSFont systemFontOfSize:12]];
    [portLabel setBezeled:NO];
    [portLabel setEditable:NO];
    [portLabel setSelectable:NO];
    [portLabel setBackgroundColor:[NSColor clearColor]];
    [portLabel setTextColor:[NSColor secondaryLabelColor]];
    [contentView addSubview:portLabel];
    
    portField = [[NSTextField alloc] initWithFrame:NSMakeRect(130, yPos, 180, 24)];
    [portField setStringValue:@"2223"];
    [portField setFont:[NSFont systemFontOfSize:12]];
    [contentView addSubview:portField];
    yPos -= 40;
    
    // Status and Connect Button in same section
    statusLabel = [[NSTextField alloc] initWithFrame:NSMakeRect(20, yPos, 300, 26)];
    [statusLabel setStringValue:@"⚪ Not Connected"];
    [statusLabel setFont:[NSFont systemFontOfSize:13 weight:NSFontWeightMedium]];
    [statusLabel setBezeled:NO];
    [statusLabel setEditable:NO];
    [statusLabel setSelectable:NO];
    [statusLabel setBackgroundColor:[NSColor clearColor]];
    [statusLabel setTextColor:[NSColor labelColor]];
    [contentView addSubview:statusLabel];
    
    connectButton = [[NSButton alloc] initWithFrame:NSMakeRect(560, yPos - 2, 120, 32)];
    [connectButton setTitle:@"Connect"];
    [connectButton setBezelStyle:NSBezelStyleRounded];
    [connectButton setKeyEquivalent:@"\r"];
    [connectButton setTarget:self];
    [connectButton setAction:@selector(onConnectClicked:)];
    [contentView addSubview:connectButton];
    yPos -= 30;
    
    // ===== ACTIONS =====
    NSBox* separator1 = [[NSBox alloc] initWithFrame:NSMakeRect(20, yPos, 660, 1)];
    [separator1 setBoxType:NSBoxSeparator];
    [contentView addSubview:separator1];
    yPos -= 30;
    
    NSTextField* actionsHeader = [[NSTextField alloc] initWithFrame:NSMakeRect(20, yPos, 200, 20)];
    [actionsHeader setStringValue:@"Actions"];
    [actionsHeader setFont:[NSFont systemFontOfSize:13 weight:NSFontWeightSemibold]];
    [actionsHeader setBezeled:NO];
    [actionsHeader setEditable:NO];
    [actionsHeader setSelectable:NO];
    [actionsHeader setBackgroundColor:[NSColor clearColor]];
    [actionsHeader setTextColor:[NSColor labelColor]];
    [contentView addSubview:actionsHeader];
    yPos -= 35;
    
    // Get Channels Button
    getChannelsButton = [[NSButton alloc] initWithFrame:NSMakeRect(20, yPos, 200, 32)];
    [getChannelsButton setBezelStyle:NSBezelStyleRounded];
    [getChannelsButton setTitle:@"Get Channels"];
    [getChannelsButton setTarget:self];
    [getChannelsButton setAction:@selector(onGetChannelsClicked:)];
    [getChannelsButton setEnabled:NO];
    [contentView addSubview:getChannelsButton];
    
    NSTextField* getChannelsDesc = [[NSTextField alloc] initWithFrame:NSMakeRect(230, yPos+8, 450, 20)];
    [getChannelsDesc setStringValue:@"Query Wing for available channels and create REAPER tracks"];
    [getChannelsDesc setFont:[NSFont systemFontOfSize:11]];
    [getChannelsDesc setBezeled:NO];
    [getChannelsDesc setEditable:NO];
    [getChannelsDesc setSelectable:NO];
    [getChannelsDesc setBackgroundColor:[NSColor clearColor]];
    [getChannelsDesc setTextColor:[NSColor secondaryLabelColor]];
    [contentView addSubview:getChannelsDesc];
    yPos -= 42;
    
    // Output Mode Selector (USB/CARD)
    NSTextField* outputModeLabel = [[NSTextField alloc] initWithFrame:NSMakeRect(20, yPos + 8, 150, 20)];
    [outputModeLabel setStringValue:@"Soundcheck Output:"];
    [outputModeLabel setFont:[NSFont systemFontOfSize:11]];
    [outputModeLabel setBezeled:NO];
    [outputModeLabel setEditable:NO];
    [outputModeLabel setSelectable:NO];
    [outputModeLabel setBackgroundColor:[NSColor clearColor]];
    [contentView addSubview:outputModeLabel];
    
    outputModeControl = [[NSSegmentedControl alloc] initWithFrame:NSMakeRect(160, yPos + 4, 120, 24)];
    [outputModeControl setSegmentCount:2];
    [outputModeControl setLabel:@"USB" forSegment:0];
    [outputModeControl setLabel:@"CARD" forSegment:1];
    [outputModeControl setSelectedSegment:0];
    [outputModeControl setSegmentStyle:NSSegmentStyleRounded];
    [outputModeControl setTarget:self];
    [outputModeControl setAction:@selector(onOutputModeChanged:)];
    [contentView addSubview:outputModeControl];
    yPos -= 32;
    
    // MIDI Actions Checkbox
    midiActionsCheckbox = [[NSButton alloc] initWithFrame:NSMakeRect(20, yPos, 400, 20)];
    [midiActionsCheckbox setButtonType:NSButtonTypeSwitch];
    [midiActionsCheckbox setTitle:@"Enable MIDI Actions (Wing buttons control REAPER)"];
    // Check if MIDI is truly enabled: config must be true AND shortcuts must exist
    auto& ext = ReaperExtension::Instance();
    BOOL midiFullyEnabled = ext.IsMidiActionsEnabled() && ext.AreMidiShortcutsRegistered();
    [midiActionsCheckbox setState:midiFullyEnabled ? NSControlStateValueOn : NSControlStateValueOff];
    [midiActionsCheckbox setTarget:self];
    [midiActionsCheckbox setAction:@selector(onMidiActionsToggled:)];
    [contentView addSubview:midiActionsCheckbox];
    yPos -= 28;
    
    // Setup Soundcheck Button
    setupSoundcheckButton = [[NSButton alloc] initWithFrame:NSMakeRect(20, yPos, 200, 32)];
    [setupSoundcheckButton setBezelStyle:NSBezelStyleRounded];
    [setupSoundcheckButton setTitle:@"Setup Soundcheck"];
    [setupSoundcheckButton setTarget:self];
    [setupSoundcheckButton setAction:@selector(onSetupSoundcheckClicked:)];
    [setupSoundcheckButton setEnabled:NO];
    [contentView addSubview:setupSoundcheckButton];
    
    NSTextField* soundcheckDesc = [[NSTextField alloc] initWithFrame:NSMakeRect(230, yPos+8, 450, 20)];
    [soundcheckDesc setStringValue:@"Configure virtual soundcheck folder and routing"];
    [soundcheckDesc setFont:[NSFont systemFontOfSize:11]];
    [soundcheckDesc setBezeled:NO];
    [soundcheckDesc setEditable:NO];
    [soundcheckDesc setSelectable:NO];
    [soundcheckDesc setBackgroundColor:[NSColor clearColor]];
    [soundcheckDesc setTextColor:[NSColor secondaryLabelColor]];
    [contentView addSubview:soundcheckDesc];
    yPos -= 42;
    
    // Toggle Soundcheck Button
    toggleSoundcheckButton = [[NSButton alloc] initWithFrame:NSMakeRect(20, yPos, 200, 32)];
    [toggleSoundcheckButton setBezelStyle:NSBezelStyleRounded];
    [toggleSoundcheckButton setTitle:@"Toggle Soundcheck"];
    [toggleSoundcheckButton setTarget:self];
    [toggleSoundcheckButton setAction:@selector(onToggleSoundcheckClicked:)];
    [toggleSoundcheckButton setEnabled:NO];
    [contentView addSubview:toggleSoundcheckButton];
    
    NSTextField* toggleDesc = [[NSTextField alloc] initWithFrame:NSMakeRect(230, yPos+8, 450, 20)];
    [toggleDesc setStringValue:@"Switch between live and playback input modes"];
    [toggleDesc setFont:[NSFont systemFontOfSize:11]];
    [toggleDesc setBezeled:NO];
    [toggleDesc setEditable:NO];
    [toggleDesc setSelectable:NO];
    [toggleDesc setBackgroundColor:[NSColor clearColor]];
    [toggleDesc setTextColor:[NSColor secondaryLabelColor]];
    [contentView addSubview:toggleDesc];
    yPos -= 50;
    
    // ===== ACTIVITY LOG =====
    NSBox* separator2 = [[NSBox alloc] initWithFrame:NSMakeRect(20, yPos, 660, 1)];
    [separator2 setBoxType:NSBoxSeparator];
    [contentView addSubview:separator2];
    yPos -= 30;
    
    NSTextField* logHeader = [[NSTextField alloc] initWithFrame:NSMakeRect(20, yPos, 200, 20)];
    [logHeader setStringValue:@"Activity Log"];
    [logHeader setFont:[NSFont systemFontOfSize:13 weight:NSFontWeightSemibold]];
    [logHeader setBezeled:NO];
    [logHeader setEditable:NO];
    [logHeader setSelectable:NO];
    [logHeader setBackgroundColor:[NSColor clearColor]];
    [logHeader setTextColor:[NSColor labelColor]];
    [contentView addSubview:logHeader];
    yPos -= 30;
    
    // Activity log scroll view and text view
    logScrollView = [[NSScrollView alloc] initWithFrame:NSMakeRect(20, 20, 660, yPos - 20)];
    [logScrollView setHasVerticalScroller:YES];
    [logScrollView setHasHorizontalScroller:NO];
    [logScrollView setBorderType:NSBezelBorder];
    [logScrollView setAutoresizingMask:(NSViewWidthSizable | NSViewHeightSizable)];
    [logScrollView setBackgroundColor:[NSColor textBackgroundColor]];
    
    activityLogView = [[NSTextView alloc] initWithFrame:NSMakeRect(0, 0, 640, yPos - 20)];
    [activityLogView setFont:[NSFont monospacedSystemFontOfSize:11 weight:NSFontWeightRegular]];
    [activityLogView setAutoresizingMask:(NSViewWidthSizable | NSViewHeightSizable)];
    [activityLogView setTextColor:[NSColor labelColor]];
    [activityLogView setBackgroundColor:[NSColor textBackgroundColor]];
    
    [logScrollView setDocumentView:activityLogView];
    [contentView addSubview:logScrollView];
}

- (void)updateConnectionStatus {
    isConnected = ReaperExtension::Instance().IsConnected();
    
    if (isConnected) {
        [statusLabel setStringValue:@"🟢 Connected"];
        [connectButton setTitle:@"Disconnect"];
        [getChannelsButton setEnabled:YES];
        [setupSoundcheckButton setEnabled:YES];
        [toggleSoundcheckButton setEnabled:YES];
    } else {
        [statusLabel setStringValue:@"⚪ Not Connected"];
        [connectButton setTitle:@"Connect"];
        [getChannelsButton setEnabled:NO];
        [setupSoundcheckButton setEnabled:NO];
        [toggleSoundcheckButton setEnabled:NO];
    }
}

- (void)appendToLog:(NSString*)message {
    NSString* currentText = [activityLogView string];
    NSString* newText = [currentText stringByAppendingString:message];
    [activityLogView setString:newText];
    
    // Scroll to bottom
    NSRange range = NSMakeRange([[activityLogView string] length], 0);
    [activityLogView scrollRangeToVisible:range];
}

- (void)onConnectClicked:(id)sender {
    if (isConnected) {
        [self appendToLog:@"\n=== Disconnecting ===\n"];
        ReaperExtension::Instance().DisconnectFromWing();
        [self appendToLog:@"Disconnected from Wing console\n"];
    } else {
        [self appendToLog:@"\n=== Connecting to Wing ===\n"];
        
        // Update config from UI
        auto& config = ReaperExtension::Instance().GetConfig();
        config.wing_ip = std::string([[ipField stringValue] UTF8String]);
        config.wing_port = 2223;
        config.listen_port = (uint16_t)[[portField stringValue] intValue];
        
        [self appendToLog:[NSString stringWithFormat:@"Connecting to %s:%d...\n", 
                          config.wing_ip.c_str(), config.wing_port]];
        
        ReaperExtension::Instance().ConnectToWing();
        
        if (ReaperExtension::Instance().IsConnected()) {
            [self appendToLog:@"✓ Connected successfully!\n"];
        } else {
            [self appendToLog:@"✗ Connection failed. Check settings and try again.\n"];
        }
    }
    
    [self updateConnectionStatus];
}

- (void)onGetChannelsClicked:(id)sender {
    [self appendToLog:@"\n=== Getting Channels ===\n"];
    [self runGetChannelsFlow];
}

- (void)onSetupSoundcheckClicked:(id)sender {
    // Update output mode from UI
    auto& config = ReaperExtension::Instance().GetConfig();
    config.soundcheck_output_mode = ([outputModeControl selectedSegment] == 0) ? "USB" : "CARD";
    
    [self appendToLog:[NSString stringWithFormat:@"\n=== Setting up Virtual Soundcheck (%s mode) ===\n",
                      config.soundcheck_output_mode.c_str()]];
    [self runSetupSoundcheckFlow];
}

- (void)onToggleSoundcheckClicked:(id)sender {
    [self appendToLog:@"\n=== Toggling Soundcheck Mode ===\n"];
    [self runToggleSoundcheckModeFlow];
}

- (void)onOutputModeChanged:(id)sender {
    auto& extension = ReaperExtension::Instance();
    auto& config = extension.GetConfig();
    config.soundcheck_output_mode = ([outputModeControl selectedSegment] == 0) ? "USB" : "CARD";

    std::string details;
    bool fullyAvailable = extension.CheckOutputModeAvailability(config.soundcheck_output_mode, details);
    [self appendToLog:[NSString stringWithFormat:@"Output mode selected: %s\n", config.soundcheck_output_mode.c_str()]];
    [self appendToLog:[NSString stringWithFormat:@"%s\n", details.c_str()]];

    if (!fullyAvailable) {
        NSAlert* alert = [[NSAlert alloc] init];
        [alert setMessageText:@"Selected mode may not be fully available"];
        [alert setInformativeText:[NSString stringWithUTF8String:details.c_str()]];
        [alert addButtonWithTitle:@"OK"];
        [alert runModal];
    }
}

- (void)onMidiActionsToggled:(id)sender {
    auto& extension = ReaperExtension::Instance();
    BOOL enabled = ([midiActionsCheckbox state] == NSControlStateValueOn);
    
    if (enabled) {
        // When enabling, always force registration to ensure shortcuts are written
        extension.EnableMidiActions(true);
        
        // Verify it actually worked
        if (!extension.AreMidiShortcutsRegistered()) {
            [self appendToLog:@"⚠️ Warning: MIDI shortcuts may not have been registered correctly\n"];
            [midiActionsCheckbox setState:NSControlStateValueOff];
        } else {
            [self appendToLog:@"✓ MIDI actions enabled - Wing buttons now control REAPER\n"];
        }
    } else {
        // When disabling, remove shortcuts
        extension.EnableMidiActions(false);
        [self appendToLog:@"MIDI actions disabled\n"];
    }
}

- (void)runGetChannelsFlow {
    auto& extension = ReaperExtension::Instance();

    if (!extension.IsConnected()) {
        if (!extension.ConnectToWing()) {
            [self appendToLog:@"✗ Connection failed. Please check settings.\n"];
            return;
        }
    }

    [self appendToLog:@"Querying Wing console for channels...\n"];
    [[NSRunLoop currentRunLoop] runUntilDate:[NSDate dateWithTimeIntervalSinceNow:0.01]];
    
    auto channels = extension.GetAvailableChannels();
    if (channels.empty()) {
        [self appendToLog:@"✗ No channels with sources found.\n"];
        return;
    }

    [self appendToLog:[NSString stringWithFormat:@"Found %d channels with sources\n", (int)channels.size()]];
    
    bool confirmed = ShowChannelSelectionDialog(
        channels,
        "Select Channels to Import",
        "Choose which channels to create as REAPER tracks."
    );

    if (!confirmed) {
        [self appendToLog:@"Cancelled by user\n"];
        return;
    }

    int selectedCount = 0;
    for (const auto& channel : channels) {
        if (channel.selected) {
            selectedCount++;
        }
    }

    if (selectedCount == 0) {
        [self appendToLog:@"✗ No channels selected\n"];
        return;
    }

    [self appendToLog:[NSString stringWithFormat:@"Creating %d tracks...\n", selectedCount]];
    extension.CreateTracksFromSelection(channels);
    [self appendToLog:@"✓ Tracks created successfully\n"];
}

- (void)runSetupSoundcheckFlow {
    auto& extension = ReaperExtension::Instance();

    if (!extension.IsConnected()) {
        if (!extension.ConnectToWing()) {
            [self appendToLog:@"✗ Connection failed. Please check settings.\n"];
            return;
        }
    }

    [self appendToLog:@"Getting Wing channels for soundcheck setup...\n"];
    [[NSRunLoop currentRunLoop] runUntilDate:[NSDate dateWithTimeIntervalSinceNow:0.01]];
    
    auto channels = extension.GetAvailableChannels();
    if (channels.empty()) {
        [self appendToLog:@"✗ No channels with sources found.\n"];
        return;
    }

    [self appendToLog:[NSString stringWithFormat:@"Found %d channels with sources\n", (int)channels.size()]];
    
    bool confirmed = ShowChannelSelectionDialog(
        channels,
        "Select Channels for Virtual Soundcheck",
        "Choose which channels to configure for virtual soundcheck."
    );

    if (!confirmed) {
        [self appendToLog:@"Cancelled by user\n"];
        return;
    }

    int selectedCount = 0;
    for (const auto& channel : channels) {
        if (channel.selected) {
            selectedCount++;
        }
    }

    if (selectedCount == 0) {
        [self appendToLog:@"✗ No channels selected\n"];
        return;
    }

    [self appendToLog:[NSString stringWithFormat:@"Setting up soundcheck for %d channels...\n", selectedCount]];
    extension.SetupSoundcheckFromSelection(channels);
    [self appendToLog:@"✓ Soundcheck setup complete\n"];
}

- (void)runToggleSoundcheckModeFlow {
    auto& extension = ReaperExtension::Instance();

    if (!extension.IsConnected()) {
        if (!extension.ConnectToWing()) {
            [self appendToLog:@"✗ Connection failed. Please check settings.\n"];
            return;
        }
    }

    [self appendToLog:@"Toggling soundcheck mode...\n"];
    extension.ToggleSoundcheckMode();
    bool enabled = extension.IsSoundcheckModeEnabled();

    if (enabled) {
        [self appendToLog:@"✓ Soundcheck mode ENABLED (using playback inputs)\n"];
    } else {
        [self appendToLog:@"✓ Soundcheck mode DISABLED (using live inputs)\n"];
    }
}

@end

extern "C" {

void ShowWingConnectorDialog() {
    // Write debug output to system stderr (visible in REAPER console)
    fprintf(stderr, "\n🔧 [WING] ShowWingConnectorDialog() called\n");
    fflush(stderr);
    
    // Must run UI operations on main thread
    dispatch_async(dispatch_get_main_queue(), ^{
        @autoreleasepool {
            fprintf(stderr, "🔧 [WING] Creating WingConnectorWindowController\n");
            fflush(stderr);
            
            WingConnectorWindowController* controller = [[WingConnectorWindowController alloc] init];
            
            fprintf(stderr, "🔧 [WING] Showing window\n");
            fflush(stderr);
            
            [[controller window] makeKeyAndOrderFront:nil];
            
            fprintf(stderr, "🔧 [WING] Appending init message\n");
            fflush(stderr);
            
            [controller appendToLog:@"🔧 Window initialization complete\n"];
        }
    });
}

} // extern "C"

#endif // __APPLE__
