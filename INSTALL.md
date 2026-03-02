# Wing Connector Installation Guide

## macOS Installation

### Option 1: Quick Install (Recommended)

The easiest way to install Wing Connector on macOS:

```bash
./install_macos.sh
```

This interactive script will:
- ✓ Check for REAPER installation
- ✓ Build the plugin (if needed)
- ✓ Install to the correct location
- ✓ Offer to restart REAPER
- ✓ Show next steps

### Option 2: Package Installer (.pkg)

For distributing to other users or a double-click installation:

1. **Create the installer package:**
   ```bash
   ./create_installer_pkg.sh
   ```

2. **Install the package:**
   - Double-click `WingConnector-1.0.0.pkg`
   - Follow the installation prompts
   - Restart REAPER

### Option 3: Manual Installation

If you prefer to install manually:

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
