# Wing Connector Releases

This folder contains pre-built installer packages for Wing Connector.

> **⚠️ Disclaimer:** This software is provided as-is for use at your own risk. No guarantees or official support are provided. Test thoroughly before production use. See [../README.md](../README.md#credits-license--warranty) for complete warranty information.

## Current Release

### Version 1.0.0

**File:** [WingConnector-1.0.0.pkg](WingConnector-1.0.0.pkg)

**Platform:** macOS 10.13 (High Sierra) or later

**Installation:**
1. Download the `.pkg` file
2. Double-click to install
3. Follow the installation prompts
4. Restart REAPER
5. Access via `Extensions → Wing Connector`

**What's Included:**
- Wing Connector REAPER extension (`.dylib`)
- Default configuration file (`config.json`)
- Automatic installation to REAPER UserPlugins directory

---

## System Requirements

- **macOS:** 10.13 (High Sierra) or later
- **REAPER:** Version 6.0 or later
- **Behringer Wing Console:** Compact, Rack, or Full model
- **Network:** Computer and Wing on same network

---

## Build Your Own

If you prefer to build from source:

```bash
# From the project root
./create_installer_pkg.sh
```

This will generate a new package in this `releases/` folder.

---

## Support

For installation help, see:
- [Installation Guide](../INSTALL.md)
- [Quick Start Guide](../QUICKSTART.md)
- [Main README](../README.md)

For issues or questions, please open an issue on the project repository.
