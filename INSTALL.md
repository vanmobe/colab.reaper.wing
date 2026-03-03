# Wing Connector Installation Guide

> **Platform Support:** Wing Connector currently supports macOS only. Windows and Linux support is planned for future releases.

> **⚠️ Disclaimer:** This software is provided as-is for use at your own risk. No guarantees or official support are provided. See the main [README](README.md) for full warranty information.

## macOS Installation

Choose one of two installation methods:

### Method 1: Package Installer (Recommended for Most Users)
### Method 2: Build from Source (For Developers)

---

## Method 1: Package Installer (Recommended)

**Best for:** End users who want a simple, double-click installation.

### Download and Install

1. **Download the installer:**
   - **[📦 Download WingConnector-1.0.0.pkg](../releases/WingConnector-1.0.0.pkg)**
   - Or create it locally from source:
     ```bash
     ./create_installer_pkg.sh
     ```
   - Package location: `releases/WingConnector-1.0.0.pkg`

2. **Install the package:**
   - Double-click `WingConnector-1.0.0.pkg`
   - Follow the installation prompts
   - The installer will place files in the correct REAPER UserPlugins directory

3. **Restart REAPER**

4. **Done!** Skip to [First Time Setup](#first-time-setup)

---

## Method 2: Build from Source

**Best for:** Developers who want to modify the code or build from the latest source.

### Option A: Automated Build & Install

The easiest way to build and install from source:

```bash
./install_macos.sh
```

This interactive script will:
- ✓ Check for REAPER installation
- ✓ Build the plugin (if needed)
- ✓ Install to the correct location
- ✓ Offer to restart REAPER
- ✓ Show next steps

### Option B: Manual Build & Install

If you prefer to build and install manually:

1. **Build the plugin:**
   ```bash
   ./build.sh
   ```

2. **Copy files:**
   ```bash
   cp build/reaper_wingconnector.dylib ~/Library/Application\ Support/REAPER/UserPlugins/
   cp config.json ~/Library/Application\ Support/REAPER/UserPlugins/
   ```

3. **Restart REAPER**

## First Time Setup

After installation:

1. **Open REAPER**

2. **Open Wing Connector:**
   - Go to: `Extensions → Wing Connector`

3. **Configure your Wing:**
   - Enter your Behringer Wing's IP address
   - Default: `192.168.10.210`
   - Click "Save Settings"

4. **Connect:**
   - Click "Connect to Wing"
   - Your tracks will be automatically created

---

## Optional: Configure Wing Buttons to Control REAPER

Make your workflow even faster by controlling Wing Connector actions directly from your Behringer Wing console buttons!

### Quick Setup (Using Snap File)

**Coming Soon:** A pre-configured REAPER snapshot file will be available for easy import.

Once available:
1. **Download:** [wing-control-buttons.snap](../snapshots/) *(file will be uploaded soon)*
2. **Import to REAPER:**
   - Go to: `Actions → Show action list`
   - Click `Import/export` → `Import`
   - Select the downloaded snap file
3. **Done!** Your Wing buttons are now mapped to REAPER actions

### Manual Setup (Available Now)

If you want to set this up manually now, or prefer custom configurations:

**📖 See detailed instructions:** [snapshots/README.md](snapshots/README.md)

The manual setup guide covers:
- Configuring Wing custom buttons to send MIDI commands
- Setting up REAPER to receive MIDI from Wing
- Mapping Wing buttons to Wing Connector actions
- Recommended button layouts
- Troubleshooting MIDI connections

**Quick overview:**
1. Configure Wing custom buttons to send MIDI notes (via USB)
2. Enable Wing MIDI input in REAPER preferences
3. Map MIDI notes to Wing Connector actions in REAPER
4. Press Wing buttons to trigger actions!

**Suggested mappings:**
- **Button 1** → Connect to Wing
- **Button 2** → Refresh Tracks
- **Button 3** → Toggle Monitoring
- Plus transport controls (Record, Play, Stop)

---

## Troubleshooting

### Plugin not appearing in REAPER?

1. Check plugin location:
   ```bash
   ls -la ~/Library/Application\ Support/REAPER/UserPlugins/reaper_wingconnector.dylib
   ```

2. Verify REAPER plugins path:
   - Open REAPER
   - Go to: `Preferences → Plug-ins → ReaScript`
   - Check that the UserPlugins folder is listed

### Connection issues?

1. **Verify Wing IP address:**
   - Check your Wing's network settings
   - Make sure your Mac is on the same network

2. **Check config file:**
   ```bash
   cat ~/Library/Application\ Support/REAPER/UserPlugins/config.json
   ```

3. **Check firewall:**
   - System Preferences → Security & Privacy → Firewall
   - Allow incoming connections for REAPER

## Uninstallation

To remove Wing Connector:

```bash
rm ~/Library/Application\ Support/REAPER/UserPlugins/reaper_wingconnector.dylib
rm ~/Library/Application\ Support/REAPER/UserPlugins/config.json
```

Then restart REAPER.

## Requirements

- macOS 10.13 (High Sierra) or later
- REAPER 6.0 or later
- CMake (for building from source)
  - Install: `brew install cmake`

## Support

For issues or questions:
- Check [README.md](README.md) for usage instructions
- See [QUICKSTART.md](QUICKSTART.md) for step-by-step guide
- Review [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md) for technical details
