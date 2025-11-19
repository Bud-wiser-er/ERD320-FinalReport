# ACTUAL HUB PROTOCOL (from Client_log.txt analysis)

## COMPLETE PROTOCOL FLOW

### Phase 1: IDLE Connection
```
Line 2:  HUB  Sends: (0-0-0) IDLE:HUB:0
Line 3:  SNC  Sends: (0-1-0) IDLE:SNC:0 with DAT1=1, DAT0=50
```

### Phase 2: CAL Initialization (ONE TIME)
```
Line 4:  HUB Sends: (1-3-0) CAL:SS:0    (112, 0, 0, 0)
Line 5:  HUB Sends: (1-2-0) CAL:MDPS:0  (96, 10, 10, 0)
Line 6:  HUB Sends: (1-2-1) CAL:MDPS:1  (97, 90, 0, 0)
Line 7:  HUB Sends: (1-3-1) CAL:SS:1    (113, 0, 0, 0)
Line 8:  SNC Sends: (1-1-0) CAL:SNC:0   (80, 0, 0, 0)
```

### Phase 3: CAL Loop (until MAZE transition)
```
REPEATING PATTERN (lines 9-110):
  HUB Sends: (1-2-1) CAL:MDPS:1  (97, 90, 0, 0)
  HUB Sends: (1-3-1) CAL:SS:1    (113, 0, 0, 0)
  SNC Sends: (1-1-0) CAL:SNC:0   (80, 0, 0, 0)
  ... repeats every ~0.5 seconds for ~20 seconds ...

Line 110: SNC Sends: (1-1-0) with DAT1=1 (pure tone detected!)
Line 111: SNC Sends: (2-1-1) MAZE:SNC:1
Line 112: SNC Sends: (2-1-2) MAZE:SNC:2
Line 113: SNC Sends: (2-1-3) MAZE:SNC:3 (10, 10, 0)
```

### Phase 4: MAZE Continuous Loop (CRITICAL!)

**THIS IS THE KEY**: HUB sends ALL 6 packets in a continuous loop!

```
CONTINUOUS REPEATING PATTERN (lines 114+):
  Packet 1: (2-2-1) MAZE:MDPS:1  (161, 90, 0, 0)
  Packet 2: (2-2-2) MAZE:MDPS:2  (162, 0, 0, 0)
  Packet 3: (2-2-3) MAZE:MDPS:3  (163, 10, 10, 0)
  Packet 4: (2-2-4) MAZE:MDPS:4  (164, DAT1, distance, 0)  <- distance increments!
  Packet 5: (2-3-1) MAZE:SS:1    (177, 0, color_data, 0)
  Packet 6: (2-3-2) MAZE:SS:2    (178, angle, 0, 0)

  [SNC responds with MAZE:SNC:1, MAZE:SNC:2, MAZE:SNC:3]

  REPEAT (with updated distance in MDPS:4)
```

## Key Observations

1. **MDPS:4 Distance Increments**:
   - Line 117: distance = 44 (0.44m)
   - Line 126: distance = 86 (0.86m)
   - Continues increasing each loop simulating forward movement

2. **SS Color Changes**:
   - Line 118: color = 128 (WHITE everywhere)
   - Line 127: color = 144 (different pattern)
   - Line 199: color = 2 (GREEN detected!)

3. **SS Angle Data**:
   - Line 119: angle = 0
   - Line 128: angle = 40
   - Line 209: angle = 22

4. **SNC IST Changes**:
   - Most of the time: SNC sends IST=3 (forward speed commands)
   - Line 222: SNC sends IST=3 with DEC=1 (crossing line)
   - SNC responds to SS color/angle changes

## The Pattern

```python
while in_maze:
    # Send MDPS sequence
    send(MAZE:MDPS:1)  # Stop/rotate command
    send(MAZE:MDPS:2)  # Confirm stopped
    send(MAZE:MDPS:3)  # Forward motion
    send(MAZE:MDPS:4)  # Distance (incrementing)

    # Send SS sequence
    send(MAZE:SS:1)    # Color data (changing)
    send(MAZE:SS:2)    # Angle data (changing)

    # Listen for SNC response
    # SNC sends MAZE:SNC:1, :2, :3 based on NAVCON decisions

    # Update simulation state
    distance += 2  # Increment for next loop
    # Update colors/angles based on "virtual maze position"
```

## Distance Encoding
- DAT1 = upper byte (meters)
- DAT0 = lower byte (decimeters/centimeters)
- Example: DAT1=1, DAT0=44 = 1.44 meters

## Color Encoding (DAT0 of MAZE:SS:1)
```
Bit packing: (S1_color << 6) | (S2_color << 3) | S3_color

Colors:
  0 = WHITE
  1 = RED
  2 = GREEN
  3 = BLUE
  4 = BLACK

Examples:
  0   = 0b00000000 = All WHITE
  2   = 0b00000010 = S3=GREEN, others WHITE
  16  = 0b00010000 = S2=GREEN, others WHITE
  18  = 0b00010010 = S2=GREEN, S3=GREEN
  128 = 0b10000000 = S1=GREEN, others WHITE
  144 = 0b10010000 = S1=GREEN, S2=GREEN
```

## What Your Tester Must Do

1. **Send IDLE:HUB:0**, wait for IDLE:SNC:0
2. **Send CAL init sequence** (SS:0, MDPS:0, MDPS:1, SS:1)
3. **Loop CAL:MDPS:1 + CAL:SS:1** until SNC transitions to MAZE
4. **Continuous MAZE loop**:
   - Send all 6 packets (MDPS:1,2,3,4 + SS:1,2)
   - Update distance each loop
   - Update colors/angles to simulate maze navigation
   - Listen for SNC responses in real-time
   - React to SNC commands (if needed for advanced testing)

This is a **continuous simulation** - not a "send and wait" system!
