# NAVCON Tester - CORRECTED HUB Protocol

## What Was Wrong

Your tester was stuck sending **CAL:SS:0** repeatedly. This was incorrect!

Looking at the **actual HUB Client_log.txt**, the correct sequence is:

---

## CORRECT HUB Protocol Sequence

### Phase 1: IDLE Connection
```
Send: IDLE:HUB:0 (0, 0, 0, 0)
Wait for: IDLE:SNC:0 (16, 1, 50, 0)
```

---

### Phase 2: CAL Initial Sequence (ONCE)
```
Send in sequence:
  1. CAL:SS:0    (112, 0, 0, 0)   // No touch
  2. CAL:MDPS:0  (96, 10, 10, 0)  // MDPS cal start
  3. CAL:MDPS:1  (97, 90, 0, 0)   // MDPS cal rotation
  4. CAL:SS:1    (113, 0, 0, 0)   // First touch

Wait for: CAL:SNC:0 (80, 0, 0, 0)
```

---

### Phase 3: CAL Loop (Until MAZE Transition)

**THIS IS THE KEY PART!**

```
Loop continuously:
  Send: CAL:MDPS:1 (97, 90, 0, 0)
  Send: CAL:SS:1   (113, 0, 0, 0)

  Repeat every 0.25-0.5 seconds

Until: MAZE:SNC:1 or MAZE:SNC:2 or MAZE:SNC:3 received
```

**From Client_log.txt (lines 9-110):**
```
Line 9:   || Sent: || (1-2-1) || CAL  | MDPS | 1 ||   90 |    0 |   0 ||  97 ||
Line 10:  || Sent: || (1-3-1) || CAL  | SS   | 1 ||    0 |    0 |   0 || 113 ||
Line 11:  || Recv: || (1-1-0) || CAL  | SNC  | 0 ||    0 |    0 |   0 ||  80 ||
Line 12:  || Sent: || (1-2-1) || CAL  | MDPS | 1 ||   90 |    0 |   0 ||  97 ||
Line 13:  || Sent: || (1-3-1) || CAL  | SS   | 1 ||    0 |    0 |   0 || 113 ||
... (repeats 50+ times)
Line 110: || Sent: || (1-3-1) || CAL  | SS   | 1 ||    0 |    0 |   0 || 113 ||
Line 111: || Recv: || (2-1-1) || MAZE | SNC  | 1 ||    0 |    0 |   0 || 145 ||
```

**See?** The HUB alternates **CAL:MDPS:1** and **CAL:SS:1** over and over until the SNC transitions to MAZE!

---

## What The Tester Does Now (FIXED)

```python
# Phase 1: IDLE → CAL
wait_for_transition(
    send_packet=IDLE:HUB:0,
    expected=IDLE:SNC:0
)

# Phase 2: Send CAL initial sequence (ONCE)
send(CAL:SS:0)
send(CAL:MDPS:0)
send(CAL:MDPS:1)
send(CAL:SS:1)

# Phase 3: Loop until MAZE transition
wait_for_transition_multi_packet(
    send_packets=[CAL:MDPS:1, CAL:SS:1],  # ← CORRECTED!
    expected=MAZE:SNC:[1,2,3]
)
```

---

## Common Mistakes (AVOID THESE!)

### ❌ WRONG Approach 1: Send CAL:SS:0 repeatedly
```python
# This is what was happening before:
while not maze:
    send(CAL:SS:0)  # ← WRONG! Only send this ONCE in init!
```

### ❌ WRONG Approach 2: Send all four packets repeatedly
```python
# Also wrong:
while not maze:
    send(CAL:SS:0)
    send(CAL:MDPS:0)
    send(CAL:MDPS:1)
    send(CAL:SS:1)
# ← Wrong! Only MDPS:1 and SS:1 should loop!
```

### ✅ CORRECT Approach:
```python
# Initial sequence ONCE:
send(CAL:SS:0)
send(CAL:MDPS:0)
send(CAL:MDPS:1)
send(CAL:SS:1)

# Then loop only these two:
while not maze:
    send(CAL:MDPS:1)  # ← Correct!
    send(CAL:SS:1)    # ← Correct!
```

---

## Why This Matters

The SCS protocol expects the tester (HUB) to:

1. **Initialize CAL state** with the 4-packet sequence
2. **Maintain CAL state** by continuously sending MDPS:1 and SS:1
3. **Wait for SNC** to detect pure tone and transition to MAZE

The SNC is busy doing calibration work. It needs the tester to keep the CAL state "alive" by sending packets. Once the SNC detects the pure tone, **it** initiates the MAZE transition, not the tester!

---

## Packet Control Bytes Reference

| Packet | Control Byte | Binary | Calculation |
|--------|--------------|--------|-------------|
| IDLE:HUB:0 | 0 | 0b00000000 | (0<<6)\|(0<<4)\|0 |
| IDLE:SNC:0 | 16 | 0b00010000 | (0<<6)\|(1<<4)\|0 |
| CAL:SS:0 | 112 | 0b01110000 | (1<<6)\|(3<<4)\|0 |
| CAL:MDPS:0 | 96 | 0b01100000 | (1<<6)\|(2<<4)\|0 |
| CAL:MDPS:1 | 97 | 0b01100001 | (1<<6)\|(2<<4)\|1 |
| CAL:SS:1 | 113 | 0b01110001 | (1<<6)\|(3<<4)\|1 |
| CAL:SNC:0 | 80 | 0b01010000 | (1<<6)\|(1<<4)\|0 |
| MAZE:SNC:1 | 145 | 0b10010001 | (2<<6)\|(1<<4)\|1 |
| MAZE:SNC:2 | 146 | 0b10010010 | (2<<6)\|(1<<4)\|2 |
| MAZE:SNC:3 | 147 | 0b10010011 | (2<<6)\|(1<<4)\|3 |

---

## Testing The Fix

Run your tester now and you should see:

```
📡 PHASE 1: Establishing connection with SNC...
🔄 Waiting for (0-1-0)
✅ SNC ready - IDLE:SNC:0 received

🛠️ PHASE 2: Calibration sequence...
Sent: CAL:SS:0
Sent: CAL:MDPS:0
Sent: CAL:MDPS:1
Sent: CAL:SS:1
⏳ Waiting for SNC to enter CAL state...
✅ SNC entered CAL state (CAL:SNC:0)

🎵 PHASE 3: Waiting for MAZE transition (pure tone)...
🔄 Waiting for (2-1-[1, 2, 3])
Sent: CAL:MDPS:1
Sent: CAL:SS:1
Sent: CAL:MDPS:1
Sent: CAL:SS:1
... (loop continues until MAZE transition)
✅ Received MAZE:SNC:3
🎯 MAZE state entered - NAVCON active!
```

---

## Summary

**The Fix:**
- ✅ Send CAL:SS:0, CAL:MDPS:0, CAL:MDPS:1, CAL:SS:1 **once** during initialization
- ✅ Then **loop** sending only CAL:MDPS:1 and CAL:SS:1 until MAZE transition
- ❌ Don't keep sending CAL:SS:0 repeatedly (that was the bug!)

**Your tester now matches the HUB protocol exactly!** 🎉
