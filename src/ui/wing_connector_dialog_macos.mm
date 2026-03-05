/*
 * macOS Native AUDIOLAB.wing.reaper.virtualsoundcheck Dialog Implementation
 * Provides consolidated dialog for all Wing operations
 */

#ifdef __APPLE__

#import <Cocoa/Cocoa.h>
#include "internal/wing_connector_dialog_macos.h"
#include "wingconnector/reaper_extension.h"
#include <string>
#include <vector>

using namespace WingConnector;

// ===== CHANNEL SELECTION DIALOG =====

extern "C" {

bool ShowChannelSelectionDialog(std::vector<WingConnector::ChannelSelectionInfo>& channels,
                                const char* title,
                                const char* description,
                                bool& setup_soundcheck) {
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
        
        // Create container view for scroll view + soundcheck checkbox
        NSView* containerView = [[NSView alloc] initWithFrame:NSMakeRect(0, 0, 500, scrollHeight + 40)];
        
        // Position scroll view at top of container
        [scrollView setFrameOrigin:NSMakePoint(0, 40)];
        [containerView addSubview:scrollView];
        
        // Add soundcheck mode checkbox at bottom
        NSButton* soundcheckCheckbox = [[NSButton alloc] initWithFrame:NSMakeRect(10, 10, 480, 20)];
        [soundcheckCheckbox setButtonType:NSButtonTypeSwitch];
        [soundcheckCheckbox setTitle:@"Configure soundcheck mode (ALT sources + inputs for REAPER playback)"];
        [soundcheckCheckbox setState:NSControlStateValueOn];  // Default to enabled
        [containerView addSubview:soundcheckCheckbox];
        
        [alert setAccessoryView:containerView];
        
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
            // Update soundcheck mode option
            setup_soundcheck = ([soundcheckCheckbox state] == NSControlStateValueOn);
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
    NSPopUpButton* wingDropdown;
    NSButton* scanButton;
    NSMutableArray* discoveredIPs;
    NSTextField* statusLabel;
    NSButton* setupSoundcheckButton;
    NSButton* toggleSoundcheckButton;
    NSButton* connectButton;
    NSTextField* setupSoundcheckDescriptionLabel;
    NSTextView* activityLogView;
    NSScrollView* logScrollView;
    NSSegmentedControl* outputModeControl;
    NSSegmentedControl* midiActionsControl;
    
    BOOL isConnected;
    BOOL isWorking;  // Prevents re-entrant button clicks while an operation is in progress
    BOOL liveSetupValidated;  // True when Wing + REAPER routing validate as a complete live setup
    BOOL validationInProgress;  // Prevent overlapping auto-connect/validation runs
}

- (instancetype)init;
- (void)setupUI;
- (void)updateConnectionStatus;
- (void)updateToggleSoundcheckButtonLabel;
- (void)updateSetupSoundcheckButtonLabel;
- (void)refreshLiveSetupValidation;
- (void)appendToLog:(NSString*)message;
- (void)setWorkingState:(BOOL)working;

- (void)startDiscoveryScan;
- (void)populateDropdownWithItems:(NSArray*)items ips:(NSArray*)ips;
- (void)onWingDropdownChanged:(id)sender;
- (void)onScanClicked:(id)sender;
- (void)onConnectClicked:(id)sender;
- (NSString*)selectedWingIP;

- (void)onSetupSoundcheckClicked:(id)sender;
- (void)onToggleSoundcheckClicked:(id)sender;
- (void)onOutputModeChanged:(id)sender;
- (void)onMidiActionsToggled:(id)sender;

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
    [window setTitle:@"AUDIOLAB.wing.reaper.virtualsoundcheck"];
    [window center];
    
    self = [super initWithWindow:window];
    [window release];  // NSWindowController retains the window, release our creation reference
    if (!self) {
        return nil;
    }
    
    [window setDelegate:self];
    discoveredIPs = [[NSMutableArray alloc] init];  // Explicitly retain
    isConnected = NO;
    isWorking = NO;
    liveSetupValidated = NO;
    validationInProgress = NO;
    
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
    
    [self appendToLog:@"\nScanning network for Wing consoles...\n"];
    [self appendToLog:@"════════════════════════════════════════════════════════════════\n"];
    
    // Auto-scan for Wings on the network
    [self startDiscoveryScan];
    
    return self;
}

- (void)dealloc {
    [discoveredIPs release];
    // Release UI elements that we retain in instance variables
    [wingDropdown release];
    [scanButton release];
    [statusLabel release];
    [setupSoundcheckButton release];
    [toggleSoundcheckButton release];
    [connectButton release];
    [setupSoundcheckDescriptionLabel release];
    [activityLogView release];
    [logScrollView release];
    [outputModeControl release];
    [midiActionsControl release];
    [super dealloc];
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
    [titleLabel setStringValue:@"Behringer AUDIOLAB.wing.reaper.virtualsoundcheck"];
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
    
    // Wing Console selector (auto-discovery dropdown)
    NSTextField* consoleLabel = [[NSTextField alloc] initWithFrame:NSMakeRect(20, yPos, 110, 20)];
    [consoleLabel setStringValue:@"Wing Console:"];
    [consoleLabel setFont:[NSFont systemFontOfSize:12]];
    [consoleLabel setBezeled:NO];
    [consoleLabel setEditable:NO];
    [consoleLabel setSelectable:NO];
    [consoleLabel setBackgroundColor:[NSColor clearColor]];
    [consoleLabel setTextColor:[NSColor secondaryLabelColor]];
    [contentView addSubview:consoleLabel];
    
    wingDropdown = [[NSPopUpButton alloc] initWithFrame:NSMakeRect(130, yPos - 3, 410, 28) pullsDown:NO];
    [wingDropdown addItemWithTitle:@"Scanning..."];
    [[wingDropdown itemAtIndex:0] setEnabled:NO];
    [wingDropdown setEnabled:NO];
    [wingDropdown setTarget:self];
    [wingDropdown setAction:@selector(onWingDropdownChanged:)];
    [contentView addSubview:wingDropdown];
    
    scanButton = [[NSButton alloc] initWithFrame:NSMakeRect(550, yPos - 1, 130, 28)];
    [scanButton setTitle:@"Scan"];
    [scanButton setBezelStyle:NSBezelStyleRounded];
    [scanButton setTarget:self];
    [scanButton setAction:@selector(onScanClicked:)];
    [contentView addSubview:scanButton];
    yPos -= 36;
    
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

    connectButton = [[NSButton alloc] initWithFrame:NSMakeRect(560, yPos - 2, 120, 28)];
    [connectButton setBezelStyle:NSBezelStyleRounded];
    [connectButton setTitle:@"Connect"];
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
    
    // Output Mode Selector (USB/CARD)
    NSTextField* outputModeLabel = [[NSTextField alloc] initWithFrame:NSMakeRect(20, yPos + 8, 150, 20)];
    [outputModeLabel setStringValue:@"Send sources to:"];
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
    
    // Wing Button Actions Toggle
    NSTextField* midiActionsLabel = [[NSTextField alloc] initWithFrame:NSMakeRect(20, yPos + 8, 150, 20)];
    [midiActionsLabel setStringValue:@"Link Reaper actions to MIDI:"];
    [midiActionsLabel setFont:[NSFont systemFontOfSize:11]];
    [midiActionsLabel setBezeled:NO];
    [midiActionsLabel setEditable:NO];
    [midiActionsLabel setSelectable:NO];
    [midiActionsLabel setBackgroundColor:[NSColor clearColor]];
    [contentView addSubview:midiActionsLabel];
    
    midiActionsControl = [[NSSegmentedControl alloc] initWithFrame:NSMakeRect(160, yPos + 4, 120, 24)];
    [midiActionsControl setSegmentCount:2];
    [midiActionsControl setLabel:@"OFF" forSegment:0];
    [midiActionsControl setLabel:@"ON" forSegment:1];
    // Check if MIDI is truly enabled: config must be true AND shortcuts must exist
    auto& ext = ReaperExtension::Instance();
    BOOL midiFullyEnabled = ext.IsMidiActionsEnabled() && ext.AreMidiShortcutsRegistered();
    [midiActionsControl setSelectedSegment:midiFullyEnabled ? 1 : 0];
    [midiActionsControl setSegmentStyle:NSSegmentStyleRounded];
    [midiActionsControl setTarget:self];
    [midiActionsControl setAction:@selector(onMidiActionsToggled:)];
    [contentView addSubview:midiActionsControl];
    yPos -= 32;
    
    // Setup Live Recording Button
    setupSoundcheckButton = [[NSButton alloc] initWithFrame:NSMakeRect(20, yPos, 200, 32)];
    [setupSoundcheckButton setBezelStyle:NSBezelStyleRounded];
    [setupSoundcheckButton setTitle:@"Setup Live Recording"];
    [setupSoundcheckButton setTarget:self];
    [setupSoundcheckButton setAction:@selector(onSetupSoundcheckClicked:)];
    [contentView addSubview:setupSoundcheckButton];
    
    setupSoundcheckDescriptionLabel = [[NSTextField alloc] initWithFrame:NSMakeRect(230, yPos+8, 450, 20)];
    [setupSoundcheckDescriptionLabel setStringValue:@"Configure live recording tracks and routing"];
    [setupSoundcheckDescriptionLabel setFont:[NSFont systemFontOfSize:11]];
    [setupSoundcheckDescriptionLabel setBezeled:NO];
    [setupSoundcheckDescriptionLabel setEditable:NO];
    [setupSoundcheckDescriptionLabel setSelectable:NO];
    [setupSoundcheckDescriptionLabel setBackgroundColor:[NSColor clearColor]];
    [setupSoundcheckDescriptionLabel setTextColor:[NSColor secondaryLabelColor]];
    [contentView addSubview:setupSoundcheckDescriptionLabel];
    yPos -= 42;
    
    // Toggle Soundcheck Button
    toggleSoundcheckButton = [[NSButton alloc] initWithFrame:NSMakeRect(20, yPos, 200, 32)];
    [toggleSoundcheckButton setBezelStyle:NSBezelStyleRounded];
    [toggleSoundcheckButton setTitle:@"🎙️ Live Mode"];
    [toggleSoundcheckButton setTarget:self];
    [toggleSoundcheckButton setAction:@selector(onToggleSoundcheckClicked:)];
    [toggleSoundcheckButton setEnabled:NO];  // Disabled until setup is complete
    [contentView addSubview:toggleSoundcheckButton];
    
    NSTextField* toggleDesc = [[NSTextField alloc] initWithFrame:NSMakeRect(230, yPos+8, 450, 20)];
    [toggleDesc setStringValue:@"Toggle between live and soundcheck modes (requires setup first)"];
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
    } else {
        [statusLabel setStringValue:@"⚪ Not Connected"];
        [connectButton setTitle:@"Connect"];
    }
    // Re-enable buttons only if no operation is currently running
    if (!isWorking) {
        [setupSoundcheckButton setEnabled:YES];
        [scanButton setEnabled:YES];
        [connectButton setEnabled:YES];
        // Toggle button handled in updateToggleSoundcheckButtonLabel
    }
    [self updateToggleSoundcheckButtonLabel];
    [self updateSetupSoundcheckButtonLabel];
    [self refreshLiveSetupValidation];
}

- (void)updateToggleSoundcheckButtonLabel {
    auto& extension = ReaperExtension::Instance();
    bool enabled = extension.IsSoundcheckModeEnabled();
    if (enabled) {
        [toggleSoundcheckButton setTitle:@"🎧 Soundcheck Mode"];
    } else {
        [toggleSoundcheckButton setTitle:@"🎙️ Live Mode"];
    }
    
    // Only enable when live setup validates against Wing + REAPER state.
    if (liveSetupValidated && !isWorking) {
        [toggleSoundcheckButton setEnabled:YES];
    } else {
        [toggleSoundcheckButton setEnabled:NO];
    }
}

- (void)updateSetupSoundcheckButtonLabel {
    auto& extension = ReaperExtension::Instance();
    const bool existing_patching_detected = liveSetupValidated || extension.IsSoundcheckModeEnabled();
    if (existing_patching_detected) {
        [setupSoundcheckButton setTitle:@"Setup Live Recording (Overwrite)"];
        [setupSoundcheckDescriptionLabel setStringValue:@"⚠ Existing live patching detected; running setup will overwrite routing"];
        [setupSoundcheckDescriptionLabel setTextColor:[NSColor systemOrangeColor]];
    } else {
        [setupSoundcheckButton setTitle:@"Setup Live Recording"];
        [setupSoundcheckDescriptionLabel setStringValue:@"Configure live recording tracks and routing"];
        [setupSoundcheckDescriptionLabel setTextColor:[NSColor secondaryLabelColor]];
    }
}

- (void)refreshLiveSetupValidation {
    // Never block the UI thread with network/OSC queries.
    if (validationInProgress || isWorking) {
        return;
    }

    auto& extension = ReaperExtension::Instance();
    auto& config = extension.GetConfig();

    std::string candidate_ip;
    NSString* selectedIP = [self selectedWingIP];
    if (selectedIP && [selectedIP length] > 0) {
        candidate_ip = std::string([selectedIP UTF8String]);
    } else if (!config.wing_ip.empty()) {
        candidate_ip = config.wing_ip;
    }

    if (!extension.IsConnected() && candidate_ip.empty()) {
        liveSetupValidated = NO;
        [self updateToggleSoundcheckButtonLabel];
        [self updateSetupSoundcheckButtonLabel];
        [self appendToLog:@"Live setup validation: NOT READY — no Wing IP available to validate against.\n"];
        return;
    }

    validationInProgress = YES;
    WingConnectorWindowController* blockSelf = self;
    dispatch_async(dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0), ^{
        bool connected_now = extension.IsConnected();
        std::string details;

        // Only validate against Wing when already connected. Explicit connect/disconnect
        // is controlled by the UI button to keep behavior predictable.
        if (!connected_now) {
            config.wing_ip = candidate_ip;
            details = "Not connected to Wing. Connect first to validate setup.";
            dispatch_async(dispatch_get_main_queue(), ^{
                blockSelf->validationInProgress = NO;
                blockSelf->isConnected = NO;
                [blockSelf->statusLabel setStringValue:@"⚪ Not Connected"];
                    blockSelf->liveSetupValidated = NO;
                    [blockSelf updateToggleSoundcheckButtonLabel];
                    [blockSelf updateSetupSoundcheckButtonLabel];
                    [blockSelf appendToLog:[NSString stringWithFormat:@"Live setup validation: NOT READY — %s\n",
                                            details.c_str()]];
                });
            return;
        }

        bool valid = extension.ValidateLiveRecordingSetup(details);
        dispatch_async(dispatch_get_main_queue(), ^{
            blockSelf->validationInProgress = NO;
            // Avoid changing state while another operation is in progress.
            if (blockSelf->isWorking) {
                return;
            }
            blockSelf->isConnected = extension.IsConnected() ? YES : NO;
            [blockSelf->statusLabel setStringValue:blockSelf->isConnected ? @"🟢 Connected" : @"⚪ Not Connected"];
            blockSelf->liveSetupValidated = valid ? YES : NO;
            [blockSelf updateToggleSoundcheckButtonLabel];
            [blockSelf updateSetupSoundcheckButtonLabel];
            [blockSelf appendToLog:[NSString stringWithFormat:@"Live setup validation: %s — %s\n",
                                    valid ? "READY" : "NOT READY",
                                    details.c_str()]];
        });
    });
}

- (void)setWorkingState:(BOOL)working {
    isWorking = working;
    [setupSoundcheckButton setEnabled:!working];
    [scanButton setEnabled:!working];
    [connectButton setEnabled:!working];
    // Toggle button state depends on both working state and setup completion
    [self updateToggleSoundcheckButtonLabel];
    [self updateSetupSoundcheckButtonLabel];
}

- (void)appendToLog:(NSString*)message {
    NSString* currentText = [activityLogView string];
    NSString* newText = [currentText stringByAppendingString:message];
    [activityLogView setString:newText];
    
    // Scroll to bottom
    NSRange range = NSMakeRange([[activityLogView string] length], 0);
    [activityLogView scrollRangeToVisible:range];
}

// ===== WING DISCOVERY =====

- (void)startDiscoveryScan {
    [wingDropdown removeAllItems];
    [wingDropdown addItemWithTitle:@"Scanning..."];
    [[wingDropdown itemAtIndex:0] setEnabled:NO];
    [wingDropdown setEnabled:NO];
    [scanButton setTitle:@"Scanning..."];
    [scanButton setEnabled:NO];
    
    WingConnectorWindowController* blockSelf = self;
    dispatch_async(dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0), ^{
        auto wings = ReaperExtension::Instance().DiscoverWings(1500);
        NSMutableArray* items = [NSMutableArray array];
        NSMutableArray* ips   = [NSMutableArray array];
        for (const auto& w : wings) {
            NSString* label;
            if (!w.name.empty() && !w.model.empty()) {
                label = [NSString stringWithFormat:@"%s \u2013 %s  (%s)",
                         w.name.c_str(), w.model.c_str(), w.console_ip.c_str()];
            } else if (!w.name.empty()) {
                label = [NSString stringWithFormat:@"%s  (%s)",
                         w.name.c_str(), w.console_ip.c_str()];
            } else if (!w.model.empty()) {
                label = [NSString stringWithFormat:@"%s  (%s)",
                         w.model.c_str(), w.console_ip.c_str()];
            } else {
                label = [NSString stringWithUTF8String:w.console_ip.c_str()];
            }
            [items addObject:label];
            [ips   addObject:[NSString stringWithUTF8String:w.console_ip.c_str()]];
        }
        dispatch_async(dispatch_get_main_queue(), ^{
            [blockSelf populateDropdownWithItems:items ips:ips];
            // Only re-enable scan button if no other operation is running
            if (!blockSelf->isWorking) {
                [blockSelf->scanButton setTitle:@"Scan"];
                [blockSelf->scanButton setEnabled:YES];
            }
        });
    });
}

- (void)populateDropdownWithItems:(NSArray*)items ips:(NSArray*)ips {
    [discoveredIPs release];
    discoveredIPs = [[NSMutableArray arrayWithArray:ips] retain];
    [wingDropdown removeAllItems];
    if ([items count] == 0) {
        [wingDropdown addItemWithTitle:@"No Wings found — press Scan"];
        [[wingDropdown itemAtIndex:0] setEnabled:NO];
        [wingDropdown setEnabled:NO];
        [self appendToLog:@"\u2717 No Wing consoles found on the network. Is the Wing powered on and reachable?\n"];
    } else {
        [wingDropdown setEnabled:YES];
        for (NSString* title in items) {
            [wingDropdown addItemWithTitle:title];
        }
        [wingDropdown selectItemAtIndex:0];
        // Immediately apply the first found Wing IP to config
        auto& config = ReaperExtension::Instance().GetConfig();
        config.wing_ip = std::string([[ips objectAtIndex:0] UTF8String]);
        [self appendToLog:[NSString stringWithFormat:@"Found %d Wing console(s):\n", (int)[items count]]];
        for (NSString* title in items) {
            [self appendToLog:[NSString stringWithFormat:@"  \u2022 %@\n", title]];
        }
        [self refreshLiveSetupValidation];
    }
}

- (void)onWingDropdownChanged:(id)sender {
    NSInteger idx = [wingDropdown indexOfSelectedItem];
    if (discoveredIPs && idx >= 0 && idx < (NSInteger)[discoveredIPs count]) {
        auto& config = ReaperExtension::Instance().GetConfig();
        config.wing_ip = std::string([[discoveredIPs objectAtIndex:idx] UTF8String]);
        [self appendToLog:[NSString stringWithFormat:@"Selected Wing: %@\n",
                          [wingDropdown titleOfSelectedItem]]];
    }
}

- (void)onScanClicked:(id)sender {
    [self appendToLog:@"\n=== Re-scanning for Wing consoles ===\n"];
    [self startDiscoveryScan];
}

- (void)onConnectClicked:(id)sender {
    if (isWorking || validationInProgress) return;

    WingConnectorWindowController* blockSelf = self;
    [self setWorkingState:YES];

    dispatch_async(dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0), ^{
        auto& extension = ReaperExtension::Instance();
        auto& config = extension.GetConfig();

        if (extension.IsConnected()) {
            extension.DisconnectFromWing();
            dispatch_async(dispatch_get_main_queue(), ^{
                blockSelf->liveSetupValidated = NO;
                [blockSelf appendToLog:@"Disconnected from Wing.\n"];
                [blockSelf setWorkingState:NO];
                [blockSelf updateConnectionStatus];
            });
            return;
        }

        NSString* wingIP = [blockSelf selectedWingIP];
        if (wingIP && [wingIP length] > 0) {
            config.wing_ip = std::string([wingIP UTF8String]);
        }

        if (config.wing_ip.empty()) {
            dispatch_async(dispatch_get_main_queue(), ^{
                [blockSelf appendToLog:@"✗ No Wing selected. Press Scan and choose a console first.\n"];
                [blockSelf setWorkingState:NO];
                [blockSelf updateConnectionStatus];
            });
            return;
        }

        if (!extension.ConnectToWing()) {
            dispatch_async(dispatch_get_main_queue(), ^{
                [blockSelf appendToLog:[NSString stringWithFormat:@"✗ Connection failed to %s.\n",
                                        config.wing_ip.c_str()]];
                [blockSelf setWorkingState:NO];
                [blockSelf updateConnectionStatus];
            });
            return;
        }

        dispatch_async(dispatch_get_main_queue(), ^{
            [blockSelf appendToLog:[NSString stringWithFormat:@"✓ Connected to %s\n",
                                    config.wing_ip.c_str()]];
            [blockSelf setWorkingState:NO];
            [blockSelf updateConnectionStatus];
        });
    });
}

- (NSString*)selectedWingIP {
    if (!wingDropdown || !discoveredIPs) {
        fprintf(stderr, "[WING] ERROR: selectedWingIP called but UI not initialized (wingDropdown=%p, discoveredIPs=%p)\n", 
                wingDropdown, discoveredIPs);
        return nil;
    }
    NSInteger idx = [wingDropdown indexOfSelectedItem];
    if (idx >= 0 && idx < (NSInteger)[discoveredIPs count]) {
        return [discoveredIPs objectAtIndex:idx];
    }
    return nil;
}

- (void)onSetupSoundcheckClicked:(id)sender {
    if (isWorking) return;  // Prevent re-entrant clicks

    auto& extension = ReaperExtension::Instance();
    const bool existing_patching_detected = liveSetupValidated || extension.IsSoundcheckModeEnabled();
    if (existing_patching_detected) {
        NSAlert* overwriteAlert = [[NSAlert alloc] init];
        [overwriteAlert setMessageText:@"Existing Live Setup Detected"];
        [overwriteAlert setInformativeText:@"Wing and REAPER routing appear to already be configured for live recording.\n\nRunning setup again will overwrite current patching/routing. Continue?"];
        [overwriteAlert addButtonWithTitle:@"Continue"];
        [overwriteAlert addButtonWithTitle:@"Cancel"];
        NSInteger result = [overwriteAlert runModal];
        [overwriteAlert release];
        if (result != NSAlertFirstButtonReturn) {
            [self appendToLog:@"Live setup cancelled by user (existing patching preserved).\n"];
            return;
        }
    }

    // Update output mode from UI
    auto& config = extension.GetConfig();
    config.soundcheck_output_mode = ([outputModeControl selectedSegment] == 0) ? "USB" : "CARD";
    
    [self appendToLog:[NSString stringWithFormat:@"\n=== Setting up Virtual Soundcheck (%s mode) ===\n",
                      config.soundcheck_output_mode.c_str()]];
    [self setWorkingState:YES];
    [self runSetupSoundcheckFlow];
}

- (void)onToggleSoundcheckClicked:(id)sender {
    if (isWorking) return;  // Prevent re-entrant clicks
    [self appendToLog:@"\n=== Toggling Soundcheck Mode ===\n"];
    [self setWorkingState:YES];
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
    BOOL enabled = ([midiActionsControl selectedSegment] == 1);
    
    if (enabled) {
        // When enabling, always force registration to ensure shortcuts are written
        extension.EnableMidiActions(true);
        
        // Verify it actually worked
        if (!extension.AreMidiShortcutsRegistered()) {
            [self appendToLog:@"⚠️ Warning: MIDI shortcuts may not have been registered correctly\n"];
            [midiActionsControl setSelectedSegment:0];
        } else {
            [self appendToLog:@"✓ MIDI actions enabled - Wing buttons now control REAPER\n"];
        }
    } else {
        // When disabling, remove shortcuts
        extension.EnableMidiActions(false);
        [self appendToLog:@"MIDI actions disabled\n"];
    }
}

- (void)runSetupSoundcheckFlow {
    // Capture UI values on the main thread before dispatching
    NSString* wingIP = [self selectedWingIP];
    WingConnectorWindowController* blockSelf = self;

    fprintf(stderr, "[WING] runSetupSoundcheckFlow: Starting background thread. wingIP=%s\n", 
            wingIP ? [wingIP UTF8String] : "nil");
    fflush(stderr);

    dispatch_async(dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0), ^{
        fprintf(stderr, "[WING] Background thread started\n"); fflush(stderr);
        
        auto& extension = ReaperExtension::Instance();
        fprintf(stderr, "[WING] Got extension instance\n"); fflush(stderr);

        if (!extension.IsConnected()) {
            fprintf(stderr, "[WING] Not connected, attempting auto-connect\n"); fflush(stderr);
            dispatch_async(dispatch_get_main_queue(), ^{ [blockSelf appendToLog:@"Not connected — attempting to connect automatically...\n"]; });
            if (!wingIP) {
                fprintf(stderr, "[WING] ERROR: No Wing IP selected\n"); fflush(stderr);
                dispatch_async(dispatch_get_main_queue(), ^{
                    [blockSelf appendToLog:@"✗ No Wing selected. Press Scan to find Wing consoles on the network.\n"];
                    [blockSelf setWorkingState:NO];
                    [blockSelf updateConnectionStatus];
                });
                return;
            }
            auto& config = extension.GetConfig();
            config.wing_ip = std::string([wingIP UTF8String]);
            fprintf(stderr, "[WING] Connecting to %s:2223...\n", config.wing_ip.c_str()); fflush(stderr);
            if (!extension.ConnectToWing()) {
                fprintf(stderr, "[WING] ERROR: ConnectToWing failed\n"); fflush(stderr);
                dispatch_async(dispatch_get_main_queue(), ^{
                    [blockSelf appendToLog:@"✗ Auto-connect failed. Check that the Wing is reachable.\n"];
                    [blockSelf setWorkingState:NO];
                    [blockSelf updateConnectionStatus];
                });
                return;
            }
            fprintf(stderr, "[WING] Connected successfully\n"); fflush(stderr);
            dispatch_async(dispatch_get_main_queue(), ^{
                [blockSelf appendToLog:@"✓ Auto-connected to Wing\n"];
                [blockSelf updateConnectionStatus];
            });
        } else {
            fprintf(stderr, "[WING] Already connected\n"); fflush(stderr);
        }

        dispatch_async(dispatch_get_main_queue(), ^{ [blockSelf appendToLog:@"Getting Wing channels for live recording setup...\n"]; });
        fprintf(stderr, "[WING] Calling GetAvailableChannels...\n"); fflush(stderr);

        auto channels = extension.GetAvailableChannels();
        fprintf(stderr, "[WING] GetAvailableChannels returned %zu channels\n", channels.size()); fflush(stderr);
        
        if (channels.empty()) {
            fprintf(stderr, "[WING] ERROR: No channels found\n"); fflush(stderr);
            dispatch_async(dispatch_get_main_queue(), ^{
                [blockSelf appendToLog:@"✗ No channels with sources found.\n"];
                [blockSelf setWorkingState:NO];
            });
            return;
        }

        __block auto blockChannels = channels;
        __block bool confirmed = false;
        __block bool setup_soundcheck = true;  // Will be set by dialog
        fprintf(stderr, "[WING] Showing channel selection dialog on main thread...\n"); fflush(stderr);
        dispatch_sync(dispatch_get_main_queue(), ^{
            [blockSelf appendToLog:[NSString stringWithFormat:@"Found %d channels with sources\n", (int)blockChannels.size()]];
            fprintf(stderr, "[WING] Calling ShowChannelSelectionDialog...\n"); fflush(stderr);
            confirmed = ShowChannelSelectionDialog(
                blockChannels,
                "Select Channels for Virtual Soundcheck",
                "Choose which channels to configure for virtual soundcheck.",
                setup_soundcheck
            );
            fprintf(stderr, "[WING] Dialog returned, confirmed=%d, setup_soundcheck=%d\n", confirmed, setup_soundcheck); fflush(stderr);
        });

        if (!confirmed) {
            fprintf(stderr, "[WING] User cancelled\n"); fflush(stderr);
            dispatch_async(dispatch_get_main_queue(), ^{
                [blockSelf appendToLog:@"Cancelled by user\n"];
                [blockSelf setWorkingState:NO];
            });
            return;
        }

        int selectedCount = 0;
        for (const auto& ch : blockChannels) {
            if (ch.selected) selectedCount++;
        }
        fprintf(stderr, "[WING] %d channels selected\n", selectedCount); fflush(stderr);

        if (selectedCount == 0) {
            fprintf(stderr, "[WING] ERROR: No channels selected\n"); fflush(stderr);
            dispatch_async(dispatch_get_main_queue(), ^{
                [blockSelf appendToLog:@"✗ No channels selected\n"];
                [blockSelf setWorkingState:NO];
            });
            return;
        }

        fprintf(stderr, "[WING] Setting up live recording on main thread...\n"); fflush(stderr);
        dispatch_sync(dispatch_get_main_queue(), ^{
            [blockSelf appendToLog:[NSString stringWithFormat:@"Setting up live recording for %d channels...\n", selectedCount]];
            fprintf(stderr, "[WING] Calling SetupSoundcheckFromSelection (setup_soundcheck=%d)...\n", setup_soundcheck); fflush(stderr);
            extension.SetupSoundcheckFromSelection(blockChannels, setup_soundcheck);
            fprintf(stderr, "[WING] SetupSoundcheckFromSelection complete\n"); fflush(stderr);
            [blockSelf appendToLog:@"✓ Live recording setup complete\n"];
            [blockSelf setWorkingState:NO];
            [blockSelf refreshLiveSetupValidation];
        });
        fprintf(stderr, "[WING] runSetupSoundcheckFlow complete\n"); fflush(stderr);
    });
}

- (void)runToggleSoundcheckModeFlow {
    // Capture UI values on the main thread before dispatching
    NSString* wingIP = [self selectedWingIP];
    WingConnectorWindowController* blockSelf = self;

    dispatch_async(dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0), ^{
        auto& extension = ReaperExtension::Instance();

        if (!extension.IsConnected()) {
            dispatch_async(dispatch_get_main_queue(), ^{ [blockSelf appendToLog:@"Not connected — attempting to connect automatically...\n"]; });
            if (!wingIP) {
                dispatch_async(dispatch_get_main_queue(), ^{
                    [blockSelf appendToLog:@"✗ No Wing selected. Press Scan to find Wing consoles on the network.\n"];
                    [blockSelf setWorkingState:NO];
                    [blockSelf updateConnectionStatus];
                });
                return;
            }
            auto& config = extension.GetConfig();
            config.wing_ip = std::string([wingIP UTF8String]);
            if (!extension.ConnectToWing()) {
                dispatch_async(dispatch_get_main_queue(), ^{
                    [blockSelf appendToLog:@"✗ Auto-connect failed. Check that the Wing is reachable.\n"];
                    [blockSelf setWorkingState:NO];
                    [blockSelf updateConnectionStatus];
                });
                return;
            }
            dispatch_async(dispatch_get_main_queue(), ^{
                [blockSelf appendToLog:@"✓ Auto-connected to Wing\n"];
                [blockSelf updateConnectionStatus];
            });
        }

        // CRITICAL: ToggleSoundcheckMode() shows message boxes, which MUST run on main thread
        dispatch_async(dispatch_get_main_queue(), ^{
            [blockSelf appendToLog:@"Toggling soundcheck mode...\n"];
            
            extension.ToggleSoundcheckMode();
            bool enabled = extension.IsSoundcheckModeEnabled();
            
            if (enabled) {
                [blockSelf appendToLog:@"✓ Soundcheck mode ENABLED (using playback inputs)\n"];
            } else {
                [blockSelf appendToLog:@"✓ Soundcheck mode DISABLED (using live inputs)\n"];
            }
            [blockSelf updateToggleSoundcheckButtonLabel];
            [blockSelf setWorkingState:NO];
            [blockSelf refreshLiveSetupValidation];
        });
    });
}

@end

extern "C" {

void ShowWingConnectorDialog() {
    // Write debug output to system stderr (visible in REAPER console)
    fprintf(stderr, "\n🔧 [WING] ShowWingConnectorDialog() called\n");
    fflush(stderr);
    
    // Static to keep controller alive in MRC - window doesn't retain it by default
    static WingConnectorWindowController* controller = nil;
    
    // Must run UI operations on main thread
    dispatch_async(dispatch_get_main_queue(), ^{
        // NO @autoreleasepool here - it would drain objects that need to live longer
        fprintf(stderr, "🔧 [WING] Creating WingConnectorWindowController\n");
        fflush(stderr);
        
        // If window already exists and is visible, just bring it to front
        if (controller && [[controller window] isVisible]) {
            [[controller window] makeKeyAndOrderFront:nil];
            return;
        }
        
        // Create new controller (retained by static variable)
        if (controller) {
            [controller release];
        }
        controller = [[WingConnectorWindowController alloc] init];
        
        fprintf(stderr, "🔧 [WING] Showing window\n");
        fflush(stderr);
        
        [[controller window] makeKeyAndOrderFront:nil];
        
        fprintf(stderr, "🔧 [WING] Appending init message\n");
        fflush(stderr);
        
        [controller appendToLog:@"🔧 Window initialization complete\n"];
    });
}

} // extern "C"

#endif // __APPLE__
