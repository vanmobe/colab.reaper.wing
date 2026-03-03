# Wing Connector User Guide

**A comprehensive visual guide to using Wing Connector with your Behringer Wing console and REAPER DAW**

**Version:** 1.0.0 | **Platform:** macOS

> **⚠️ Disclaimer:** This software is provided as-is for use at your own risk. No guarantees or official support are provided. Test thoroughly before using in production. See the main [README](../README.md#credits-license--warranty) for complete warranty information.

---

## Table of Contents

1. [Installation Verification](#1-installation-verification)
2. [Wing Console Configuration](#2-wing-console-configuration)
3. [Understanding the Wing Connector Dialog](#3-understanding-the-wing-connector-dialog)
4. [Virtual Soundcheck Setup](#4-virtual-soundcheck-setup)
5. [Using Virtual Soundcheck](#5-using-virtual-soundcheck)
6. [Connecting and Creating Tracks](#6-connecting-and-creating-tracks)
7. [Setting Up Wing Button Control (MIDI)](#7-setting-up-wing-button-control-midi)
8. [Complete Workflow Examples](#8-complete-workflow-examples)
9. [Troubleshooting](#9-troubleshooting)

---

## 1. Installation Verification

After installing Wing Connector, verify it's properly loaded in REAPER.

### Step 1: Check Extensions Menu

![Screenshot 1: Extensions menu](images/01-extensions-menu.png)
> **Screenshot needed:** REAPER's Extensions menu dropdown showing "Wing Connector" listed as an available extension.

**What to look for:**
- Open REAPER
- Go to: `Extensions` (in the menu bar)
- You should see **"Wing Connector"** in the list

**If not visible:** See [Troubleshooting](#9-troubleshooting) section.

---

### Step 2: Verify Plugin Loaded

![Screenshot 2: Console showing loaded message](images/02-console-loaded.png)
> **Screenshot needed:** REAPER's ReaScript console (View → Monitoring) showing "Wing Connector loaded" message on startup.

**What to look for:**
- Go to: `View → Monitoring` (or press Ctrl+Alt+M)
- In the console output, you should see:
  - `Wing Connector loaded`
  - Version information
  - No error messages

**What this means:** The plugin successfully initialized and is ready to use.

---

### Step 3: Available Actions

![Screenshot 3: Wing Connector submenu](images/03-wing-connector-submenu.png)
> **Screenshot needed:** Extensions → Wing Connector submenu expanded, showing all available actions (Connect to Behringer Wing, Refresh Tracks, etc.)

**Available actions:**
- **Connect to Behringer Wing** — Initial connection and track creation
- **Refresh Tracks** — Update existing tracks with Wing changes
- **Toggle Monitoring** — Enable/disable real-time synchronization
- **Setup Virtual Soundcheck** — Configure soundcheck routing
- **Toggle Soundcheck** — Switch between live and playback modes

---

## 2. Wing Console Configuration

Before using Wing Connector, configure your Behringer Wing console for OSC and MIDI communication.

### Step 1: Configure OSC Settings

![Screenshot 4: Wing OSC settings](images/04-wing-osc-settings.png)
> **Screenshot needed:** Behringer Wing Setup → Remote → OSC settings page showing IP address, port 2223, and Remote OSC lock OFF.

**On your Behringer Wing:**
1. Press **Setup** button
2. Navigate to: **Remote** → **OSC**
3. Configure the following:
   - **Remote OSC Lock:** Set to **OFF** (critical!)
   - **OSC Port:** Should be **2223** (default)
   - **IP Address:** Note your Wing's IP (e.g., `192.168.1.100`)

**Why this matters:**
- **OSC Lock OFF:** Allows external control from REAPER
- **Port 2223:** Standard Wing OSC communication port
- **IP Address:** You'll need this to connect from Wing Connector

**Network tip:** Ensure your Mac and Wing are on the same network (wired connection recommended for reliability).

---

### Step 2: Enable MIDI Control (For Wing Buttons)

![Screenshot 5: Wing MIDI settings](images/05-wing-midi-settings.png)
> **Screenshot needed:** Wing Setup → Remote → showing "External MIDI Control" set to USB.

**On your Behringer Wing:**
1. Press **Setup** button
2. Navigate to: **Remote** → **MIDI**
3. Set **External MIDI Control** to: **USB**

**Why this matters:**
- Allows Wing custom buttons to send MIDI commands to REAPER
- Enables hands-on control of REAPER from your Wing console
- Required for Wing button integration (see section 7)

---

## 3. Understanding the Wing Connector Dialog

The Wing Connector dialog is your control center for all plugin operations. Every option explained.

![Screenshot 6: Complete Wing Connector dialog](images/06-wing-connector-dialog-full.png)
> **Screenshot needed:** Full Wing Connector dialog window with ALL sections visible and labeled. Annotate each section: Connection, Output Configuration, MIDI Actions, Soundcheck controls, Activity Log.

### Opening the Dialog

**Method 1:** `Extensions → Wing Connector → Connect to Behringer Wing`  
**Method 2:** Use keyboard shortcut (default: Ctrl+Shift+W)

---

### Connection Section

**IP Address Field**
- Enter your Wing console's IP address
- Example: `192.168.1.100` or `192.168.10.210`
- Find this on Wing: Setup → Network → IP Configuration

**Port Field**
- Default: `2223`
- Only change if you've modified Wing's OSC port
- Must match Wing's OSC Port setting

**"Get Channels" Button**
- Queries your Wing for all channel information
- Creates/updates REAPER tracks automatically
- Shows progress in Activity Log below
- **When to use:** Initial setup, after Wing configuration changes

---

### Output Configuration

**Soundcheck Output: USB / CARD**

This selector determines how REAPER sends audio back to your Wing during virtual soundcheck.

![Screenshot 6b: USB vs CARD selector closeup](images/06b-usb-card-selector.png)
> **Screenshot needed:** Close-up of the "Soundcheck Output" segmented control showing USB and CARD options.

**USB Mode:**
- Audio routes through Wing's USB audio interface
- **Use when:** Your Wing connects to Mac via USB
- **Typical setup:** USB cable from Wing to Mac, using Wing as audio interface
- **Channels:** Uses Wing's USB input channels (typically 64 channels)

**CARD Mode:**
- Audio routes through Wing's SD Card recording interface
- **Use when:** Wing is on network only (no USB connection)
- **Typical setup:** Network connection only, using separate audio interface
- **Channels:** Uses Wing's CARD (SD recording) input channels

**Which to choose?**
- If Wing is your only audio interface: **USB**
- If using separate interface for REAPER: **CARD**
- Test both if unsure—you can change anytime

---

### Optional Features

**Enable MIDI Actions Checkbox**

![Screenshot 6c: MIDI Actions checkbox](images/06c-midi-actions-checkbox.png)
> **Screenshot needed:** Close-up of "Enable MIDI Actions (Wing buttons control REAPER)" checkbox.

**What it does:**
- ☑ **Checked:** Wing custom buttons can trigger REAPER actions
- ☐ **Unchecked:** MIDI control disabled

**When enabled:**
- REAPER listens for MIDI from Wing
- Custom Wing buttons can trigger: Connect, Refresh, Record, Play, etc.
- Requires MIDI setup (see section 7)

**When to enable:**
- You want hands-on control from Wing console
- You've configured Wing custom buttons to send MIDI
- You prefer console-based workflow

---

### Virtual Soundcheck Controls

**"Setup Soundcheck" Button**

![Screenshot 6d: Setup Soundcheck button](images/06d-setup-soundcheck-button.png)
> **Screenshot needed:** "Setup Soundcheck" button (both enabled and disabled states if possible).

**What it does:**
- Configures Wing routing for virtual soundcheck
- Sets up ALT channel sources for playback mode
- Prepares REAPER for multitrack playback through Wing

**When to use:**
- First time setting up virtual soundcheck
- After changing Wing channel configuration
- To reconfigure soundcheck routing

**Enabled when:** You've successfully connected to Wing (Get Channels)  
**Disabled when:** Not connected to Wing

**What happens when clicked:**
1. Prompts for folder location (where recordings are stored)
2. Shows channel selection dialog
3. Configures Wing routing automatically
4. Sets up ALT channels for mode switching

---

**"Toggle Soundcheck" Button**

![Screenshot 6e: Toggle Soundcheck button](images/06e-toggle-soundcheck-button.png)
> **Screenshot needed:** "Toggle Soundcheck" button showing the toggle state.

**What it does:**
- Switches Wing between LIVE and PLAYBACK modes
- Uses Wing's ALT channel feature to swap input sources

**Two modes:**

**LIVE Mode (Normal):**
- Wing uses standard input sources (microphones, instruments)
- ALT channels: OFF
- **Use for:** Actual recording sessions

**PLAYBACK Mode (Soundcheck):**
- Wing uses USB/CARD inputs from REAPER
- ALT channels: ON
- **Use for:** Mixing rehearsal, virtual soundcheck

**When to use:**
- Switch to playback mode for virtual soundcheck
- Switch back to live mode for recording
- Quick toggle without reconfiguring

**Enabled when:** Virtual soundcheck has been set up  
**Disabled when:** Soundcheck not configured yet

---

### Activity Log

![Screenshot 6f: Activity Log section](images/06f-activity-log.png)
> **Screenshot needed:** Activity Log window showing various messages (connection success, channel creation, errors, etc.)

**What it shows:**
- Real-time connection status
- Channel query progress
- Track creation messages
- Error messages and warnings
- Soundcheck setup progress

**Example messages:**
- ✓ `Connected to Wing at 192.168.1.100`
- ✓ `Found 32 channels`
- ✓ `Created track: Kick`
- ✗ `Connection timeout - check IP address`

**How to use:**
- Monitor progress during operations
- Diagnose connection issues
- Verify successful setup

---

## 4. Virtual Soundcheck Setup

Virtual soundcheck allows you to perfect your mix by playing back multitrack recordings through your Wing console as if you were still recording live.

### What is Virtual Soundcheck?

**The concept:**
1. **Record** your live performance to multitrack audio in REAPER
2. **Play back** those tracks through Wing's USB/CARD inputs
3. **Mix** on the Wing console as if musicians were still playing
4. **Refine** your mix without needing musicians present

**Benefits:**
- Practice and perfect mixes after the session
- Train monitor engineers without live musicians
- Create multiple mix versions from one recording
- Fine-tune effects and routing without pressure

---

### How It Works

**Normal Recording Flow:**
```
Microphones → Wing Inputs → USB/CARD → REAPER (Record)
```

**Virtual Soundcheck Flow:**
```
REAPER (Playback) → USB/CARD → Wing Inputs → Mixing
```

**The Magic: ALT Channels**
- Wing's ALT feature lets each channel have two input sources
- **ALT OFF (Live):** Use normal inputs (mics, instruments)
- **ALT ON (Soundcheck):** Use USB/CARD playback from REAPER
- Toggle with one button!

---

### Step 1: Select Recording Folder

![Screenshot 7: Folder selection dialog](images/07-folder-selection.png)
> **Screenshot needed:** macOS folder selection dialog for choosing where soundcheck multitrack recordings are stored.

**When you click "Setup Soundcheck":**
1. Dialog prompts: "Where are your multitrack recordings stored?"
2. Navigate to your REAPER project folder
3. Select the folder containing rendered/bounced tracks
4. Click **Choose**

**Recommended structure:**
```
MyProject/
  ├── MyProject.RPP          (REAPER project)
  ├── MyProject-multitrack/  (Select this folder)
  │   ├── Kick.wav
  │   ├── Snare.wav
  │   ├── Bass.wav
  │   └── ...
```

---

### Step 2: Select Channels to Configure

![Screenshot 8: Channel selection dialog](images/08-channel-selection.png)
> **Screenshot needed:** Channel selection dialog showing list of Wing channels with checkboxes, channel names, source routing info (e.g., "Kick - A:8", "Snare - A:9"), with Select All/Deselect All options and OK/Cancel buttons.

**What you see:**
- List of all Wing channels with active sources
- Channel number and name
- Current input routing (e.g., "A:8", "USR:25")
- Checkbox for each channel
- Select All / Deselect All buttons

**What to select:**
- ✓ **Check:** Channels you want to use in virtual soundcheck
- ☐ **Uncheck:** Channels that don't need soundcheck routing

**Typical selections:**
- ✓ All input channels (vocals, instruments)
- ☐ Effects returns (don't need playback)
- ☐ Aux buses (configured separately)

**Pro tip:** You can always run Setup Soundcheck again to modify selection.

---

### Step 3: Automatic Configuration

![Screenshot 9: Setup progress in log](images/09-soundcheck-setup-log.png)
> **Screenshot needed:** Activity Log showing virtual soundcheck setup progress with messages like "Configuring channel 1...", "Setting ALT source...", "Setup complete".

**What Wing Connector does automatically:**

**For each selected channel:**
1. **Queries current routing** — Reads existing input configuration
2. **Sets normal source** — Preserves your live input (ALT OFF state)
3. **Configures ALT source** — Sets USB/CARD playback (ALT ON state)
4. **Updates channel** — Sends configuration to Wing

**Log messages you'll see:**
```
=== Setting up Virtual Soundcheck (USB mode) ===
Configuring channel 1: Kick
  Normal source: A:8 (maintained)
  ALT source: USB:1 (configured)
✓ Channel 1 configured

Configuring channel 2: Snare
  Normal source: A:9 (maintained)
  ALT source: USB:2 (configured)
✓ Channel 2 configured

...

✓ Virtual Soundcheck setup complete!
All channels configured. Press ALT on Wing to toggle modes.
```

**What gets configured on Wing:**
- ALT input routing for each channel
- USB/CARD input assignments
- Preserved live input settings

---

## 5. Using Virtual Soundcheck

Now that setup is complete, here's how to actually use virtual soundcheck in your workflow.

### Understanding the Two Modes

![Screenshot 10: Wing in Live mode](images/10-wing-live-mode.png)
> **Screenshot needed:** Wing console showing channels in normal/live mode. Highlight a channel's input source showing regular input (e.g., "A:8"), with ALT indicator OFF.

**LIVE MODE (ALT OFF)**
- **Input sources:** Microphones, instruments, line inputs
- **ALT indicator:** OFF (dim/dark)
- **Use for:** Actual recording sessions, live performance
- **What you see on Wing:** Standard input labels (A:1, A:2, etc.)

---

![Screenshot 11: Wing in Soundcheck mode](images/11-wing-soundcheck-mode.png)
> **Screenshot needed:** Wing console showing same channels in soundcheck/playback mode. Highlight input source now showing USB/CARD (e.g., "USB:1"), with ALT indicator ON (bright/lit).

**PLAYBACK MODE (ALT ON)**
- **Input sources:** REAPER playback via USB/CARD
- **ALT indicator:** ON (bright/lit)
- **Use for:** Virtual soundcheck, mix rehearsal
- **What you see on Wing:** USB:1, USB:2 or CARD:1, CARD:2, etc.

**How to toggle:**
- Use Wing Connector's "Toggle Soundcheck" button in REAPER, OR
- Press ALT button on Wing console for channel strips
- All configured channels switch together

---

### REAPER Setup for Playback

![Screenshot 12: REAPER project with soundcheck routing](images/12-reaper-soundcheck-routing.png)
> **Screenshot needed:** REAPER project showing multiple tracks with routing configured to send audio to Wing USB/CARD outputs. Show mixer view with output routing visible (e.g., tracks routed to outputs 1-32 corresponding to Wing inputs).

**Configure REAPER tracks for soundcheck:**

1. **Load your multitrack recording**
   - Open project with recorded tracks
   - Or import individual WAV files

2. **Set output routing:**
   - Track 1 (Kick) → Output to Channel 1&2 (or USB Out 1)
   - Track 2 (Snare) → Output to Channel 3&4 (or USB Out 2)
   - And so on for each track
   - Match REAPER outputs to Wing USB/CARD inputs

3. **Start playback:**
   - Press Play in REAPER
   - Audio flows to Wing via USB/CARD
   - Mix on Wing as if recording live!

**Tip:** Save this routing as a template for future soundcheck sessions.

---

### Typical Soundcheck Workflow

**1. After recording session:**
- Finish recording multitrack audio in REAPER
- Save project

**2. Prepare for soundcheck:**
- Keep Wing connected to Mac
- Switch to playback mode: Click "Toggle Soundcheck" OR press ALT on Wing
- Verify ALT indicators are ON

**3. In REAPER:**
- Configure track outputs to Wing USB/CARD inputs
- Press Play
- Audio plays through Wing console

**4. On Wing console:**
- Mix as normal (faders, EQ, effects, routing)
- Perfect your monitor mixes
- Adjust effects sends
- Fine-tune stereo imaging

**5. When finished:**
- Toggle back to live mode
- Ready for next session!

---

## 6. Connecting and Creating Tracks

The core functionality: automatically creating REAPER tracks from your Wing channel configuration.

### Before Connection

![Screenshot 13: REAPER before connection](images/13-reaper-before-connection.png)
> **Screenshot needed:** REAPER arrange view with empty project or just a few basic tracks (before Wing Connector creates tracks).

**Starting point:**
- New or existing REAPER project
- Few or no tracks
- Ready to record multitrack session

---

### During Connection

![Screenshot 14: Connection in progress](images/14-connection-progress.png)
> **Screenshot needed:** Wing Connector dialog with Activity Log showing connection and track creation in progress.

**What happens:**
1. **Connection established** — Wing Connector connects via OSC
2. **Querying channels** — Requests all channel information
3. **Receiving data** — Wing sends names, colors, routing, etc.
4. **Creating tracks** — REAPER tracks created for each channel
5. **Applying settings** — Names, colors, routing configured

**Watch the Activity Log:**
```
Connecting to 192.168.1.100:2223...
✓ Connected successfully
Querying Wing channels...
Found 32 channels

Creating tracks:
✓ Track 1: Kick (color: Red)
✓ Track 2: Snare (color: Red)
✓ Track 3: Hi-Hat (color: Red)
✓ Track 4: Bass DI (color: Blue)
...
✓ All tracks created successfully!
```

---

### After Connection

![Screenshot 15: REAPER after connection](images/15-reaper-after-connection.png)
> **Screenshot needed:** REAPER arrange view showing all automatically created tracks with Wing channel names, color-coded to match Wing colors (reds, blues, greens, etc.), some tracks grouped as stereo pairs, all tracks armed and ready for recording.

**What you'll see:**
- ✓ **Automatic tracks** — One track per Wing channel
- ✓ **Correct names** — Matching Wing channel labels exactly
- ✓ **Color coding** — REAPER track colors match Wing colors
- ✓ **Stereo pairs** — Linked L/R channels grouped together
- ✓ **Armed for recording** — Tracks ready to record
- ✓ **Input routing** — Configured to receive from Wing

**Track organization:**
- Tracks appear in Wing channel order (1, 2, 3, etc.)
- Stereo channels paired automatically (if configured)
- Colors provide visual consistency between Wing and REAPER

**Pro tip:** You can manually adjust track order, grouping, or colors after connection—Wing Connector won't override manual changes unless you reconnect.

---

## 7. Setting Up Wing Button Control (MIDI)

Control REAPER actions directly from your Wing console custom buttons!

### Overview

**What this enables:**
- Press Wing button → Trigger REAPER action
- No need to touch computer during session
- Hands-on workflow from mixing console

**Common uses:**
- Connect to Wing (session start)
- Start/stop recording
- Play/pause playback
- Toggle soundcheck mode
- Refresh tracks after changes

---

### Step 1: Configure Wing Custom Buttons

![Screenshot 16: Wing Custom Controls setup](images/16-wing-custom-controls.png)
> **Screenshot needed:** Wing Setup → Remote → Custom Controls page, showing configuration for one custom button. Display fields: Type (MIDI), Message Type (Note On), Channel (1), Note Number (60), Value (127), Destination (USB).

**On your Behringer Wing:**

1. **Open Custom Controls:**
   - Press **Setup**
   - Navigate to: **Remote** → **Custom Controls**

2. **Select a button slot:**
   - Choose Custom Button 1-8 (or higher if available)

3. **Configure MIDI message:**
   - **Type:** `MIDI`
   - **Message Type:** `Note On`
   - **Channel:** `1` (or your preferred channel)
   - **Note Number:** `60` (or unique number for each button)
   - **Value:** `127` (maximum)
   - **Destination:** `USB` (sends to computer)

4. **Assign to physical button:**
   - Navigate to button assignment page
   - Map this custom control to a physical Wing button or softkey
   - Label the button (e.g., "REC", "PLAY", "CONNECT")

5. **Repeat for additional buttons:**
   - Button 2: Note 61
   - Button 3: Note 62
   - Button 4: Note 63
   - And so on...

**Recommended button mappings:**

| Wing Button | Note # | Suggested Action |
|-------------|--------|------------------|
| Custom 1 | 60 (C4) | Connect to Wing |
| Custom 2 | 61 (C#4) | Refresh Tracks |
| Custom 3 | 62 (D4) | Toggle Monitoring |
| Custom 4 | 63 (D#4) | Record |
| Custom 5 | 64 (E4) | Play/Stop |
| Custom 6 | 65 (F4) | Toggle Soundcheck |

---

### Step 2: Enable Wing MIDI in REAPER

![Screenshot 17: REAPER MIDI devices](images/17-reaper-midi-devices.png)
> **Screenshot needed:** REAPER Preferences → Audio → MIDI Devices, showing Wing device checked/enabled for input, with "Enable input for control messages" selected.

**In REAPER:**

1. **Open Preferences:**
   - Go to: `Preferences` (Mac: Cmd+,, Windows: Ctrl+P)

2. **Navigate to MIDI Devices:**
   - Select: `Audio` → `MIDI Devices`

3. **Enable Wing MIDI input:**
   - Find your Wing in the device list (e.g., "USB MIDI Device" or "Behringer Wing")
   - ✓ Check **"Enable input from this device"**
   - Set mode to: **"Enable input for control messages"**

4. **Optional - Add Control Surface:**
   - Go to: `Preferences` → `Control/OSC/Web`
   - Click **Add**
   - Type: `MIDI`
   - MIDI Input: Select Wing device
   - Click **OK**

---

### Step 3: Map Wing Buttons to REAPER Actions

![Screenshot 18: REAPER Actions list with MIDI mapping](images/18-reaper-actions-midi-mapping.png)
> **Screenshot needed:** REAPER Actions window (Actions → Show action list) filtered to show "Wing:" actions. One action selected (e.g., "Wing: Connect to Behringer Wing") with the Shortcuts section at bottom showing "Add" button highlighted, and example of MIDI shortcut already assigned (e.g., "Note On C4 Chan 1").

**In REAPER:**

1. **Open Actions List:**
   - Go to: `Actions` → `Show action list`

2. **Find Wing Connector actions:**
   - In filter box, type: `Wing:`
   - You'll see all Wing Connector actions:
     - Wing: Connect to Behringer Wing
     - Wing: Refresh Tracks
     - Wing: Toggle Monitoring
     - Wing: Setup Virtual Soundcheck
     - Wing: Toggle Soundcheck

3. **Assign MIDI to action:**
   - **Select** the action (e.g., "Wing: Connect to Behringer Wing")
   - Click **"Add"** button (in Shortcuts section at bottom)
   - **Press the corresponding button on your Wing**
   - REAPER detects: "Note On C4 Chan 1" (or your note number)
   - Click **OK** to confirm

4. **Repeat for each action** you want to map

5. **Test:**
   - Press button on Wing
   - REAPER action should trigger!
   - Check Activity Log to confirm

**Pro tip:** You can also map Wing buttons to standard REAPER actions like:
- Transport: Record
- Transport: Play/Stop
- Transport: Play/Pause
- View: Toggle mixer visible
- Track: Arm all tracks for recording

---

## 8. Complete Workflow Examples

Real-world workflows showing how everything fits together.

### Workflow A: Recording Session

![Screenshot 19: Complete recording session](images/19-recording-session-complete.png)
> **Screenshot needed:** Split view showing Wing console with labeled channels on left, REAPER on right with corresponding colored tracks with matching names, tracks armed for recording, transport ready.

**Session Setup (5 minutes):**

1. **Power on Wing console**
   - Verify network connection
   - Check OSC settings (lock OFF)

2. **Open REAPER project**
   - New or existing project
   - Set sample rate, bit depth

3. **Connect Wing Connector:**
   - Extensions → Wing Connector → Connect to Behringer Wing
   - Or press Wing custom button (if configured)
   - Watch tracks auto-create

4. **Verify setup:**
   - Check track names match Wing
   - Verify colors match
   - Confirm all tracks armed

5. **Sound check:**
   - Test each input
   - Adjust levels on Wing
   - Monitor in REAPER

6. **Record!**
   - Press Record in REAPER (or Wing button)
   - Perform/record session
   - Stop when complete

---

### Workflow B: Virtual Soundcheck Session

![Screenshot 20: Virtual soundcheck workflow](images/20-virtual-soundcheck-session.png)
> **Screenshot needed:** Split view showing REAPER in playback mode with tracks playing (meters moving), output routing visible, and Wing console showing ALT indicators ON, engineer adjusting mix on the Wing.

**After Recording:**

1. **Prepare multitrack files:**
   - Render/bounce individual tracks if needed
   - Or use existing recorded project

2. **Setup virtual soundcheck:**
   - Click "Setup Soundcheck" in Wing Connector
   - Choose recording folder
   - Select channels to configure
   - Wait for automatic configuration

3. **Configure REAPER routing:**
   - Route each track to corresponding Wing USB/CARD input
   - Track 1 → USB Out 1
   - Track 2 → USB Out 2
   - Etc.

4. **Switch to playback mode:**
   - Click "Toggle Soundcheck" in REAPER
   - Or press ALT on Wing console
   - Verify ALT indicators ON

5. **Start playback:**
   - Press Play in REAPER
   - Audio flows to Wing
   - Mix on Wing console

6. **Practice mixing:**
   - Adjust faders, EQ, effects
   - Try different monitor mixes
   - Experiment with effects sends
   - Perfect your mix!

7. **Return to live mode:**
   - Toggle soundcheck OFF
   - ALT indicators turn OFF
   - Wing back to normal inputs
   - Ready for next session

---

### Workflow C: Continuous Session Updates

**Scenario:** Channel names change during multi-day sessions

1. **Day 1 - Initial setup:**
   - Connect Wing Connector
   - Record session
   - Save project

2. **Between sessions - Changes made:**
   - Wing channel names updated
   - New channels added
   - Some channels removed/changed

3. **Day 2 - Refresh without recreating:**
   - Open existing REAPER project
   - Extensions → Wing Connector → Refresh Tracks
   - Or press Wing button (if configured)

4. **What happens:**
   - Existing tracks updated with new names
   - New Wing channels create new tracks
   - Removed channels don't affect REAPER
   - No loss of existing REAPER-side settings
   - Recording continues seamlessly

---

## 9. Troubleshooting

Common issues and visual solutions.

### Issue: Plugin Not Loading

![Screenshot 21: Common error messages](images/21-error-messages.png)
> **Screenshot needed:** Examples of common error messages: "Connection timeout", "No Wing device found at IP", "Unable to query channels", displayed in Wing Connector Activity Log and/or REAPER console.

**Symptom:** Wing Connector doesn't appear in Extensions menu

**Solutions:**

1. **Check file location:**
   ```bash
   ls -la ~/Library/Application\ Support/REAPER/UserPlugins/reaper_wingconnector.dylib
   ```
   - Should show file exists with proper permissions

2. **Check REAPER version:**
   - Requires REAPER 6.0 or later
   - Check: `Reaper → About REAPER`

3. **View console errors:**
   - `View → Monitoring`
   - Look for load errors or missing dependencies

4. **Reinstall:**
   - Remove dylib and config.json
   - Reinstall from package or rebuild from source

---

### Issue: Connection Timeout

**Symptom:** "Connection timeout - check IP address" in Activity Log

**Solutions:**

1. **Verify IP address:**
   - On Wing: Setup → Network → note IP
   - In Wing Connector: match exactly

2. **Check same network:**
   - Ping Wing from Terminal:
     ```bash
     ping 192.168.1.100
     ```
   - Should get responses

3. **Verify OSC settings:**
   - Wing: Setup → Remote → OSC
   - Remote OSC Lock must be **OFF**
   - Port must be **2223**

4. **Check firewall:**
   - Mac: System Preferences → Security & Privacy → Firewall
   - Allow REAPER to receive connections
   - Allow UDP port 2223

5. **Restart both devices:**
   - Restart REAPER
   - Restart Wing console
   - Try connecting again

---

### Issue: No Channels Found

**Symptom:** "Found 0 channels" or "No channels with sources found"

**Solutions:**

1. **Check Wing inputs:**
   - Ensure some channels have active input sources configured
   - Wing Connector only queries channels with sources

2. **Wait for Wing bootup:**
   - Wing may still be initializing
   - Wait 30 seconds after Wing powers on
   - Try again

3. **Check OSC responses:**
   - View → Monitoring (in REAPER)
   - Look for OSC communication errors

---

### Issue: MIDI Buttons Not Working

![Screenshot 22: REAPER monitoring debug output](images/22-reaper-monitoring-debug.png)
> **Screenshot needed:** REAPER View → Monitoring console showing MIDI debug messages when Wing buttons are pressed (e.g., "MIDI input: Note On C4 Chan 1 Vel 127").

**Symptom:** Pressing Wing buttons doesn't trigger REAPER actions

**Solutions:**

1. **Check MIDI connection:**
   - View → MIDI devices (in REAPER)
   - Wing should show activity when buttons pressed

2. **Verify MIDI messages:**
   - Open: Actions → Show action list
   - Type "midi" in filter
   - Select "View MIDI input"
   - Press Wing button
   - Should see MIDI message appear

3. **Check action mappings:**
   - Actions → Show action list
   - Find Wing action
   - Verify MIDI shortcut is listed in Shortcuts section

4. **Verify Wing MIDI settings:**
   - Wing: Setup → Remote → MIDI
   - External MIDI Control = **USB**
   - Custom buttons configured correctly

5. **Re-map actions:**
   - Remove existing MIDI mapping
   - Add new mapping
   - Press Wing button to learn
   - Test again

---

### Issue: Virtual Soundcheck No Audio

**Symptom:** Toggled to soundcheck mode but no audio through Wing

**Solutions:**

1. **Check REAPER playback:**
   - Is REAPER actually playing?
   - Are meters moving on tracks?

2. **Verify output routing:**
   - REAPER tracks must route to correct USB/CARD outputs
   - Check I/O button on each track
   - Outputs should match Wing inputs (1→1, 2→2, etc.)

3. **Check ALT indicators:**
   - Wing should show ALT ON for configured channels
   - If ALT OFF, click "Toggle Soundcheck" again

4. **Verify USB/CARD mode:**
   - Check selected mode in Wing Connector (USB vs CARD)
   - Must match your physical connection setup
   - Try switching modes if unsure

5. **Check Wing ALT routing:**
   - On Wing console, verify ALT sources are set to USB/CARD
   - May need to run Setup Soundcheck again

---

### General Debug Tips

**Enable verbose logging:**
1. View → Monitoring (in REAPER)
2. All Wing Connector activity appears here
3. Look for error messages
4. Copy log for support requests

**Check versions:**
- REAPER: 6.0 or later
- Wing firmware: Latest recommended
- macOS: 10.13 or later

**Get help:**
- Check: [README.md](../README.md)
- See: [INSTALL.md](../INSTALL.md)
- Review: [ARCHITECTURE.md](ARCHITECTURE.md)
- Open GitHub issue with log output

---

## Appendix: Screenshot Checklist

When taking screenshots for this guide, ensure:

- **High resolution** (at least 1920x1080 for full windows)
- **Clear text** (use default or larger UI fonts)
- **Annotations** where needed (arrows, labels, highlights)
- **Consistent theme** (use same REAPER theme throughout)
- **File format** PNG (lossless, good for UI)
- **File naming** matches this guide (01-extensions-menu.png, etc.)

**Screenshot specifications:**
- Full window screenshots: 1920x1080 or higher
- Close-up/detail shots: Crop to relevant area, minimum 800px width
- Annotations: Use red arrows/boxes for highlighting
- Labels: Clear, readable font (system default or larger)

---

## Document Version History

- **1.0.0** (March 2026) - Initial user guide with complete visual workflow documentation

---

**Need help?** Refer to the troubleshooting section or consult the main documentation files.
