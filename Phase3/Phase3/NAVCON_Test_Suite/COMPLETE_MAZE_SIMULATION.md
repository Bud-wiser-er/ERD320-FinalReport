# Complete Maze Simulation - Test Coverage

## Overview

The NAVCON tester now simulates a **complete maze traversal** with all line types that the SNC will encounter during the AMazeEng challenge.

---

## Full Maze Event Timeline

| Loop | Distance | Color | Angle | Event | Expected SNC Response |
|------|----------|-------|-------|-------|----------------------|
| 1-9  | 44-60cm  | 0     | 0°    | WHITE surface (normal driving) | MAZE:SNC:3 (vR, vL) |
| **10** | **62cm** | **16** | **22°** | **🟢 GREEN line #1** | IST=2 (STOP), IST=1 (ROTATE 22°), IST=3 (RESUME) |
| 11-14 | 64-70cm | 16 | 22° | GREEN continues | Continue rotation/correction |
| **15** | **72cm** | **0** | **0°** | **⚪ GREEN cleared** | IST=3 (FORWARD) |
| 16-24 | 74-92cm | 0 | 0° | WHITE surface | MAZE:SNC:3 (vR, vL) |
| **25** | **94cm** | **24** | **30°** | **🔵 BLUE wall** | IST=2 (STOP), IST=1 (ROTATE 90°), IST=3 (RESUME) |
| 26-29 | 96-102cm | 24 | 30° | BLUE continues | Turn away from wall |
| **30** | **104cm** | **0** | **0°** | **⚪ BLUE cleared** | IST=3 (FORWARD new direction) |
| 31-39 | 106-122cm | 0 | 0° | WHITE surface | MAZE:SNC:3 (vR, vL) |
| **40** | **124cm** | **16** | **35°** | **🟢 GREEN line #2** | IST=2 (STOP), IST=1 (ROTATE 35°), IST=3 (RESUME) |
| 41-44 | 126-132cm | 16 | 35° | GREEN continues | Continue rotation/correction |
| **45** | **134cm** | **0** | **0°** | **⚪ GREEN cleared** | IST=3 (FORWARD) |
| 46-54 | 136-152cm | 0 | 0° | WHITE surface | MAZE:SNC:3 (vR, vL) |
| **55** | **154cm** | **32** | **28°** | **⚫ BLACK wall** | IST=2 (STOP), IST=1 (ROTATE 90°), IST=3 (RESUME) |
| 56-59 | 156-162cm | 32 | 28° | BLACK continues | Turn away from wall |
| **60** | **164cm** | **0** | **0°** | **⚪ BLACK cleared** | IST=3 (FORWARD new direction) |
| 61-69 | 166-182cm | 0 | 0° | WHITE surface | MAZE:SNC:3 (vR, vL) |
| **70** | **184cm** | **16** | **15°** | **🟢 GREEN line #3 (small angle)** | IST=2 (STOP), IST=1 (ROTATE 15°), IST=3 (RESUME) |
| 71-74 | 186-192cm | 16 | 15° | GREEN continues | Small correction |
| **75** | **194cm** | **0** | **0°** | **⚪ GREEN cleared** | IST=3 (FORWARD) |
| 76-84 | 196-212cm | 0 | 0° | WHITE surface | MAZE:SNC:3 (vR, vL) |
| **85** | **214cm** | **73** | **0°** | **🔴 RED end-of-maze!** | IST=2 (STOP), transition to IDLE |
| 86-100 | 216-244cm | 73 | 0° | RED continues (end state) | Remain stopped |

---

## Color Encoding Reference

| Color Value | Binary | S1 | S2 | S3 | Meaning |
|-------------|--------|----|----|----|---------|
| 0 | 0b00000000 | WHITE | WHITE | WHITE | Normal surface |
| 16 | 0b00010000 | WHITE | GREEN | WHITE | Navigable line |
| 24 | 0b00011000 | WHITE | BLUE | WHITE | Wall (turn 90°) |
| 32 | 0b00100000 | WHITE | BLACK | WHITE | Wall (turn 90°) |
| 73 | 0b01001001 | RED | RED | RED | End of maze |

**Formula**: `(S1_color << 6) | (S2_color << 3) | S3_color`

**Color codes**: 0=WHITE, 1=RED, 2=GREEN, 3=BLUE, 4=BLACK

---

## Test Scenario Breakdown

### 1. GREEN Line Navigation (3 occurrences)
Tests the SNC's ability to:
- Detect navigable crossing lines
- Calculate rotation angle from SS:2 DAT1
- Execute STOP → ROTATE → FORWARD sequence
- Handle various angles (22°, 35°, 15°)

**Expected behavior**:
```
SNC receives: MAZE:SS:1 (color=16), MAZE:SS:2 (angle=22)
SNC sends:    MAZE:SNC:2 (STOP/REVERSE)
SNC sends:    MAZE:SNC:1 (ROTATE angle=22, direction)
SNC sends:    MAZE:SNC:3 (FORWARD vR, vL)
```

### 2. BLUE Wall Avoidance (1 occurrence)
Tests the SNC's ability to:
- Detect BLUE wall markers
- Execute 90° turn away from wall
- Resume forward motion in new direction

**Expected behavior**:
```
SNC receives: MAZE:SS:1 (color=24), MAZE:SS:2 (angle=30)
SNC sends:    MAZE:SNC:2 (STOP/REVERSE)
SNC sends:    MAZE:SNC:1 (ROTATE angle=90, direction away from wall)
SNC sends:    MAZE:SNC:3 (FORWARD vR, vL)
```

### 3. BLACK Wall Avoidance (1 occurrence)
Tests the SNC's ability to:
- Detect BLACK wall markers (same as BLUE behavior)
- Execute 90° turn away from wall
- Resume forward motion in new direction

**Expected behavior**: Same as BLUE wall

### 4. RED End-of-Maze (1 occurrence)
Tests the SNC's ability to:
- Detect RED end marker on all sensors
- Stop the robot
- Potentially transition back to IDLE state

**Expected behavior**:
```
SNC receives: MAZE:SS:1 (color=73)
SNC sends:    MAZE:SNC:2 (STOP)
SNC may send: IDLE:SNC:0 (return to idle)
```

---

## Continuous Packet Flow

The tester sends **6 packets per loop** at ~500ms intervals:

```
Loop N:
  → MAZE:MDPS:1 (161, 90, 0, 0)        # Stop/rotate command
  → MAZE:MDPS:2 (162, 0, 0, 0)         # Confirm stopped
  → MAZE:MDPS:3 (163, 10, 10, 0)       # Forward motion (vR=10, vL=10)
  → MAZE:MDPS:4 (164, DAT1, dist, 0)   # Distance (increments +2 each loop)
  → MAZE:SS:1 (177, 0, color, 0)       # Color data (changes at events)
  → MAZE:SS:2 (178, angle, 0, 0)       # Angle data (changes at events)

  ← MAZE:SNC:[1,2,3] (SNC responds with navigation commands)

  [Update distance += 2]
  [Update color/angle based on loop_count]

Loop N+1:
  [Repeat with new values...]
```

---

## What This Tests

### ✅ Protocol Compliance
- Correct SCS packet structure
- Proper state transitions (IDLE→CAL→MAZE)
- Control byte encoding
- Data byte encoding

### ✅ NAVCON Logic
- **Line Detection**: Recognizing color patterns from SS:1
- **Angle Calculation**: Processing angle data from SS:2
- **Decision Making**: Choosing correct response (STOP, ROTATE, FORWARD)
- **Wall Avoidance**: 90° turns for BLUE/BLACK
- **Line Crossing**: Angle-based turns for GREEN
- **End Detection**: Stopping at RED

### ✅ Real-Time Response
- Processing continuous packet stream
- Responding within timing windows
- Handling multiple events in sequence
- Maintaining state across transitions

### ✅ Edge Cases
- Small angle correction (15° GREEN at loop 70)
- Large angle correction (35° GREEN at loop 40)
- Back-to-back events (GREEN, then BLUE, then GREEN...)
- Sustained conditions (RED continues for 15 loops)

---

## Success Criteria

A successful test should show:

1. **IDLE Phase**: SNC responds with IDLE:SNC:0
2. **CAL Phase**: SNC responds with CAL:SNC:0, then transitions to MAZE
3. **MAZE Phase - Normal driving**: SNC sends MAZE:SNC:3 with speed commands
4. **GREEN #1 (loop 10)**:
   - SNC sends MAZE:SNC:2 (STOP)
   - SNC sends MAZE:SNC:1 (ROTATE 22°)
   - SNC sends MAZE:SNC:3 (RESUME)
5. **BLUE wall (loop 25)**:
   - SNC sends MAZE:SNC:2 (STOP)
   - SNC sends MAZE:SNC:1 (ROTATE 90°)
   - SNC sends MAZE:SNC:3 (RESUME)
6. **GREEN #2 (loop 40)**: Similar to GREEN #1
7. **BLACK wall (loop 55)**: Similar to BLUE wall
8. **GREEN #3 (loop 70)**: Small angle correction
9. **RED end (loop 85)**: SNC sends MAZE:SNC:2 and stops

---

## Known Issues to Watch For

### Issue 1: SNC Only Sending IST=1
If SNC continuously sends MAZE:SNC:1 and never IST=3:
- **Cause**: SNC may be stuck in rotation state
- **Check**: `handleNavconIncomingData()` state machine
- **Fix**: Ensure rotation completion triggers IST=3 response

### Issue 2: SNC Not Responding to Colors
If SNC sends same IST=3 throughout (ignoring line detections):
- **Cause**: Color decoding or NAVCON logic not running
- **Check**: Color unpacking in `handleNavconIncomingData()`
- **Fix**: Verify bit-shift operations match encoding

### Issue 3: Wrong Rotation Angles
If SNC rotates but at wrong angles:
- **Cause**: Angle encoding/decoding mismatch
- **Check**: SS:2 DAT1 interpretation
- **Fix**: Verify angle is in DAT1 (not DAT0)

---

## Running The Test

1. Connect SNC to COM port (19200 baud)
2. Upload SNC firmware with NAVCON enabled
3. Start tester, select test scenario
4. Click "Start Test"
5. Monitor packet log for:
   - State transitions
   - Color event detections
   - SNC IST changes
   - Distance progression

---

## Expected Log Output

```
📡 PHASE 1: Establishing connection...
✅ IDLE:SNC:0 received

🛠️ PHASE 2: Calibration...
✅ CAL:SNC:0 received

🎵 PHASE 3: Waiting for MAZE transition...
✅ MAZE:SNC:3 received - NAVCON active!

🔄 PHASE 4: MAZE continuous loop
📊 Loop 0: distance=44cm, color=0
📊 Loop 10: distance=64cm, color=0
🟢 GREEN line #1 detected (22° angle)
  ← SNC: MAZE:SNC:2 (STOP)
  ← SNC: MAZE:SNC:1 (ROTATE 22)
  ← SNC: MAZE:SNC:3 (RESUME)
📊 Loop 20: distance=84cm, color=0
🔵 BLUE wall detected (30° angle) - should trigger 90° turn!
  ← SNC: MAZE:SNC:2 (STOP)
  ← SNC: MAZE:SNC:1 (ROTATE 90)
  ← SNC: MAZE:SNC:3 (RESUME)
📊 Loop 30: distance=104cm, color=0
...
🔴 RED END-OF-MAZE detected! SNC should STOP!
  ← SNC: MAZE:SNC:2 (STOP)
✅ MAZE continuous loop completed!
```

---

**Your NAVCON tester now provides COMPLETE maze coverage! 🎉**

This tests every scenario the SNC will face:
- ✅ Navigable lines (GREEN) at various angles
- ✅ Wall avoidance (BLUE and BLACK)
- ✅ End-of-maze detection (RED)
- ✅ Continuous real-time packet simulation
- ✅ Distance progression
- ✅ State machine transitions

Perfect for validating your NAVCON implementation!
