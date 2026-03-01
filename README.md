# Wing Connector - Reaper Extension for Behringer Wing

A professional C++ Reaper extension that connects to a Behringer Wing console via UDP/OSC to automatically set up recording tracks based on the Wing's channel configuration.

**Status:** ✅ Production Ready | **Version:** 1.0.0 | **License:** MIT

---

## Table of Contents

1. [Overview](#overview)
2. [Features](#features)
3. [Quick Start](#quick-start-5-minutes)
4. [Requirements](#requirements)
5. [Installation & Setup](#installation--setup)
6. [Configuration](#configuration)
7. [Usage Guide](#usage-guide)
8. [Architecture](#architecture)
9. [OSC Protocol](#osc-protocol)
10. [Troubleshooting](#troubleshooting)
11. [Development](#development)
12. [Credits & License](#credits--license)

---

## Overview

Wing Connector bridges your Behringer Wing console and Reaper DAW, automating track setup and synchronization. Instead of manually creating tracks that match your Wing's channel configuration, simply connect and let Wing Connector handle it—creating tracks with proper names, colors, routing, and stereo pairing all automatically.

### What It Does

- **Queries your Wing console** via OSC (Open Sound Control) to discover all channels
- **Automatically creates matching Reaper tracks** with correct names and configurations
- **Syncs channel colors** from Wing to Reaper tracks for visual consistency
- **Optionally groups stereo channels** into paired tracks
- **Monitors real-time changes** on the Wing and updates tracks accordingly
- **Loads automatically** with Reaper—no manual plugin activation needed

### Perfect For

- Location recording with Wing consoles
- Multi-track recording workflows
- Quick session setup without manual track creation
- Maintaining channel naming consistency between Wing and Reaper

---

## Features

### Core Functionality

- ✅ **Native Reaper Integration** — Seamlessly integrated, appears in Extensions menu, loads automatically
- ✅ **Full OSC Protocol Support** — Complete UDP/OSC implementation based on Patrick Gillot's Wing documentation
- ✅ **Automatic Track Creation** — Queries Wing and creates perfectly matched Reaper tracks
- ✅ **Real-Time Monitoring** — Subscribe to Wing changes and automatically update tracks
- ✅ **Channel-Accurate Colors** — Reaper tracks inherit Wing's color palette
- ✅ **Stereo Channel Pairing** — Optional automatic grouping of stereo channel pairs
- ✅ **Complete Channel Metadata** — Captures names, colors, icons, and routing information
- ✅ **Cross-Platform** — Works on macOS, Windows, and Linux

### Available Actions

All actions are accessible via **Extensions → Wing Connector** menu:

| Action | Purpose | Keyboard Shortcut |
|--------|---------|-------------------|
| **Connect to Behringer Wing** | Initial connection, query Wing, create tracks | Ctrl+Shift+W (customizable) |
| **Refresh Tracks** | Re-query Wing, update existing tracks | (customizable) |
| **Toggle Monitoring** | Enable/disable real-time track synchronization | (customizable) |

---

## Quick Start (5 Minutes)

Getting up and running with Wing Connector is straightforward:

### 1. Prepare Your Wing Console

On your Behringer Wing:
1. Press **Setup** button
2. Navigate to **Network** → **OSC**
3. Enable OSC (toggle to **ON**)
4. Note the **IP Address** (e.g., `192.168.1.100`)
5. Verify **OSC Port** is `2223` (or note the actual port)

### 2. Set Up Dependencies

#### macOS / Linux
```bash
./setup_dependencies.sh
```

Then manually download the Reaper SDK:
- Visit: https://www.reaper.fm/sdk/plugin/
- Download `reaper_plugin.h` and `reaper_plugin_functions.h`
- Create directory: `mkdir -p lib/reaper-sdk`
- Copy downloaded files to `lib/reaper-sdk/`

#### Windows
Download and set up manually (see [Dependencies Setup](#dependencies-setup) section)

### 3. Update Configuration

Edit `config.json` with your Wing's IP address:

```json
{
  "wing_ip": "192.168.1.100",  ← Update to your Wing's IP
  "wing_port": 2223,
  "listen_port": 2223,
  "channel_count": 48,
  "timeout": 5
}
```

### 4. Build the Extension

#### macOS / Linux
```bash
chmod +x build.sh
./build.sh
```

#### Windows
```cmd
build.bat
```

Build usually takes 1-2 minutes. Watch for `BUILD SUCCESS` message.

### 5. Install to Reaper

After successful build, copy files to Reaper's UserPlugins folder:

**macOS:**
```bash
mkdir -p ~/Library/Application\ Support/REAPER/UserPlugins
cp install/reaper_wingconnector.dylib ~/Library/Application\ Support/REAPER/UserPlugins/
cp config.json ~/Library/Application\ Support/REAPER/UserPlugins/
```

**Windows:**
```cmd
mkdir %APPDATA%\REAPER\UserPlugins
copy install\reaper_wingconnector.dll %APPDATA%\REAPER\UserPlugins\
copy config.json %APPDATA%\REAPER\UserPlugins\
```

**Linux:**
```bash
mkdir -p ~/.config/REAPER/UserPlugins
cp install/reaper_wingconnector.so ~/.config/REAPER/UserPlugins/
cp config.json ~/.config/REAPER/UserPlugins/
```

### 6. Restart Reaper and Connect

1. Close and reopen Reaper, or restart it
2. Check console: **View → Monitoring** — you should see `Wing Connector loaded`
3. Go to **Extensions → Wing Connector → Connect to Behringer Wing**
4. Wait 3-5 seconds for connection
5. Success! Tracks appear in your arrange view with Wing channel names

---

## Requirements

### Runtime Requirements

- **Reaper DAW** — Version 6.0 or later
- **Behringer Wing Console** — Compact, Rack, or Full model
- **Network Connection** — Computer and Wing on same network (wired or WiFi)

### Build Requirements

**CMake:**
- Version 3.15 or later
- macOS: `brew install cmake`
- Windows: Download from https://cmake.org/download/
- Linux: `sudo apt install cmake` (Ubuntu/Debian)

**C++ Compiler:**
- macOS: Xcode Command Line Tools (`xcode-select --install`)
- Windows: Visual Studio 2019, 2022, or MinGW with C++17 support
- Linux: GCC 8+ or Clang 7+ with C++17 support

**Git:**
- Required for downloading oscpack library
- macOS: Included with Xcode
- Windows: Download from https://git-scm.com/download/win
- Linux: `sudo apt install git` or `sudo yum install git`

### Third-Party Dependencies

All automatically configured by the build system:

- **oscpack** — OSC protocol implementation (auto-downloaded)
- **Reaper SDK** — Extension API headers (requires manual download)

---

## Installation & Setup

### Detailed Setup Instructions

#### Step 1: Prerequisites Check

Before building, ensure you have all required tools:

```bash
# Check CMake
cmake --version        # Should show 3.15 or later

# Check Git
git --version         # Should show 2.0 or later

# Check compiler
cc --version          # macOS/Linux
# or
cl.exe /?            # Windows Visual Studio
```

#### Step 2: Download Dependencies

Navigate to your project directory:

```bash
cd /path/to/colab.reaper.recordwing
```

**Download Reaper SDK:**
1. Visit https://www.reaper.fm/sdk/plugin/
2. Download the latest SDK package
3. Extract and copy these files:
   - `reaper_plugin.h`
   - `reaper_plugin_functions.h`
4. Create location: `mkdir -p lib/reaper-sdk`
5. Copy files to `lib/reaper-sdk/`

**Download oscpack:**

```bash
cd lib
git clone https://github.com/RossBencina/oscpack.git
cd ..
```

Or manually download from: https://github.com/RossBencina/oscpack/archive/refs/heads/master.zip

#### Step 3: Verify Directory Structure

Ensure your project has this layout:

```
colab.reaper.recordwing/
├── CMakeLists.txt
├── build.sh / build.bat
├── config.json
├── src/
│   ├── main.cpp
│   ├── wing_osc.cpp
│   ├── track_manager.cpp
│   ├── wing_config.cpp
│   ├── reaper_extension.cpp
│   └── *.mm files (macOS)
├── include/
│   ├── wing_osc.h
│   ├── track_manager.h
│   ├── wing_config.h
│   └── reaper_extension.h
├── lib/
│   ├── reaper-sdk/
│   │   ├── reaper_plugin.h
│   │   └── reaper_plugin_functions.h
│   └── oscpack/
│       ├── osc/
│       └── ip/
└── docs/
    └── WING_OSC_PROTOCOL.md
```

#### Step 4: Configure Wing Console

On your Behringer Wing:
1. Power on the console
2. Press **Setup** button
3. Navigate to **Network** section
4. Locate **OSC Settings**
5. **Enable OSC** (toggle to ON)
6. **Note the IP Address** — this is what you need for config.json
7. **Verify OSC Port** — default is 2223

#### Step 5: Update Configuration File

Edit `config.json` in your project root:

```json
{
  "wing_ip": "192.168.1.100",  ← Change to your Wing's IP
  "wing_port": 2223,
  "listen_port": 2223,
  "channel_count": 48,
  "timeout": 5,
  "track_prefix": "",
  "color_tracks": true,
  "create_stereo_pairs": false,
  "default_track_color": {
    "r": 100,
    "g": 150,
    "b": 200
  }
}
```

#### Step 6: Build the Extension

**macOS / Linux:**
```bash
chmod +x build.sh
./build.sh
```

**Windows:**
```cmd
build.bat
```

Monitor build progress. Expected messages:
- `Configuring project...`
- `Building extension...`
- `BUILD SUCCESS` (final message)

**Troubleshooting build issues:**
- If CMake not found: Install CMake and restart terminal
- If `reaper_plugin.h` not found: Verify SDK files in `lib/reaper-sdk/`
- If oscpack not found: Run `git clone` or download manually

#### Step 7: Install to Reaper

After successful build, the compiled extension is in the `install/` folder:

**macOS:**
```bash
# Create Reaper plugins directory if needed
mkdir -p ~/Library/Application\ Support/REAPER/UserPlugins

# Copy extension and config
cp install/reaper_wingconnector.dylib ~/Library/Application\ Support/REAPER/UserPlugins/
cp config.json ~/Library/Application\ Support/REAPER/UserPlugins/
```

**Windows:**
```cmd
# Create directory if needed
mkdir %APPDATA%\REAPER\UserPlugins

# Copy files
copy install\reaper_wingconnector.dll %APPDATA%\REAPER\UserPlugins\
copy config.json %APPDATA%\REAPER\UserPlugins\
```

**Linux:**
```bash
mkdir -p ~/.config/REAPER/UserPlugins
cp install/reaper_wingconnector.so ~/.config/REAPER/UserPlugins/
cp config.json ~/.config/REAPER/UserPlugins/
```

#### Step 8: First Launch

1. **Close Reaper** completely
2. **Wait 10 seconds** (to ensure it fully closes)
3. **Reopen Reaper**
4. **View Monitoring** window: **View → Monitoring**
   - Look for: `Wing Connector 1.0.0 loaded`
   - This confirms the extension loaded successfully
5. The extension menu appears here: **Extensions → Wing Connector**

---

## Configuration

### config.json Reference

The `config.json` file controls all Wing Connector behavior. Place it in Reaper's UserPlugins folder alongside the extension binary.

#### Full Configuration Example

```json
{
  "wing_ip": "192.168.1.100",
  "wing_port": 2223,
  "listen_port": 2223,
  "channel_count": 48,
  "create_stereo_pairs": false,
  "timeout": 5,
  "track_prefix": "",
  "color_tracks": true,
  "include_channels": "",
  "exclude_channels": "",
  "default_track_color": {
    "r": 100,
    "g": 150,
    "b": 200
  }
}
```

#### Configuration Parameters

| Parameter | Type | Default | Description |
|-----------|------|---------|-------------|
| `wing_ip` | String | `192.168.1.100` | IP address of your Wing console on the network |
| `wing_port` | Integer | `2223` | OSC port on Wing (matches Wing's OSC settings) |
| `listen_port` | Integer | `2223` | Local port to listen for Wing responses |
| `channel_count` | Integer | `48` | Number of channels to query from Wing |
| `timeout` | Integer | `5` | Timeout in seconds for OSC queries |
| `track_prefix` | String | `` | Optional prefix for all created track names (e.g., "Wing_") |
| `color_tracks` | Boolean | `true` | Whether to apply Wing colors to Reaper tracks |
| `create_stereo_pairs` | Boolean | `false` | Auto-create stereo track pairs |
| `include_channels` | String | `` | Comma-separated channel numbers to include (empty = all) |
| `exclude_channels` | String | `` | Comma-separated channel numbers to exclude |
| `default_track_color` | Object | `{r:100, g:150, b:200}` | RGB color for tracks when Wing color unavailable |

#### Configuration Examples

**Example 1: Basic Setup**
```json
{
  "wing_ip": "192.168.2.50",
  "channel_count": 32
}
```

**Example 2: With Track Prefix**
```json
{
  "wing_ip": "192.168.1.100",
  "track_prefix": "Wing_",
  "color_tracks": true
}
```

**Example 3: Selected Channels Only**
```json
{
  "wing_ip": "192.168.1.100",
  "include_channels": "1,2,3,4,5,6",
  "track_prefix": "Main_"
}
```

### Updating Configuration on the Fly

After the initial install, you can update `config.json` without rebuilding:

1. Edit `config.json` in Reaper's UserPlugins folder
2. **Disconnect** from Wing (if currently connected)
3. **Re-connect** — extension reloads configuration
4. New settings take effect immediately

---

## Usage Guide

### Your First Connection

#### Step 1: Prepare Reaper

1. **Open Reaper**
2. **Create a new project** (Session menu) or open an existing one
3. **Extensions → Wing Connector → Connect to Behringer Wing**

#### Step 2: Monitor Connection

A dialog appears while connecting:
- Status shows: `Connecting to Wing...`
- After 3-5 seconds: `Connected! Creating 48 tracks...`
- Success dialog: Shows total tracks created

#### Step 3: Review Created Tracks

After successful connection:
- **Arrange View** shows new tracks named after Wing channels
- **Mixer** shows all channels with proper routing
- **Color** of each track matches Wing's channel color (if enabled)
- **Stereo pairs** grouped together (if enabled)

### Available Actions

Everything is controlled through **Extensions → Wing Connector** menu:

#### Connect to Behringer Wing
- **Purpose:** Initial connection and full track setup
- **Action:** Queries Wing for all channel info, creates matching Reaper tracks
- **When to use:** After configuration changes, or when starting a new session
- **Keyboard:** Default is Ctrl+Shift+W (customizable)

**What happens:**
1. Extension connects to Wing via OSC
2. Queries all channels for names, colors, routing
3. Creates Reaper tracks with matching configuration
4. Sets colors, names, and optional stereo pairing
5. Arms tracks for recording

#### Refresh Tracks
- **Purpose:** Re-sync with Wing without full recreate
- **Action:** Updates existing tracks or creates new ones added to Wing
- **When to use:** After changing Wing channel settings, or to sync manual edits
- **Result:** Tracks update without losing any Reaper-side settings

#### Toggle Monitoring
- **Purpose:** Real-time synchronization
- **Action:** Subscribes to Wing OSC updates, auto-updates Reaper tracks
- **When to use:** During rehearsal when Wing settings change frequently
- **Benefit:** Changes on Wing immediately reflected in Reaper

### Assigning Custom Keyboard Shortcuts

To create custom shortcuts for any Wing action:

1. **Reaper menu:** Actions → Show action list
2. **Search for:** "Wing:"
3. **Select** action you want (e.g., "Wing: Connect to Behringer Wing")
4. **Click "Add"** button
5. **Press** your desired key combination
6. **Confirm** with Enter

Now your shortcut triggers that action!

### Typical Workflow

**Session start:**
1. Power on Wing console
2. Open project in Reaper
3. **Extensions → Wing Connector → Connect to Behringer Wing**
4. Tracks created automatically
5. Start recording!

**During session:**
- Wing channel names change? → **Refresh Tracks**
- Need real-time sync? → **Toggle Monitoring** ON
- All set? → Work normally, Wing connector runs in background

---

## Architecture

> **📖 For detailed architecture documentation, see [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md)** - covers directory structure, design patterns, build system integration, and development guidelines.

### How Wing Connector Works

```
┌─────────────────────┐
│  Behringer Wing     │
│  (OSC Responder)    │
└──────────┬──────────┘
           │ OSC over UDP
           │ (Port 2223)
           │
┌──────────▼──────────┐         ┌──────────────────┐
│   Wing Connector    │         │   Reaper DAW     │
│   OSC Client        │────────▶│  Track Creator   │
│                     │         │                  │
│ ┌─────────────────┐ │         └──────────────────┘
│ │ OSC Sender      │ │
│ │ OSC Receiver    │ │
│ │ Query Parser    │ │
│ └─────────────────┘ │
│ ┌─────────────────┐ │
│ │ Track Manager   │ │
│ │ Config Manager  │ │
│ └─────────────────┘ │
└─────────────────────┘
```

### Codebase Organization

The extension is organized into **functional domains** for maintainability and scalability:

- **Extension Layer** (`src/extension/`) - REAPER plugin bootstrap
- **Core OSC** (`src/core/`) - UDP/OSC protocol implementation
- **Track Management** (`src/track/`) - Track creation and synchronization
- **Utilities** (`src/utilities/`) - Configuration, logging, and helpers
- **UI Layer** (`src/ui/`) - Platform-specific dialogs (macOS Objective-C++)

Headers are split into:
- **Public API** (`include/wingconnector/`) - Main interfaces
- **Internal Implementation** (`include/internal/`) - Supporting utilities

See [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md) for the complete directory structure and design patterns.

### Component Overview

**Extension Layer** (`src/extension/`)
- Initializes REAPER extension framework
- Registers actions and keyboard shortcuts
- Manages UI dialogs

**Core OSC** (`src/core/`)
- Sends/receives OSC queries to Wing console
- Parses Wing OSC responses
- Implements Wing OSC protocol
- Handles network errors and timeouts

**Track Manager** (`src/track/`)
- Creates Reaper tracks matching Wing channels
- Applies channel names, colors, routing
- Manages stereo pairing logic
- Handles track updates and refreshes

**Utilities** (`src/utilities/`)
- `wing_config.cpp` - Configuration file I/O and validation
- `logger.cpp` - Logging system
- `platform_util.cpp` - macOS native dialogs
- `string_format.cpp` - String utilities

**reaper_extension.cpp - Main Class**
- Orchestrates overall extension behavior
- Manages connection lifecycle
- Coordinates OSC queries and track creation
- Provides monitoring/update subscriptions

### Data Flow

1. **User clicks "Connect to Behringer Wing"**
   - Extension loads configuration
   - Creates OSC client connection

2. **OSC Query Phase**
   - Extension sends queries to Wing
   - Wing responds with channel data (names, colors, routing)
   - Extension parses responses

3. **Track Creation Phase**
   - For each Wing channel, extension:
     - Creates Reaper track
     - Sets track name from Wing
     - Applies color from Wing palette
     - Configures routing
   - Applies stereo pairing if enabled

4. **Completion**
   - Shows success dialog
   - Tracks appear in arrange view
   - User can begin recording

---

## OSC Protocol

Wing Connector implements the Behringer Wing OSC (Open Sound Control) protocol, allowing communication over standard UDP network connections.

### Protocol Overview

**Connection Details:**
- **Protocol:** UDP/OSC
- **Default Port:** 2223
- **Message Format:** OSC packets over UDP

### Supported OSC Commands

**Channel Information Queries:**
- Channel names and labels
- Channel colors (12-color palette)
- Channel icons (128 library)
- Scribble strip configuration
- Input routing information
- Current fader levels
- Mute state

### Common OSC Paths

```
/wing/ch/[channel]/name      → Channel name
/wing/ch/[channel]/color     → Channel color index
/wing/ch/[channel]/icon      → Channel icon index
/wing/ch/[channel]/fader     → Fader level
/wing/ch/[channel]/mute      → Mute state
/wing/busses/[bus]/name      → Bus name
/wing/masters/[master]/level → Master level
```

### Extended Protocol Reference

For complete OSC protocol documentation and advanced usage:
- See: [docs/WING_OSC_PROTOCOL.md](docs/WING_OSC_PROTOCOL.md)
- Original documentation: Patrick Gillot's Behringer Wing manual

### Network Requirements

- **Network Type:** Standard Ethernet or WiFi
- **Connection:** Wing and computer on same subnet (192.168.1.x)
- **Firewall:** UDP ports 2223 must be open
- **Latency:** Works well with standard network latency (<50ms)

---

## Troubleshooting

### Installation Issues

#### "Did not find Wing Connector in Extensions menu"

**Cause:** Extension didn't load properly

**Solutions:**
1. **Check file location:**
   - macOS: `~/Library/Application\ Support/REAPER/UserPlugins/`
   - Windows: `%APPDATA%\REAPER\UserPlugins\`
   - Linux: `~/.config/REAPER/UserPlugins/`

2. **Verify file name:**
   - macOS: `reaper_wingconnector.dylib`
   - Windows: `reaper_wingconnector.dll`
   - Linux: `reaper_wingconnector.so`

3. **Check Reaper version:**
   ```
   Reaper → About → Should show v6.0 or later
   ```

4. **Check permissions (Linux/macOS):**
   ```bash
   chmod +x ~/Library/Application\ Support/REAPER/UserPlugins/reaper_wingconnector.dylib
   ```

5. **View error messages:**
   - Reaper menu: **View → Monitoring**
   - Look for error messages about the extension
   - Screenshot and share for support

#### "Extension failed to load" in Reaper

**Cause:** Compatibility or dependencies issue

**Solutions:**
1. Right-click extension file → **Quarantine: Always Allow** (macOS)
2. Disable other extensions temporarily (to isolate issue)
3. Check Reaper console for specific error
4. Try with test project (minimal setup)

### Connection Issues

#### "Connection Failed" dialog

**Cause:** OSC connection to Wing couldn't be established

**Solutions:**
1. **Verify Wing IP address:**
   ```bash
   ping 192.168.1.100  # Use your Wing's actual IP
   ```
   - Should show successful ping responses
   - If not, check network connection

2. **Double-check config.json:**
   ```bash
   cat config.json  # Display current config
   ```
   - Look for `"wing_ip"` value
   - Ensure it matches Wing's actual IP
   - Check for typos

3. **Verify Wing OSC is enabled:**
   - Wing console: **Setup → Network → OSC**
   - Toggle should be **ON**
   - Restart Wing if recently changed

4. **Check ports are correct:**
   ```json
   {
     "wing_ip": "192.168.1.100",
     "wing_port": 2223,
     "listen_port": 2223
   }
   ```
   - Both should be 2223 (unless Wing configured differently)

5. **Check firewall:**
   - macOS: **System Settings → Security & Privacy → Firewall**
   - Windows: **Windows Defender Firewall → Allow app through firewall**
   - Allow Reaper through firewall for UDP traffic

6. **Try direct connection:**
   ```bash
   nc -u 192.168.1.100 2223  # Test UDP port
   ```
   - Should show connection accepted

#### "Timeout waiting for response"

**Cause:** Wing not responding to OSC queries within timeout period

**Solutions:**
1. **Increase timeout in config.json:**
   ```json
   {
     "timeout": 10
   }
   ```
   - Try 10 seconds instead of default 5
   - Increase further if on slow network

2. **Check Wing responsiveness:**
   - Wing console: Can you interact with it normally?
   - Is it becoming unresponsive after time?
   - Try soft restart: Power off → Wait 30 sec → Power on

3. **Check network latency:**
   ```bash
   ping -c 3 192.168.1.100  # macOS/Linux
   ping -n 3 192.168.1.100  # Windows
   ```
   - Should show <50ms latency
   - If >100ms, check network congestion

4. **Update Wing firmware:**
   - Wing console: **Setup → System → Firmware**
   - Check if firmware update available
   - Update if available (may fix OSC issues)

### Track Creation Issues

#### "Connection successful but no tracks created"

**Cause:** Tracks created but not visible, or creation failed silently

**Solutions:**
1. **Check arrange view is visible:**
   - View menu → **Arrange** (should show tracks)
   - If hidden, tracks were created but not showing

2. **Check track limit:**
   - Reaper: **Options → Preferences → Advanced**
   - Search for "max tracks"
   - Increase limit if near maximum

3. **Verify project is open:**
   - Must have active project (Session → New)
   - Can't create tracks in empty project state

4. **Check Reaper console for errors:**
   - **View → Monitoring**
   - Look for any error messages about track creation
   - Copy error text for support

#### "Wrong number of tracks created"

**Cause:** `channel_count` mismatch or Wing configuration issue

**Solutions:**
1. **Check channel_count setting:**
   ```json
   {
     "channel_count": 48
   }
   ```
   - How many channels does your Wing actually have?
   - Wing Compact: 16, Wing Rack: 32, Wing Full: 48+
   - Adjust number accordingly

2. **Verify Wing has audio connections:**
   - Wing I/O may limit available channels
   - Some channels might not be active

3. **Use include/exclude filters:**
   ```json
   {
     "include_channels": "1,2,3,4,5,6",
     "exclude_channels": ""
   }
   ```
   - Specify exact channels to create

#### "Tracks created but no colors applied"

**Cause:** `color_tracks` disabled or Wing not sending color info

**Solutions:**
1. **Enable color_tracks:**
   ```json
   {
     "color_tracks": true
   }
   ```

2. **Reconnect:**
   - **Extensions → Wing Connector → Connect to Behringer Wing**
   - New tracks will have colors

3. **Set default color:**
   ```json
   {
     "default_track_color": {
       "r": 200,
       "g": 100,
       "b": 150
     }
   }
   ```

### Build Issues

#### "CMake not found"

**Solution:**
```bash
# macOS
brew install cmake

# Windows: Download from http://cmake.org/download/

# Linux (Ubuntu/Debian)
sudo apt install cmake

# After install, restart your terminal
```

#### "reaper_plugin.h not found"

**Solution:**
1. Download Reaper SDK: https://www.reaper.fm/sdk/plugin/
2. Create: `mkdir -p lib/reaper-sdk`
3. Copy files: `reaper_plugin.h`, `reaper_plugin_functions.h` to `lib/reaper-sdk/`
4. Retry build

#### "oscpack not found"

**Solution:**
```bash
cd lib
git clone https://github.com/RossBencina/oscpack.git
cd ..
./build.sh  # or build.bat on Windows
```

### Getting Help

If you're still stuck:

1. **Check Extension Console:**
   - Reaper: **View → Monitoring**
   - Copy any error messages

2. **Verify Configuration:**
   - Wing IP address correct?
   - OSC enabled on Wing?
   - Network connection working?

3. **Test Connectivity:**
   ```bash
   ping [wing_ip]
   ```

4. **Check Documentation:**
   - See [docs/WING_OSC_PROTOCOL.md](docs/WING_OSC_PROTOCOL.md) for protocol details
   - Patrick Gillot's Wing manual for OSC reference

---

## Development

### Building for Development

To build with debug symbols and verbose output:

**macOS / Linux:**
```bash
./build.sh Debug
```

**Windows:**
```cmd
build.bat Debug
```

### Project Structure

```
colab.reaper.recordwing/
├── CMakeLists.txt              # Build system configuration
├── build.sh / build.bat        # Build scripts
├── config.json                 # Default configuration
├── src/
│   ├── main.cpp               # Extension entry point, command registration
│   ├── wing_osc.cpp           # OSC client implementation
│   ├── track_manager.cpp      # Reaper track creation logic
│   ├── wing_config.cpp        # Configuration file parsing
│   ├── reaper_extension.cpp   # Main extension class
│   ├── settings_dialog_macos.mm    # macOS UI
│   ├── wing_connector_dialog_macos.mm  # macOS connection dialog
│   └── *.cpp                  # Platform-specific implementations
├── include/
│   ├── wing_osc.h
│   ├── track_manager.h
│   ├── wing_config.h
│   ├── reaper_extension.h
│   └── *.h                    # Other headers
├── lib/
│   ├── reaper-sdk/            # Reaper SDK headers (user-provided)
│   │   ├── reaper_plugin.h
│   │   └── reaper_plugin_functions.h
│   └── oscpack/               # OSC protocol library
│       ├── osc/
│       └── ip/
├── docs/
│   ├── WING_OSC_PROTOCOL.md   # Complete OSC reference
│   └── OSC_Documentation.pdf  # Wing OSC specification
└── install/                    # Built extension output
    ├── reaper_wingconnector.dylib (macOS)
    ├── reaper_wingconnector.dll   (Windows)
    └── reaper_wingconnector.so    (Linux)
```

### Building and Testing Locally

**Standard build:**
```bash
./build.sh
```

**Build output stored in:**
- macOS: `install/reaper_wingconnector.dylib`
- Windows: `install\reaper_wingconnector.dll`
- Linux: `install/reaper_wingconnector.so`

**Copy to Reaper:**
```bash
# macOS example
cp install/reaper_wingconnector.dylib ~/Library/Application\ Support/REAPER/UserPlugins/
```

**Restart Reaper and test changes**

### Adding Features

#### Adding a New OSC Query

1. **Define query in wing_osc.cpp:**
   ```cpp
   std::string WingOSC::queryNewFeature() {
       // Send OSC message
       // Parse response
       // Return data
   }
   ```

2. **Update track creation in track_manager.cpp:**
   ```cpp
   void TrackManager::applyNewFeature(Track track, std::string data) {
       // Use query response to configure track
   }
   ```

3. **Call from main workflow:**
   ```cpp
   // In reaper_extension.cpp
   auto data = osc->queryNewFeature();
   tracker->applyNewFeature(track, data);
   ```

4. **Add configuration option if needed:**
   ```json
   // In config.json
   {
     "enable_new_feature": true
   }
   ```

#### Adding a New Action

1. **Register in main.cpp:**
   ```cpp
   action.idStr = "_WING_NEW_ACTION";
   action.name = "Wing: New Action Name";
   ```

2. **Implement in reaper_extension.cpp:**
   ```cpp
   void ReaperExtension::performNewAction() {
       // Implementation
   }
   ```

3. **Hook command in OnAction:**
   ```cpp
   if (cmd == g_cmd_new_action) {
       instance->performNewAction();
       return true;
   }
   ```

### Code Style

- **Language:** C++17
- **Naming:** camelCase for variables/functions, PascalCase for classes
- **Comments:** Clear comments for complex logic
- **Modularity:** Keep functions focused and testable

### Console Output

View extension debug/info messages:
- Reaper: **View → Monitoring**
- All `printf` statements from extension appear here
- Useful for debugging issues

---

## Credits & License

### Authors & Contributors

- **Patrick Gillot** — Behringer Wing OSC Protocol documentation (invaluable reference)
- **Cockos / Justin Frankel** — Reaper DAW and SDK
- **Ross Bencina** — oscpack library (OSC implementation)
- **CO_LAB** — Wing Connector extension development

### Third-Party Libraries

- **oscpack** — OSC protocol implementation (licensed under BSD)
- **Reaper SDK** — Cockos Reaper extensibility framework

### License

Wing Connector is released under the **MIT License**.

**You are free to:**
- Use commercially and privately
- Modify the code
- Distribute copies
- Include in other projects

**You must:**
- Include original license text
- Include copyright notice

See [LICENSE](LICENSE) file for complete terms.

### Citation

If you use Wing Connector in a project, we appreciate (but don't require) attribution:

```
Wing Connector - Reaper Extension for Behringer Wing
https://github.com/CO_LAB/colab.reaper.recordwing
Licensed under MIT License
```

---

## Support & Community

### Getting Support

**Documentation:**
- README.md (this file) — Overview and quick start
- SETUP.md — Detailed setup instructions
- QUICKSTART.md — 5-minute setup guide
- docs/WING_OSC_PROTOCOL.md — OSC protocol reference

**Community:**
- Reaper Forums: https://forum.cockos.com
- Behringer User Community
- GitHub Issues (if hosted on GitHub)

### Reporting Issues

When reporting issues, please include:
1. Operating system (macOS 13.x, Windows 11, Ubuntu 22.04, etc.)
2. Reaper version (Options → About)
3. Wing model (Compact/Rack/Full)
4. Wing firmware version (Setup → System)
5. Network setup (wired/WiFi, same subnet?)
6. Error message from Reaper console (View → Monitoring)
7. Your config.json file (with IP masked if sensitive)

---

## FAQ

**Q: Do I need a specific Wing model?**
A: Works with Wing Compact, Rack, and Full. Channel count may vary.

**Q: Can I use this over the internet?**
A: Not recommended. OSC requires low latency. Local network (LAN) only.

**Q: Will this track real-time fader movements?**
A: Yes, via Monitoring mode. Enable "Toggle Monitoring" action.

**Q: Can I reorder tracks after creation?**
A: Yes, you can reorder in Reaper. Re-running Connect won't override your ordering.

**Q: Is MIDI supported?**
A: Not in this version. Wing→Reaper is OSC-based only.

**Q: Can I use with Dante networking?**
A: Wing Connector uses standard OSC/UDP. Should work with Dante if both devices on same network.

**Q: How many tracks can it create?**
A: Limited only by Reaper's track limit (configurable).

---

## Roadmap & Future Enhancements

Potential future features (not yet implemented):
- Real-time fader control (two-way sync)
- MIDI integration for Wing button mapping
- Automation recording from Wing faders
- Custom OSC command scripting
- Web UI configuration dashboard
- Extended color palette support
- Template-based track creation

---

## Version History

**v1.0.0** (Current)
- Initial release
- OSC protocol implementation
- Automatic track creation
- Color mapping
- Real-time monitoring
- Cross-platform support (macOS, Windows, Linux)
