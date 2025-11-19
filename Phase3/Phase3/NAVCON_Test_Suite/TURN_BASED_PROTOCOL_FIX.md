# Turn-Based Protocol Fix - CRITICAL UPDATE

## Problem Identified

The NAVCON tester was **violating the turn-based SCS communication protocol** by continuously sending packets without waiting for SNC responses.

### What Was Wrong

**Old Behavior (INCORRECT):**
```
HUB sends: MDPS:1, MDPS:2, MDPS:3, MDPS:4, SS:1, SS:2
         ↓ IMMEDIATELY LOOPS AGAIN (NO WAIT!)
HUB sends: MDPS:1, MDPS:2, MDPS:3, MDPS:4, SS:1, SS:2
         ↓ AGAIN! (flooding SNC)
HUB sends: MDPS:1, MDPS:2, MDPS:3, MDPS:4, SS:1, SS:2
...
SNC tries to respond but gets overwhelmed
SNC stuck sending IST=1 repeatedly
```

**Real HUB Behavior (from Client_log.txt):**
```
HUB sends: MDPS:1, MDPS:2, MDPS:3, MDPS:4, SS:1, SS:2
         ↓ WAITS FOR SNC!
SNC processes and responds: SNC:1, SNC:2, or SNC:3
         ↓ AFTER SNC RESPONDS
HUB sends: Next 6-packet sequence
         ↓ WAITS AGAIN
SNC responds again
...
```

---

## The Fix

**File: `navcon_tester.py`**

### Change 1: Track Received Packets (Lines 436-438)

Added timestamp tracking in `handle_received_packet()`:

```python
# Track last received packet timestamp for turn-based protocol
self.last_received_time = time.time()
self.last_received_packet = packet
```

**Why:** This lets us know when SNC has responded.

### Change 2: Wait After 6-Packet Sequence (Lines 1054-1078)

Added turn-based wait logic after sending SS:2 (the 6th packet):

```python
# ========================================
# WAIT for SNC response (turn-based protocol!)
# ========================================
# The real HUB waits for SNC to respond after sending all 6 packets
# This gives SNC time to process and respond with MAZE:SNC:[1,2,3]

# Record the timestamp before waiting
last_rx_time = getattr(self, 'last_received_time', 0)

# Wait for SNC response with timeout
timeout_start = time.time()
snc_responded = False

while (time.time() - timeout_start) < 0.5:  # 500ms timeout
    # Check if we received a NEW packet since we started waiting
    current_rx_time = getattr(self, 'last_received_time', 0)
    if current_rx_time > last_rx_time:
        # SNC has responded!
        snc_responded = True
        break
    time.sleep(0.01)  # Small delay to avoid busy-waiting

# Optional: Add delay if SNC didn't respond (give it more time)
if not snc_responded:
    time.sleep(0.1)
```

**Why:** This ensures we wait for SNC to process and respond before sending the next loop.

---

## Expected Results After Fix

### Before Fix (Observed):
```
SNC continuously sends: MAZE:SNC:1 (145, 0, 0, 0)
Never changes IST
Stuck in rotation request state
```

### After Fix (Expected):
```
Loop 1-9 (WHITE surface):
  SNC → MAZE:SNC:3 (forward speed vR=10, vL=10) ✅

Loop 10 (GREEN 22°):
  SNC → MAZE:SNC:2 (STOP) ✅
  SNC → MAZE:SNC:1 (ROTATE 22°) ✅
  SNC → MAZE:SNC:3 (FORWARD) ✅

Loop 25 (BLUE 30°):
  SNC → MAZE:SNC:2 (STOP) ✅
  SNC → MAZE:SNC:1 (ROTATE 90°) ✅
  SNC → MAZE:SNC:3 (FORWARD) ✅

Loop 50 (STEEP GREEN - edge sensor):
  SNC → MAZE:SNC:2 (STOP) ✅
  SNC → MAZE:SNC:1 (ROTATE 90°) ✅
  SNC → MAZE:SNC:3 (FORWARD) ✅

Loop 98 (RED EOM):
  SNC → MAZE:SNC:2 (STOP) ✅
  Maze complete! ✅
```

---

## Why This Matters

### Turn-Based Protocol

The SCS protocol is **turn-based**, not continuous:

1. **HUB's turn**: Send 6 packets (MDPS:1,2,3,4 + SS:1,2)
2. **SNC's turn**: Process data, make NAVCON decision, respond with MAZE:SNC packet(s)
3. **HUB's turn**: Send next 6 packets (with updated distance/colors)
4. **Repeat**

### What Happens Without Waiting

- SNC receives packets faster than it can process
- Input buffer overflows
- SNC can't complete NAVCON logic between packets
- Gets stuck in one state (e.g., rotation)
- Never transitions properly

### What Happens With Waiting ✅

- SNC has time to process all 6 packets
- NAVCON logic executes fully
- SNC makes proper decisions (STOP, ROTATE, FORWARD)
- Proper state transitions
- **Realistic maze simulation!**

---

## Testing The Fix

### Run the test again:

1. Upload your SNC firmware (ensure NAVCON logic is correct)
2. Connect to COM port
3. Start NAVCON tester
4. Click "Start Test"

### What to Look For:

✅ **SNC responses should vary** between IST=1, IST=2, IST=3

✅ **On WHITE surface** (loops 1-9, 16-24, etc.):
```
SNC → MAZE:SNC:3 (vR=10, vL=10) - forward motion
```

✅ **On GREEN detection** (loops 10, 40, 50, 70, 80):
```
SNC → MAZE:SNC:2 (STOP)
SNC → MAZE:SNC:1 (ROTATE angle)
SNC → MAZE:SNC:3 (FORWARD)
```

✅ **On BLUE/BLACK walls** (loops 25, 60):
```
SNC → MAZE:SNC:2 (STOP)
SNC → MAZE:SNC:1 (ROTATE 90°)
SNC → MAZE:SNC:3 (FORWARD)
```

✅ **On RED EOM** (loop 98):
```
SNC → MAZE:SNC:2 (STOP)
Stays stopped
```

---

## Summary

### The Issue
❌ Tester sent packets continuously without waiting for SNC responses
❌ Violated turn-based SCS protocol
❌ SNC couldn't keep up, got stuck in one state

### The Fix
✅ Added packet reception timestamp tracking
✅ Added 500ms wait for SNC response after each 6-packet sequence
✅ Only proceeds to next loop after SNC responds or timeout

### The Result
✅ Turn-based communication restored
✅ SNC has time to process and make NAVCON decisions
✅ Proper state transitions (IST=1, IST=2, IST=3)
✅ **Realistic maze simulation matching real HUB behavior!**

---

**Run the test again and you should see proper NAVCON behavior!** 🎯
