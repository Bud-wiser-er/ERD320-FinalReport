# NAVCON Tester - SCS Protocol Compliance Guide

## Overview

The NAVCON Tester v2.0 has been updated to fully comply with the SCS (Subsystem Communication Standard) protocol used in the AMazeEng MARV system.

## Key Changes from v1.0

### ❌ **OLD BEHAVIOR (v1.0):**
- Sent packets once and hoped the SNC received them
- Used fixed delays and timeouts
- Did not handle packet loss or timing issues
- Failed when SNC was not immediately ready

### ✅ **NEW BEHAVIOR (v2.0):**
- **Continuously sends packets** until SNC responds with expected acknowledgment
- Follows proper SCS state transition protocol
- Robust against packet loss and timing variations
- Waits for proper state transitions before proceeding

---

## SCS Protocol Implementation

### State Transition Pattern

The tester now implements the **continuous send-and-wait** pattern for all state transitions:

```python
while not timeout:
    send_packet(state_specific_packet)
    wait_for_response()

    if received_expected_response:
        transition_complete = True
        break

    sleep(send_interval)
```

---

## Test Phases

### Phase 1: IDLE → CAL Transition

**Objective:** Establish connection with SNC

**Tester Action:**
- Continuously sends `IDLE:HUB:0` (0, 0, 0, 0)

**Expected SNC Response:**
- `IDLE:SNC:0` with DAT1=1, DAT0=50

**Success Criteria:**
- SNC responds within 10 seconds

---

### Phase 2: CAL State - Initial Calibration Sequence

**Objective:** Send the calibration initialization sequence

**Tester Action (sent ONCE in sequence):**
1. `CAL:SS:0` (112, 0, 0, 0) - No touch detected
2. `CAL:MDPS:0` (96, 10, 10, 0) - MDPS calibration start
3. `CAL:MDPS:1` (97, 90, 0, 0) - MDPS calibration rotation
4. `CAL:SS:1` (113, 0, 0, 0) - First touch detected

**Expected SNC Response:**
- `CAL:SNC:0` after receiving initial packets

**Success Criteria:**
- SNC enters CAL state (may or may not send CAL:SNC:0)

---

### Phase 3: CAL → MAZE Transition (Pure Tone Detection)

**Objective:** Continuously send CAL packets until SNC detects pure tone and transitions to MAZE

**Tester Action:**
- Continuously alternates between (this is the main loop):
  1. `CAL:MDPS:1` (97, 90, 0, 0) - Calibration rotation
  2. `CAL:SS:1` (113, 0, 0, 0) - Touch detected (maintain)

**Why these two packets?**
- The HUB keeps sending MDPS:1 and SS:1 to maintain calibration state
- When SNC detects pure tone, it transitions to MAZE
- The tester must keep the loop running until this happens

**Expected SNC Response:**
- `MAZE:SNC:1` or `MAZE:SNC:2` or `MAZE:SNC:3`
  - Any MAZE state IST indicates successful transition

**Success Criteria:**
- SNC transitions to MAZE state within 30 seconds

---

### Phase 4: MAZE State - NAVCON Operations

#### Phase 4.1: Forward Movement

**Tester Action:**
- Continuously alternates:
  1. `MAZE:MDPS:3` (163, 10, 10, 0) - Forward motion
  2. `MAZE:MDPS:4` (164, 0, 50, 0) - Distance update

**Expected SNC Response:**
- `MAZE:SNC:3` - Speed command (vR, vL values)

**Success Criteria:**
- SNC sends speed commands (NAVCON active)

---

#### Phase 4.2: GREEN Line Detection

**Tester Action:**
- Continuously alternates:
  1. `MAZE:SS:1` (177, 0, 16, 0) - GREEN on S2
     - DAT0 = 16 = 0b00010000 = (S1:0 | S2:GREEN(2) | S3:0)
  2. `MAZE:SS:2` (178, 35, 0, 0) - Incidence angle 35°

**Expected SNC Response:**
- `MAZE:SNC:2` - STOP command or REVERSE command

**Success Criteria:**
- SNC reacts to line detection within 10 seconds

---

#### Phase 4.3: Reverse Completion

**Tester Action:**
- Continuously sends:
  - `MAZE:MDPS:2` (162, 0, 6, 3) - Reverse 60mm complete
    - DAT0 = 6 (60mm / 10)
    - DEC = 3 (reverse direction indicator)

**Expected SNC Response:**
- `MAZE:SNC:1` - Rotation command with angle and direction

**Success Criteria:**
- SNC sends rotation command within 10 seconds

---

#### Phase 4.4: Rotation Completion

**Tester Action:**
- Continuously sends:
  - `MAZE:MDPS:2` (162, 0, 35, 2) - Rotation 35° RIGHT complete
    - DAT0 = 35 (angle in degrees)
    - DEC = 2 (RIGHT/CW direction)

**Expected SNC Response:**
- `MAZE:SNC:3` - Forward command

**Success Criteria:**
- SNC resumes forward motion within 10 seconds

---

#### Phase 4.5: Clear Surface (WHITE)

**Tester Action:**
- Continuously alternates:
  1. `MAZE:SS:1` (177, 0, 0, 0) - All sensors WHITE
  2. `MAZE:MDPS:3` (163, 10, 10, 0) - Forward motion

**Expected SNC Response:**
- `MAZE:SNC:3` - Continues sending speed commands

**Success Criteria:**
- SNC maintains forward navigation

---

## Control Byte Encoding Reference

### Structure
```
Control Byte = SYS<1:0> | SUB<1:0> | IST<3:0>
Bits:         [7:6]       [5:4]      [3:0]
```

### System States (SYS)
- `00` = IDLE (0)
- `01` = CAL (1)
- `10` = MAZE (2)
- `11` = SOS (3)

### Subsystem IDs (SUB)
- `00` = HUB (0)
- `01` = SNC (1)
- `10` = MDPS (2)
- `11` = SS (3)

### Example Control Bytes

| Packet | Binary | Decimal | Description |
|--------|--------|---------|-------------|
| IDLE:HUB:0 | 0b00000000 | 0 | Initial contact |
| IDLE:SNC:0 | 0b00010000 | 16 | SNC ready |
| CAL:SS:0 | 0b01110000 | 112 | No touch |
| CAL:SS:1 | 0b01110001 | 113 | First touch |
| CAL:SNC:0 | 0b01010000 | 80 | CAL entered |
| CAL:SNC:1 | 0b01010001 | 81 | Touch acknowledged |
| CAL:MDPS:0 | 0b01100000 | 96 | Cal motion |
| CAL:MDPS:1 | 0b01100001 | 97 | Cal rotation |
| MAZE:SNC:1 | 0b10010001 | 145 | Rotation command |
| MAZE:SNC:2 | 0b10010010 | 146 | Stop/Reverse |
| MAZE:SNC:3 | 0b10010011 | 147 | Speed command |
| MAZE:MDPS:2 | 0b10100010 | 162 | Motion complete |
| MAZE:MDPS:3 | 0b10100011 | 163 | Forward motion |
| MAZE:MDPS:4 | 0b10100100 | 164 | Distance update |
| MAZE:SS:1 | 0b10110001 | 177 | Color detection |
| MAZE:SS:2 | 0b10110010 | 178 | Angle measurement |

---

## Color Encoding in DAT0 (MAZE:SS:1)

Colors are packed into DAT0 as: `(S1 << 6) | (S2 << 3) | S3`

### Color Values
- `0` = WHITE
- `1` = RED
- `2` = GREEN
- `3` = BLUE
- `4` = BLACK

### Examples

| Scenario | S1 | S2 | S3 | DAT0 | Binary | Decimal |
|----------|----|----|-------|------|--------|---------|
| All WHITE | 0 | 0 | 0 | `(0<<6)\|(0<<3)\|0` | 0b00000000 | 0 |
| GREEN on S2 | 0 | 2 | 0 | `(0<<6)\|(2<<3)\|0` | 0b00010000 | 16 |
| BLUE on S2 | 0 | 3 | 0 | `(0<<6)\|(3<<3)\|0` | 0b00011000 | 24 |
| BLACK on S2 | 0 | 4 | 0 | `(0<<6)\|(4<<3)\|0` | 0b00100000 | 32 |
| All RED | 1 | 1 | 1 | `(1<<6)\|(1<<3)\|1` | 0b01001001 | 73 |

---

## Timing Parameters

| Parameter | Value | Purpose |
|-----------|-------|---------|
| `send_interval` | 0.5s | Time between packet resends |
| `IDLE timeout` | 10s | Max wait for SNC connection |
| `CAL timeout` | 15s | Max wait for touch acknowledgment |
| `MAZE transition` | 30s | Max wait for pure tone → MAZE |
| `NAVCON response` | 10s | Max wait for NAVCON reactions |

---

## Using the Tester

### Basic Usage

1. **Connect to Serial Port:**
   - Select COM port from dropdown
   - Set baud rate to 19200
   - Click "Connect"

2. **Select Test Scenario:**
   - Choose from predefined scenarios
   - Or create custom test sequence

3. **Run Test:**
   - Click "Start Test"
   - Monitor packet exchange in real-time
   - Review statistics and progress

### Expected Test Flow

```
📡 PHASE 1: Establishing connection...
   🔄 Sending IDLE:HUB:0...
   ✅ Received IDLE:SNC:0

🛠️ PHASE 2: Calibration - Touch detection...
   🔄 Sending CAL:SS:0...
   ✅ Received CAL:SNC:0
   🔄 Sending CAL:SS:1...
   ✅ Received CAL:SNC:1

🎵 PHASE 3: Pure tone detection...
   🔄 Sending CAL:MDPS packets...
   ✅ Received MAZE:SNC:3 - NAVCON active!

🧪 PHASE 4: NAVCON test sequence...
   ➡️ Forward movement...
   ✅ SNC sending speed commands

   🟢 GREEN line detection...
   ✅ SNC sent STOP command

   ⏪ Reverse execution...
   ✅ SNC sent rotation command

   🔄 Rotation execution...
   ✅ SNC resumed forward motion

   ⚪ Clear surface...
   ✅ Test completed!
```

---

## Troubleshooting

### Issue: "Timeout waiting for SNC ready signal"

**Cause:** SNC is not responding to IDLE:HUB:0

**Solutions:**
- Check serial connection (correct COM port, baud rate)
- Verify SNC firmware is running
- Check SNC Serial1 RX/TX connections (pins 19/18)
- Review SNC serial monitor for errors

---

### Issue: "SNC did not enter CAL state"

**Cause:** SNC not transitioning from IDLE to CAL

**Solutions:**
- Verify touch detection logic in SNC code
- Check that SNC is processing CAL:SS:0 packets
- Review system_state.cpp state transition logic

---

### Issue: "SNC did not transition to MAZE state"

**Cause:** Pure tone detection not triggering or timing issue

**Solutions:**
- Increase timeout from 30s to 60s
- Verify pure tone detection logic in SNC
- Check that SNC `waitingForSecondTouch` flag is being set
- Review CAL → MAZE transition conditions

---

### Issue: "No response to GREEN line detection"

**Cause:** NAVCON not processing SS packets

**Solutions:**
- Verify line detection logic in navcon_core.cpp
- Check color encoding (DAT0 = 16 for GREEN on S2)
- Review handleNavconIncomingData() function
- Enable NAVCON debug output

---

## API Reference

### wait_for_transition()

Continuously send a single packet until expected response received.

```python
wait_for_transition(
    send_packet: SCSPacket,
    send_description: str,
    expected_state: SystemState,
    expected_subsystem: SubsystemID,
    expected_ist: int,
    timeout: float,
    send_interval: float = 0.5
) -> bool
```

---

### wait_for_transition_multi_packet()

Continuously alternate between multiple packets until expected response received.

```python
wait_for_transition_multi_packet(
    send_packets: List[SCSPacket],
    send_descriptions: List[str],
    expected_state: SystemState,
    expected_subsystem: SubsystemID,
    expected_ist_list: List[int],
    timeout: float,
    send_interval: float = 0.5
) -> bool
```

---

### send_and_wait_response()

Send packets and wait for SNC to respond with specific IST values.

```python
send_and_wait_response(
    send_packets: List[SCSPacket],
    send_descriptions: List[str],
    wait_for_ist_list: List[int],
    timeout: float,
    send_interval: float = 0.5
) -> bool
```

---

## Summary

The updated NAVCON Tester v2.0:

✅ **Follows SCS protocol** - Continuous send-and-wait pattern
✅ **Robust communication** - Handles packet loss and delays
✅ **Proper state transitions** - Waits for acknowledgments
✅ **Comprehensive logging** - Full packet trace for debugging
✅ **Flexible timing** - Configurable intervals and timeouts

This ensures reliable testing of your SNC subsystem's NAVCON implementation!
