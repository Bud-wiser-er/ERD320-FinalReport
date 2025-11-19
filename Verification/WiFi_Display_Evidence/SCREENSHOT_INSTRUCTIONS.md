# WiFi Display Screenshot Instructions

## Quick Start

1. **Power on the MARV robot** with SNC and WiFi ESP32 connected
2. **Wait for WiFi network** "MARV_System" to appear (about 5 seconds)
3. **Connect your device** (phone/laptop) to "MARV_System" WiFi
4. **Open browser** and go to: http://192.168.4.1
5. **Put robot in MAZE mode** so real data is displayed
6. **Take screenshot** when data is actively updating

---

## Recommended Screenshots

### Primary Screenshot (REQUIRED)

**Filename:** `wifi_display_screenshot.png`

**Capture during:**
- MAZE state active
- Robot navigating autonomously
- Sensor data actively changing
- NAVCON making decisions

**Should show:**
- System State: MAZE
- Active sensor colors (not all WHITE)
- Real distance value
- Wheel speeds non-zero
- NAVCON decision visible
- Connection status: Connected
- Packets per second > 0

---

### Optional Additional Screenshots

**IDLE State:** `wifi_display_idle.png`
- Shows initial state
- Clean starting point
- All systems ready

**CAL State:** `wifi_display_cal.png`
- During calibration
- Shows calibration process
- State transition evidence

**Mobile View:** `wifi_display_mobile.png`
- Screenshot from phone browser
- Shows responsive design
- Mobile compatibility proof

---

## Screenshot Requirements

### Technical Specifications

**Resolution:**
- Minimum: 1920x1080 (Full HD)
- Preferred: Native screen resolution
- Format: PNG (use PNG, not JPEG)

**Window Setup:**
- Full browser window visible
- URL bar showing http://192.168.4.1
- No browser extensions/toolbars blocking content
- Full-screen mode OFF (show browser chrome)

**Capture Tool:**
- Windows: Win+Shift+S or Snipping Tool
- Mac: Cmd+Shift+4 then Space (window capture)
- Linux: Screenshot tool or Shift+PrtScn

### Content Requirements

**Data Quality:**
- Real operating data (not all zeros/defaults)
- Multiple data fields populated
- Timestamp recent
- No error messages

**Visual Quality:**
- Clear and readable text
- No blur or compression artifacts
- Proper colors (not washed out)
- No overlapping windows

---

## Example Procedure

### Step-by-Step

1. **Prepare Robot:**
   ```
   - Power on main board
   - Power on WiFi ESP32
   - Place on test track
   - Ensure batteries charged
   ```

2. **Connect WiFi:**
   ```
   - Look for "MARV_System" network
   - Connect (no password required)
   - Verify connected (check WiFi icon)
   ```

3. **Open Browser:**
   ```
   - Launch Chrome/Firefox/Safari
   - Navigate to http://192.168.4.1
   - Wait for page to load
   - Verify data appearing
   ```

4. **Activate System:**
   ```
   - Trigger touch sensor (IDLE -> CAL)
   - Wait for calibration complete
   - System enters MAZE automatically
   - Robot starts navigating
   ```

5. **Capture Screenshot:**
   ```
   - Wait for robot to encounter colored line
   - Observe sensor data updating
   - Check NAVCON decision displayed
   - Take screenshot
   ```

6. **Verify Screenshot:**
   ```
   - Check file saved as PNG
   - Open and verify clarity
   - Confirm all data visible
   - Check file size (should be 200-500KB)
   ```

---

## What Good Screenshots Look Like

### GOOD Screenshot Checklist

- [ ] URL bar visible with http://192.168.4.1
- [ ] System State shows "MAZE" (or whichever state you're capturing)
- [ ] Connection Status: "Connected"
- [ ] At least 2 sensors showing non-WHITE colors
- [ ] Distance value > 0
- [ ] Wheel speeds showing values (0-10)
- [ ] NAVCON state shows actual decision (not "UNKNOWN")
- [ ] Packets Per Second > 0
- [ ] Timestamp shows recent time
- [ ] All cards/sections visible
- [ ] Text is crisp and readable
- [ ] No browser error messages

### AVOID These Issues

- ❌ All sensor data showing zeros/defaults
- ❌ System State: "UNKNOWN" or error
- ❌ Connection Status: "Disconnected"
- ❌ Blurry or low-resolution image
- ❌ JPEG compression artifacts
- ❌ Window partially off-screen
- ❌ Dark mode or inverted colors
- ❌ Browser developer tools open (blocks view)

---

## After Capturing

### File Management

1. **Save screenshot** to this directory
2. **Rename** to match expected filename (e.g., `wifi_display_screenshot.png`)
3. **Verify size:** Should be 200KB - 2MB
4. **Add to git:** `git add wifi_display_screenshot.png`

### Documentation

Add caption to README.md describing what the screenshot shows:

```markdown
**Screenshot captured:** 2025-11-18
**System State:** MAZE
**Scenario:** Robot navigating through intersection with GREEN and BLUE lines
**Notable:** Shows NAVCON deciding to rotate 90 degrees RIGHT based on BLUE line detection
```

---

## Troubleshooting

### Cannot Connect to WiFi

**Problem:** "MARV_System" network not appearing

**Solutions:**
- Verify ESP32 powered on (LED should be lit)
- Wait 10-15 seconds after power on
- Check ESP32 SPI connections
- Restart ESP32 module

### Page Won't Load

**Problem:** http://192.168.4.1 times out

**Solutions:**
- Confirm connected to "MARV_System" network
- Try http://192.168.4.1:80 explicitly
- Clear browser cache
- Try different browser
- Check ESP32 serial monitor for errors

### No Data Updating

**Problem:** Page loads but shows all zeros

**Solutions:**
- Verify SPI connection to main board
- Check main board is powered and running
- Trigger system to enter MAZE state
- Look for "SPI communication error" messages
- Verify all three modules connected (HUB simulation for testing)

### Data Frozen

**Problem:** Data not updating in real-time

**Solutions:**
- Refresh page (F5)
- Check JavaScript console for errors
- Verify packets per second > 0
- Look at "Last Update" timestamp
- May need to restart ESP32

---

## File Placement

Once captured, place screenshots here:

```
Verification/WiFi_Display_Evidence/
├── README.md                           (This documentation)
├── SCREENSHOT_INSTRUCTIONS.md          (This file)
├── wifi_display_screenshot.png         (PRIMARY - Required)
├── wifi_display_idle.png              (Optional)
├── wifi_display_cal.png               (Optional)
├── wifi_display_maze.png              (Optional - if different from primary)
└── wifi_display_mobile.png            (Optional)
```

---

## Verification Checklist

Before committing screenshots:

- [ ] Primary screenshot captured (wifi_display_screenshot.png)
- [ ] PNG format (not JPEG)
- [ ] Resolution >= 1920x1080
- [ ] URL bar visible
- [ ] Real operating data shown
- [ ] System in MAZE state
- [ ] All data fields visible
- [ ] Clear and readable
- [ ] File size reasonable (200KB-2MB)
- [ ] Screenshot documented in README.md
- [ ] Added to git repository

---

**Ready to capture?** Follow the "Example Procedure" section above!

**Questions?** See the troubleshooting section or refer to the main WiFi Display Evidence README.md

---

**Last Updated:** 2025-01-18
**Status:** Instructions Complete - Ready for Screenshot Capture
