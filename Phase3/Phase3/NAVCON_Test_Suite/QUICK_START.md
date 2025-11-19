# NAVCON Tester v2.0 - Quick Start Guide

## What's New? 🎉

Your NAVCON tester now **follows the SCS protocol properly**!

Instead of sending packets once and hoping the SNC receives them, it now:
- ✅ **Continuously sends packets** until the SNC responds
- ✅ **Waits for acknowledgments** before proceeding
- ✅ **Handles packet loss** and timing issues automatically

---

## Installation

### 1. Install Python Dependencies

```bash
pip install -r requirements.txt
```

This installs:
- `pyserial` - For serial communication
- `tkinter` - GUI (usually pre-installed with Python)

---

## Running the Tester

### 1. Launch the Application

```bash
python navcon_tester.py
```

Or on Windows:
```cmd
python.exe navcon_tester.py
```

### 2. Connect to Your SNC

1. **Refresh Ports** - Click "🔄 Refresh Ports" to detect COM ports
2. **Select Port** - Choose the COM port your SNC is connected to
3. **Set Baud Rate** - Use **19200** (default for MARV)
4. **Connect** - Click "🔌 Connect"

You should see: `✅ Connected`

### 3. Select a Test Scenario

Choose from the dropdown:
- **QTP1: First GREEN Line** - Tests calibration → first GREEN detection
- **QTP2: GREEN Lines (Navigable)** - Multiple GREEN line scenarios
- **QTP3: BLUE Walls** - BLUE wall avoidance
- **QTP4: BLACK Walls** - BLACK wall avoidance + 180° turns
- **QTP5: RED End-of-Maze** - RED line end detection
- **Full Maze: All Colors** - Complete end-to-end test

### 4. Run the Test

Click **"▶️ Start Test"**

Watch the real-time packet exchange in the "📊 Packet Monitor" tab!

---

## Understanding the Output

### Phase-by-Phase Progress

```
📡 PHASE 1: Establishing connection with SNC...
🔄 Waiting for (0-1-0)
[Sending IDLE:HUB:0 repeatedly until SNC responds]
✅ SNC ready - IDLE:SNC:0 received

🛠️ PHASE 2: Calibration - Waiting for touch detection...
🔄 Waiting for (1-1-0)
[Sending CAL:SS:0 repeatedly until SNC enters CAL state]
✅ SNC entered CAL state (CAL:SNC:0)

📍 Sending first touch signal...
🔄 Waiting for (1-1-1)
[Sending CAL:SS:1 repeatedly until SNC acknowledges touch]
✅ First touch acknowledged (CAL:SNC:1)

🎵 PHASE 3: Waiting for pure tone detection...
🔄 Waiting for (2-1-[1, 2, 3])
[Sending CAL:MDPS packets until SNC transitions to MAZE]
✅ MAZE state entered - NAVCON active!

🧪 PHASE 4: NAVCON test sequence in MAZE state...
➡️ MAZE Phase 1: Sending forward movement packets...
🔄 Waiting for SNC MAZE:SNC:[3]
✅ Received expected MAZE:SNC:3
✅ SNC sending speed commands (MAZE:SNC:3)

🟢 MAZE Phase 2: Simulating GREEN line detection...
🔄 Waiting for SNC MAZE:SNC:[2]
✅ Received expected MAZE:SNC:2

... [continues through phases] ...

✅ NAVCON test sequence completed!
```

---

## Packet Monitor

The packet log shows all communication in this format:

```
HH:MM:SS.mmm || SEQ || DIRECTION || (SYS-SUB-IST) || STATE | SUBSYSTEM | IST || DAT1 | DAT0 | DEC || CONTROL
```

**Example:**
```
14:32:15.234 ||   1 || SENT     || (0-0-0) || IDLE | HUB  | 0 ||   0 |   0 |   0 ||   0
14:32:15.456 ||   1 || RECEIVED || (0-1-0) || IDLE | SNC  | 0 ||   1 |  50 |   0 ||  16
14:32:16.123 ||   2 || SENT     || (1-3-0) || CAL  | SS   | 0 ||   0 |   0 |   0 || 112
14:32:16.345 ||   2 || RECEIVED || (1-1-0) || CAL  | SNC  | 0 ||   0 |   0 |   0 ||  80
```

**Color Legend:**
- 🔵 **Blue** = SENT packets (from tester)
- 🟢 **Green** = RECEIVED packets (from SNC)
- 🔴 **Red** = ERROR messages
- ⚪ **White** = INFO messages
- 🟢 **Bright Green** = SUCCESS messages

---

## Statistics Tab

Track test performance in real-time:

| Metric | Description |
|--------|-------------|
| **Test Duration** | How long the test has been running |
| **Packets Sent** | Total packets sent by tester |
| **Packets Received** | Total packets received from SNC |
| **Success Rate** | Receive/Send ratio (should be ~50% for good two-way communication) |
| **Current State** | SNC's current system state |
| **Touch Events** | Touch detections counted |
| **Rotation Commands** | Rotation requests from SNC |
| **Green Detections** | GREEN lines detected |

---

## Troubleshooting

### "❌ Failed to establish connection with SNC"

**Problem:** SNC not responding to IDLE:HUB:0

**Solutions:**
1. Check serial cable connection
2. Verify correct COM port selected
3. Confirm baud rate is 19200
4. Check SNC is powered on and firmware is running
5. Open Arduino Serial Monitor to check SNC is sending packets

---

### "❌ SNC did not enter CAL state"

**Problem:** SNC not transitioning from IDLE to CAL

**Solutions:**
1. Verify your SNC code is processing CAL:SS:0 packets
2. Check `system_state.cpp` → `processStateTransition()`
3. Ensure SNC recognizes CAL state transition triggers
4. Review SNC Serial Monitor for state transition logs

---

### "❌ SNC did not acknowledge first touch"

**Problem:** Touch detection not working

**Solutions:**
1. Check touch detection logic in SNC
2. Verify `systemStatus.touchDetected` flag is being set
3. Review CAL state IST progression (0 → 1)
4. Simulate touch via serial command: send 'T' to SNC

---

### "❌ SNC did not transition to MAZE state"

**Problem:** Pure tone detection or CAL completion issue

**Solutions:**
1. Increase timeout from 30s to 60s
2. Verify pure tone detection in SNC code
3. Check `waitingForSecondTouch` flag logic
4. Manually trigger with 'P' command to SNC
5. Review CAL → MAZE transition conditions

---

### Test Runs But SNC Doesn't React to Lines

**Problem:** NAVCON not processing sensor data

**Solutions:**
1. Check `handleNavconIncomingData()` in `navcon_core.cpp`
2. Verify color encoding in packets (DAT0 byte)
3. Enable NAVCON debug output
4. Review `updateLineDetection()` function
5. Check that `runEnhancedNavcon()` is being called

---

## Saving Test Logs

Click **"💾 Save Log"** to export the complete packet trace.

Logs are saved as:
```
navcon_test_log_YYYYMMDD_HHMMSS.txt
```

Use these logs to:
- Debug protocol issues
- Verify SNC behavior
- Document test results
- Compare different test runs

---

## Advanced: Creating Custom Test Scenarios

You can create custom scenarios by editing `navcon_tester.py`:

```python
# Example: Custom scenario
custom_test = NAVCONTestScenario("My Custom Test",
                                "Tests specific NAVCON behavior")

# Add test steps
custom_test.add_step(
    SCSPacket(control, dat1, dat0, dec),
    "Description of this step"
)

# Add to scenarios
self.scenarios["Custom"] = custom_test
```

See existing scenarios in `create_test_scenarios()` for examples.

---

## Key Differences from v1.0

### OLD (v1.0): Fire and Forget
```python
send_packet_once()
sleep(fixed_delay)
hope_it_worked()
```

### NEW (v2.0): Loop Until Acknowledged
```python
while not timeout:
    send_packet()
    if received_expected_response():
        break
    sleep(interval)
```

**This is the SCS protocol way!** 🎯

---

## Next Steps

1. ✅ Run **QTP1** to test basic calibration → first GREEN line
2. ✅ Run **Full Maze** to test complete NAVCON sequence
3. ✅ Review packet logs to understand SCS communication flow
4. ✅ Use this as reference for other subsystem testing

---

## Support Files

- **`navcon_tester.py`** - Main application
- **`SCS_PROTOCOL_GUIDE.md`** - Complete protocol documentation
- **`CHANGES_SUMMARY.md`** - What changed from v1.0 to v2.0
- **`QUICK_START.md`** - This file
- **`README.md`** - Original test suite documentation

---

## Need Help?

Review the logs in this order:

1. **Packet Monitor tab** - See real-time packet exchange
2. **Statistics tab** - Check success rate and counts
3. **Saved log files** - Export for detailed analysis
4. **SCS_PROTOCOL_GUIDE.md** - Understand expected behavior
5. **SNC Serial Monitor** - Check what SNC is doing

---

**Happy Testing! 🚀**

Your NAVCON tester is now SCS protocol compliant and ready to thoroughly test your SNC subsystem!
