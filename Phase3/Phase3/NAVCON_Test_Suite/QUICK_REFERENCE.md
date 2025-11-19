# NAVCON Tester - Quick Reference Card

## What The Tester Does Now ✅

**Emulates the HUB by continuously sending SS (sensor) and MDPS (motor) packets to test your SNC's NAVCON logic in real-time.**

## Packet Flow

### Phase 1: Connection
```
YOU → IDLE:HUB:0
SNC → IDLE:SNC:0 ✅
```

### Phase 2: Calibration Init
```
YOU → CAL:SS:0
YOU → CAL:MDPS:0
YOU → CAL:MDPS:1
YOU → CAL:SS:1
SNC → CAL:SNC:0 ✅
```

### Phase 3: Calibration Loop
```
LOOP:
  YOU → CAL:MDPS:1
  YOU → CAL:SS:1
  SNC → CAL:SNC:0
UNTIL SNC → MAZE:SNC:[1,2,3] ✅
```

### Phase 4: MAZE Continuous Loop ⭐
```
CONTINUOUS LOOP (100 iterations):
  YOU → MAZE:MDPS:1 (stop/rotate cmd)
  YOU → MAZE:MDPS:2 (confirm)
  YOU → MAZE:MDPS:3 (forward)
  YOU → MAZE:MDPS:4 (distance++) ← INCREMENTS!
  YOU → MAZE:SS:1 (colors) ← CHANGES!
  YOU → MAZE:SS:2 (angle) ← CHANGES!

  SNC → MAZE:SNC:[1,2,3] (responds to your data)

  REPEAT with updated values...
```

## Virtual Maze Events

| Loop | Distance | Color | Angle | Event |
|------|----------|-------|-------|-------|
| 1-9  | 44-60cm  | 0     | 0°    | WHITE surface |
| 10   | 62cm     | 16    | 22°   | 🟢 GREEN detected! |
| 11-14| 64-70cm  | 16    | 22°   | GREEN continues |
| 15   | 72cm     | 0     | 0°    | ⚪ Back to WHITE |
| 16-29| 74-100cm | 0     | 0°    | WHITE surface |
| 30   | 102cm    | 16    | 35°   | 🟢 Second GREEN! |
| 31-34| 104-110cm| 16    | 35°   | GREEN continues |
| 35+  | 112cm+   | 0     | 0°    | WHITE surface |

## Color Encoding (MAZE:SS:1 DAT0)

| Value | Binary | Meaning |
|-------|--------|---------|
| 0     | 0b00000000 | All WHITE |
| 2     | 0b00000010 | S3=GREEN |
| 16    | 0b00010000 | S2=GREEN ⭐ |
| 24    | 0b00011000 | S2=BLUE |
| 32    | 0b00100000 | S2=BLACK |
| 73    | 0b01001001 | All RED (end) |

Formula: `(S1_color << 6) | (S2_color << 3) | S3_color`

Colors: 0=WHITE, 1=RED, 2=GREEN, 3=BLUE, 4=BLACK

## Expected SNC Responses

### Normal Forward Motion
```
SNC → MAZE:SNC:3 (DAT1=vR, DAT0=vL, DEC=0)
```

### GREEN Line Detected
```
SNC → MAZE:SNC:2 (STOP/REVERSE)
SNC → MAZE:SNC:1 (ROTATE angle, direction)
SNC → MAZE:SNC:3 (FORWARD after correction)
```

### BLUE/BLACK Wall
```
SNC → MAZE:SNC:2 (STOP/REVERSE)
SNC → MAZE:SNC:1 (90° turn)
SNC → MAZE:SNC:3 (FORWARD new direction)
```

## Control Bytes Quick Lookup

| Packet | Control | Binary | Dec |
|--------|---------|--------|-----|
| IDLE:HUB:0 | 0b00000000 | 0x00 | 0 |
| IDLE:SNC:0 | 0b00010000 | 0x10 | 16 |
| CAL:SS:0 | 0b01110000 | 0x70 | 112 |
| CAL:SS:1 | 0b01110001 | 0x71 | 113 |
| CAL:MDPS:0 | 0b01100000 | 0x60 | 96 |
| CAL:MDPS:1 | 0b01100001 | 0x61 | 97 |
| CAL:SNC:0 | 0b01010000 | 0x50 | 80 |
| MAZE:MDPS:1 | 0b10100001 | 0xA1 | 161 |
| MAZE:MDPS:2 | 0b10100010 | 0xA2 | 162 |
| MAZE:MDPS:3 | 0b10100011 | 0xA3 | 163 |
| MAZE:MDPS:4 | 0b10100100 | 0xA4 | 164 |
| MAZE:SS:1 | 0b10110001 | 0xB1 | 177 |
| MAZE:SS:2 | 0b10110010 | 0xB2 | 178 |
| MAZE:SNC:1 | 0b10010001 | 0x91 | 145 |
| MAZE:SNC:2 | 0b10010010 | 0x92 | 146 |
| MAZE:SNC:3 | 0b10010011 | 0x93 | 147 |

Formula: `(SYS << 6) | (SUB << 4) | IST`

## Running The Test

1. Connect to COM port (19200 baud)
2. Select test scenario
3. Click "Start Test"
4. Watch the continuous packet exchange!

## What Success Looks Like

```
✅ IDLE:SNC:0 received
✅ CAL:SNC:0 received
✅ MAZE transition detected
✅ SNC sending MAZE:SNC:3 (forward commands)
🟢 GREEN line simulated at loop 10
✅ SNC sends MAZE:SNC:2 (STOP)
✅ SNC sends MAZE:SNC:1 (ROTATE)
✅ SNC sends MAZE:SNC:3 (RESUME)
⚪ GREEN cleared at loop 15
✅ Test completes 100 loops
```

## Troubleshooting

### No IDLE:SNC:0
- Check SNC is running
- Check serial connection (COM port, baud rate)
- Verify SNC Serial1 pins (RX=19, TX=18)

### Stuck in CAL
- Use 'P' command in SNC Serial Monitor to trigger pure tone
- Check pure tone detection logic in SNC

### No SNC responses in MAZE
- Verify NAVCON is running (SNC should send MAZE:SNC:3)
- Check `handleNavconIncomingData()` is processing packets
- Enable NAVCON debug output

## Key Files

- **`navcon_tester.py`** - Main application
- **`REAL_HUB_PROTOCOL.md`** - Complete protocol doc
- **`FINAL_FIX_SUMMARY.md`** - What was fixed and why
- **`QUICK_REFERENCE.md`** - This file

---

**Remember**: This tester EMULATES the real HUB's continuous packet stream. Your SNC sees realistic sensor/motor data in real-time!
