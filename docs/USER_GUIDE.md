# AUDIOLAB.wing.reaper.virtualsoundcheck User Guide

Practical guide for daily operation in REAPER with a Behringer WING.

## 1. Validate Installation

1. Restart REAPER after installing AUDIOLAB.wing.reaper.virtualsoundcheck.
2. Confirm menu entry exists: `Extensions -> AUDIOLAB.wing.reaper.virtualsoundcheck`.
3. Open: `Extensions -> AUDIOLAB.wing.reaper.virtualsoundcheck -> Connect to Behringer Wing`.

![Extensions actions](images/actions-menu.png)

If the menu is missing, verify plugin files are in your REAPER `UserPlugins` folder for your OS.

## 2. Connect to WING and Fetch Channels

1. In the AUDIOLAB.wing.reaper.virtualsoundcheck dialog, enter WING IP and port.
2. Use default port `2223` unless changed on the console.
3. Start connect/fetch.
4. Wait for channel discovery and track creation/update.

![AUDIOLAB.wing.reaper.virtualsoundcheck dialog](images/wing-connector.png)

Expected result:

- Connection succeeds
- Channel metadata is retrieved
- Tracks are created or refreshed in REAPER

## 3. Confirm Connected State

After a successful connect, the dialog should indicate active status and show progress/log feedback.

![Connected state](images/wing-connected.png)

If connection fails:

- check WING OSC settings
- verify network path and firewall
- verify IP/port values

## 4. Use Optional Features

From the same dialog flow you can:

- refresh tracks from current WING state
- enable/disable live monitoring behavior
- configure virtual soundcheck routing
- toggle soundcheck mode (ALT source switching)

![Actions/config view](images/action-config.png)

## 5. Recommended Session Workflow

1. Connect and fetch channels at session start.
2. Verify names/colors/routing in REAPER.
3. Record as usual.
4. When needed, configure virtual soundcheck and toggle mode.
5. Refresh tracks if channel metadata changes on WING.

## 6. MIDI Button Control (Optional)

Built-in CC mapping used by AUDIOLAB.wing.reaper.virtualsoundcheck:

| CC # | Action |
|------|--------|
| 20 | Set Marker |
| 21 | Previous Marker |
| 22 | Next Marker |
| 23 | Record |
| 24 | Stop |
| 25 | Play |
| 26 | Pause |

Requirements:

- Message type must be `MIDI CC` (not Note On)
- MIDI channel must be channel 1
- Button press value must be `> 0`

For full setup steps on WING and REAPER, see:

- [snapshots/README.md](../snapshots/README.md)

## 7. Troubleshooting

- No connection:
  - WING and computer must be on same network
  - OSC lock must be disabled on WING
  - OSC port must match plugin config
- Plugin not visible in REAPER:
  - verify plugin binary exists in `UserPlugins`
  - restart REAPER after install
- No track updates:
  - rerun channel fetch/refresh
  - verify channel source configuration on WING

## Related Docs

- [INSTALL.md](../INSTALL.md)
- [QUICKSTART.md](../QUICKSTART.md)
- [SETUP.md](../SETUP.md)
- [docs/ARCHITECTURE.md](ARCHITECTURE.md)
