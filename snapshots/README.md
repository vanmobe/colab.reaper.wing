# WING Button to REAPER MIDI Mapping

This guide explains how to map Behringer WING custom controls to REAPER actions used with AUDIOLAB.wing.reaper.virtualsoundcheck.

## Fixed MIDI Mapping Used by the Plugin

When `Link Reaper actions to MIDI` is enabled, the plugin listens for these exact MIDI CC messages and triggers these REAPER actions:

| CC # | REAPER Action | Command ID |
|------|---------------|------------|
| 20 | Markers: Insert marker at current position | 40157 |
| 21 | Markers: Go to previous marker/project start | 40172 |
| 22 | Markers: Go to next marker/project end | 40173 |
| 23 | Transport: Record | 1013 |
| 24 | Transport: Stop | 1016 |
| 25 | Transport: Play | 1007 |
| 26 | Transport: Pause | 1008 |

MIDI message requirements:

- Type: `Control Change (CC)`
- Channel: MIDI Channel 1 (`status 0xB0`)
- Trigger condition: value `> 0` (value `0` is ignored as button release)

## 1. Configure WING Custom Controls

On WING:

1. Open `Setup -> Remote -> Custom Controls`.
2. Choose a button slot.
3. Set message type to `MIDI CC` (not Note On).
4. Set destination to `USB`.
5. Set CC numbers to match the table above (20-26).

## 2. Enable WING MIDI Input in REAPER

1. Open `Preferences -> Audio -> MIDI Devices`.
2. Enable WING MIDI input device.
3. Enable input for control messages.

## 3. Map Actions in REAPER

By default, AUDIOLAB.wing.reaper.virtualsoundcheck writes these shortcuts automatically when `Link Reaper actions to MIDI` is ON in the plugin dialog.

If you want to map them manually:

1. Open `Actions -> Show action list`.
2. Select the corresponding action from the table above.
3. Click `Add` in shortcuts.
4. Press the WING button that sends the matching CC number.
5. Save mapping.

## Troubleshooting

- No trigger in REAPER:
  - verify WING sends MIDI over USB
  - verify controls are sending CC on MIDI channel 1
  - verify CC numbers are 20-26 (per mapping table)
  - verify device enabled in REAPER
  - remap shortcut and retest
- Wrong action triggers:
  - ensure each button uses the intended CC number from the table
  - remove duplicate shortcuts in REAPER action list

## Related Docs

- [INSTALL.md](../INSTALL.md)
- [QUICKSTART.md](../QUICKSTART.md)
- [docs/USER_GUIDE.md](../docs/USER_GUIDE.md)
