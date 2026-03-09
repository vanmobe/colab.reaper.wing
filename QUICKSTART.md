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

## 6. Optional: Auto-Record Trigger

The extension can auto-start/stop REAPER recording based on signal activity on armed + monitored tracks.

Set these in `config.json`:

- `auto_record_enabled` = `true`
- `auto_record_threshold_db` (example: `-40.0`)
- `auto_record_attack_ms`
- `auto_record_release_ms`
- `auto_record_min_record_ms`

Then reconnect to WING so the monitor loop starts.

UI support:
- Configure these directly in the main plugin window under `Auto Trigger`.
- `Mode` supports `WARNING` (no transport start) and `RECORD`.
- Signal detection is REAPER-based (armed + monitored tracks).
- `Monitor track` can target a specific REAPER track (`0` = auto across armed+monitored tracks).
- `Hold ms` keeps warning/record active briefly after drops to avoid rapid stop/start chatter.
- Live meter preview is shown as `Trigger level` in the dialog.
- Use `Test CC Flash` to verify the WING warning LEDs (buttons 1-4) flash correctly.

## 7. Optional: Route Main LR to SD (CARD 1/2)

Use REAPER action list:

- `AUDIOLAB.wing.reaper.virtualsoundcheck: Route Main LR to CARD 1/2 (SD)`

This sets CARD output routing for SD LR recording based on:

- `sd_lr_group`
- `sd_lr_left_input`
- `sd_lr_right_input`

If `sd_auto_record_with_reaper` is enabled, SD recorder start/stop OSC commands are also sent when auto-record starts/stops.
The plugin now sends a fallback list of SD recorder OSC paths for better firmware compatibility.

## 8. Optional: OSC Notifications

Enable `OSC Out notifications` in the UI (or config) to emit trigger events:

- warning: `osc_warning_path`
- start: `osc_start_path`
- stop: `osc_stop_path`

Destination is configured by `osc_out_host` + `osc_out_port`.

## Troubleshooting Checklist

- Confirm WING and computer are on the same network.
- Verify IP/port in AUDIOLAB.wing.reaper.virtualsoundcheck settings.
- Check firewall rules for UDP traffic.
- Confirm REAPER loaded the extension from `UserPlugins`.

## Next Docs

- [docs/USER_GUIDE.md](docs/USER_GUIDE.md)
- [SETUP.md](SETUP.md)
- [docs/WING_OSC_PROTOCOL.md](docs/WING_OSC_PROTOCOL.md)
