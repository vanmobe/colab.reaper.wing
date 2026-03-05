# Quick Start (5 Minutes)

This guide gets you from install to first successful channel sync.

## 1. Prepare the WING

On the Behringer WING console:

1. Open `Setup -> Remote -> OSC`.
2. Ensure OSC remote lock is disabled.
3. Confirm OSC port (default `2223`).
4. Note the WING IP address.
5. If you plan to use button MIDI actions, set `External MIDI Control` to `USB`.

## 2. Install AUDIOLAB.wing.reaper.virtualsoundcheck

Use the installer for your OS from GitHub Releases:

- https://github.com/vanmobe/colab.reaper.wing/releases

See [INSTALL.md](INSTALL.md) if needed.

## 3. Launch REAPER and Connect

1. Restart REAPER after installation.
2. Open `Extensions -> AUDIOLAB.wing.reaper.virtualsoundcheck -> Connect to Behringer Wing`.
3. Enter the WING IP/port.
4. Run channel fetch / connect in the dialog.

Expected result:

- AUDIOLAB.wing.reaper.virtualsoundcheck connects successfully.
- REAPER tracks are created or refreshed from WING channel data.

## 4. Optional: Configure Virtual Soundcheck

Inside the AUDIOLAB.wing.reaper.virtualsoundcheck dialog:

1. Configure soundcheck output mode (`USB` or `CARD`).
2. Run virtual soundcheck setup.
3. Use soundcheck mode toggle (ALT source switching) when needed.

## 5. Optional: Configure Wing Buttons

Default mapping used by the plugin:

- `CC 20` -> Set Marker
- `CC 21` -> Previous Marker
- `CC 22` -> Next Marker
- `CC 23` -> Record
- `CC 24` -> Stop
- `CC 25` -> Play
- `CC 26` -> Pause

Use `MIDI CC` on channel 1 from WING custom controls.

Full setup steps:

- [snapshots/README.md](snapshots/README.md)

## Troubleshooting Checklist

- Confirm WING and computer are on the same network.
- Verify IP/port in AUDIOLAB.wing.reaper.virtualsoundcheck settings.
- Check firewall rules for UDP traffic.
- Confirm REAPER loaded the extension from `UserPlugins`.

## Next Docs

- [docs/USER_GUIDE.md](docs/USER_GUIDE.md)
- [SETUP.md](SETUP.md)
- [docs/WING_OSC_PROTOCOL.md](docs/WING_OSC_PROTOCOL.md)
