# NAVCON Tester v2.0 - Changes Summary

## What Was Fixed

Your NAVCON tester was sending packets **once** and expecting the SNC to be ready immediately. This violated the SCS protocol and failed when:

- The SNC wasn't ready to receive
- Packets were lost or delayed
- State transitions took time to process

## The Fix

The tester now follows the **SCS Protocol**: continuously sending state-specific packets until the SNC responds with the expected acknowledgment.

---

## Before & After Comparison

### ❌ OLD CODE (v1.0)

```python
# Send packet ONCE and hope it works
self.send_packet(SCSPacket(112, 0, 0, 0), "SS: Start calibration")
time.sleep(0.5)  # Fixed delay

self.send_packet(SCSPacket(113, 0, 0, 0), "SS: Calibration complete")
time.sleep(2.0)  # Another fixed delay

# Cross fingers and hope SNC received it...
maze_started = False
start_time = time.time()

while not maze_started and (time.time() - start_time) < 15:
    # Just passively wait for response
    try:
        msg_type, data = self.message_queue.get(timeout=0.1)
        # Check if we got lucky...
    except queue.Empty:
        continue
```

**Problems:**
- ❌ No retries if packet lost
- ❌ Fixed delays don't adapt to SNC timing
- ❌ No feedback loop
- ❌ Fails if SNC is busy processing

---

### ✅ NEW CODE (v2.0)

```python
# Keep sending CAL:SS:0 until SNC acknowledges
if not self.wait_for_transition(
    send_packet=SCSPacket(112, 0, 0, 0),
    send_description="SS: Calibration no touch (CAL:SS:0)",
    expected_state=SystemState.CAL,
    expected_subsystem=SubsystemID.SNC,
    expected_ist=0,
    timeout=15.0,
    send_interval=0.5  # Send every 0.5s until response
):
    self.log_message("❌ SNC did not enter CAL state", "ERROR")
    return

# Keep sending CAL:SS:1 until SNC acknowledges
if not self.wait_for_transition(
    send_packet=SCSPacket(113, 0, 0, 0),
    send_description="SS: First touch detected (CAL:SS:1)",
    expected_state=SystemState.CAL,
    expected_subsystem=SubsystemID.SNC,
    expected_ist=1,
    timeout=15.0,
    send_interval=0.5
):
    self.log_message("❌ SNC did not acknowledge first touch", "ERROR")
    return
```

**Benefits:**
- ✅ Retries automatically until response received
- ✅ Adapts to SNC processing speed
- ✅ Proper feedback loop (send → wait → check → repeat)
- ✅ Robust against packet loss
- ✅ Clear success/failure reporting

---

## New Helper Functions

### 1. `wait_for_transition()`

Sends a single packet repeatedly until SNC responds.

```python
def wait_for_transition(send_packet, expected_state, expected_subsystem,
                       expected_ist, timeout, send_interval=0.5):
    """
    Keep sending packet every send_interval seconds
    until expected response is received or timeout occurs
    """
    while not timeout:
        send_packet(packet)

        if received_expected_response():
            return True  # Success!

        sleep(send_interval)

    return False  # Timeout
```

**Use Case:** Simple state transitions with one packet type

**Example:** Waiting for SNC to acknowledge touch detection

---

### 2. `wait_for_transition_multi_packet()`

Alternates between multiple packets until SNC responds.

```python
def wait_for_transition_multi_packet(send_packets, expected_state,
                                    expected_subsystem, expected_ist_list,
                                    timeout, send_interval=0.5):
    """
    Rotate through multiple packets, sending one every send_interval,
    until expected response is received or timeout
    """
    packet_index = 0

    while not timeout:
        send_packet(send_packets[packet_index])
        packet_index = (packet_index + 1) % len(send_packets)

        if received_expected_response():
            return True

        sleep(send_interval)

    return False
```

**Use Case:** State transitions requiring multiple packet types

**Example:** CAL → MAZE transition (alternating MDPS:0 and MDPS:1)

---

### 3. `send_and_wait_response()`

Specialized for MAZE state interactions.

```python
def send_and_wait_response(send_packets, wait_for_ist_list,
                          timeout, send_interval=0.5):
    """
    Send packets continuously and wait for SNC to respond
    with any IST value in the wait_for_ist_list
    """
    while not timeout:
        send_next_packet()

        if received_maze_snc_with_ist_in_list():
            return True

        sleep(send_interval)

    return False
```

**Use Case:** NAVCON testing in MAZE state

**Example:** Sending GREEN line detection, waiting for STOP/REVERSE command

---

## Updated Test Flow

### Phase 1: IDLE → CAL

```
OLD: Send IDLE:HUB:0 once → Wait → Hope for response
NEW: Loop { Send IDLE:HUB:0 → Check response } until IDLE:SNC:0 received
```

### Phase 2: CAL Touch Detection

```
OLD: Send CAL:SS:0 once → Fixed delay → Send CAL:SS:1 once → Fixed delay
NEW: Loop { Send CAL:SS:0 } until CAL:SNC:0
     Loop { Send CAL:SS:1 } until CAL:SNC:1
```

### Phase 3: CAL → MAZE

```
OLD: Send MDPS:0, MDPS:1 once each → Wait → Hope SNC transitions
NEW: Loop {
       Send MDPS:0
       Send MDPS:1
     } until MAZE:SNC received
```

### Phase 4: MAZE Operations

```
OLD: Send SS/MDPS packets once → Fixed delays → Pray NAVCON responds
NEW: Loop { Send packets } until expected IST received from SNC
```

---

## SCS Protocol Compliance Checklist

| Requirement | v1.0 | v2.0 |
|-------------|------|------|
| Continuous packet sending | ❌ | ✅ |
| Wait for acknowledgments | ❌ | ✅ |
| Handle packet loss | ❌ | ✅ |
| Adaptive timing | ❌ | ✅ |
| Proper state transitions | ❌ | ✅ |
| Timeout protection | ⚠️ | ✅ |
| Clear error reporting | ⚠️ | ✅ |
| Protocol documentation | ❌ | ✅ |

---

## Key Takeaways

### The Problem
Your tester was **fire-and-forget** - it sent packets once and hoped the SNC received them.

### The Solution
The tester now **loops until acknowledged** - following proper SCS protocol.

### Why This Matters

1. **Reliability:** Handles real-world communication issues (noise, timing, busy SNC)
2. **Compliance:** Follows the SCS specification properly
3. **Debugging:** Clear feedback when transitions fail
4. **Robustness:** Works even if SNC response timing varies

### The Pattern

```
SEND → WAIT → CHECK → REPEAT until:
  ✅ Expected response received, OR
  ❌ Timeout occurs
```

This is how **all SCS subsystems should communicate** - continuous handshaking until acknowledged!

---

## Testing Recommendations

1. **Test with varying delays:**
   - Add delays in SNC processing to verify tester handles slow responses

2. **Test packet loss simulation:**
   - Randomly drop packets in tester to verify retry logic

3. **Test timeout scenarios:**
   - Verify tester reports failures clearly when SNC doesn't respond

4. **Test state transition edge cases:**
   - Verify all CAL → MAZE transition paths work correctly

---

## Version Information

**v1.0:** Basic tester with fixed delays and single-send packets
**v2.0:** SCS protocol compliant with continuous send-and-wait pattern ✅

**Updated Files:**
- `navcon_tester.py` - Main tester with new protocol functions
- `SCS_PROTOCOL_GUIDE.md` - Complete protocol documentation
- `CHANGES_SUMMARY.md` - This file

---

**Your tester is now SCS compliant! 🎉**
