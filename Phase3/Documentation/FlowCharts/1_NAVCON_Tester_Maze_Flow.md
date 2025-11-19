# NAVCON Tester - Full Maze Run Flowchart

```mermaid
flowchart TD
    Start([Start Test]) --> Init[Initialize Tester<br/>- Setup serial connection<br/>- Create log file<br/>- Reset counters]

    Init --> IDLE[IDLE State Loop<br/>Loop until SNC responds]

    IDLE --> IDLE_Send[Send IDLE Packets:<br/>IDLE:HUB:0]
    IDLE_Send --> IDLE_Wait{SNC Response?}
    IDLE_Wait -->|No| IDLE_Send
    IDLE_Wait -->|Yes IDLE:SNC:0| CAL_Init[Transition to CAL State]

    CAL_Init --> CAL[CAL State Loop<br/>Send calibration packets<br/>Loop until MAZE state]

    CAL --> CAL_Send[Send 4 CAL Packets:<br/>1. CAL:HUB:0<br/>2. MDPS:1 battery=0<br/>3. MDPS:3 speeds=0,0<br/>4. MDPS:4 distance=0]

    CAL_Send --> CAL_Wait[Wait 500ms for SNC response]

    CAL_Wait --> CAL_Check{SNC in MAZE?}
    CAL_Check -->|No| CAL_Send
    CAL_Check -->|Yes MAZE:SNC:*| MAZE_Init[Transition to MAZE State<br/>Initialize maze variables:<br/>- distance=44cm<br/>- loop_count=1<br/>- current_color=0 WHITE<br/>- current_angle=0]

    MAZE_Init --> MAZE_Loop{Loop < 100?}

    MAZE_Loop -->|No| End_Test[End Test<br/>Save log<br/>Generate report]

    MAZE_Loop -->|Yes| MAZE_Step[MAZE Loop Iteration]

    MAZE_Step --> Send_MDPS[Send 4 MDPS Packets:<br/>1. MDPS:1 battery=0,0<br/>2. MDPS:2 rotation=0,0<br/>3. MDPS:3 speeds=10,10<br/>4. MDPS:4 distance=XXcm]

    Send_MDPS --> Send_SS[Send 2 SS Packets:<br/>1. SS:1 color=current_color<br/>2. SS:2 angle=current_angle]

    Send_SS --> Wait_SNC[Wait 500ms for SNC Response]

    Wait_SNC --> Update_State[Update Virtual Maze State<br/>Increment distance +2cm<br/>Increment loop_count +1]

    Update_State --> Check_Events{Check Loop Count<br/>for Event Triggers}

    Check_Events -->|Loop 10| Event_G1[GREEN Line #1<br/>S2=GREEN 16<br/>Angle=22°<br/>Moderate angle]
    Check_Events -->|Loop 15| Event_Clear1[Clear GREEN<br/>Back to WHITE]
    Check_Events -->|Loop 25| Event_B1[BLUE Wall<br/>S2=BLUE 24<br/>Angle=30°<br/>Should trigger 90° turn]
    Check_Events -->|Loop 30| Event_Clear2[Clear BLUE]
    Check_Events -->|Loop 40| Event_G2[GREEN Line #2<br/>S2=GREEN 16<br/>Angle=35°<br/>Moderate-high angle]
    Check_Events -->|Loop 45| Event_Clear3[Clear GREEN]
    Check_Events -->|Loop 50| Event_G3[GREEN Line #3 STEEP<br/>S1=GREEN 2 EDGE SENSOR<br/>Angle=0 NO DATA<br/>>45° steep angle]
    Check_Events -->|Loop 55| Event_Clear4[Clear STEEP GREEN]
    Check_Events -->|Loop 60| Event_BL1[BLACK Wall<br/>S2=BLACK 32<br/>Angle=28°]
    Check_Events -->|Loop 65| Event_Clear5[Clear BLACK]
    Check_Events -->|Loop 70| Event_G4[GREEN Line #4<br/>S2=GREEN 16<br/>Angle=8°<br/>Small angle]
    Check_Events -->|Loop 75| Event_Clear6[Clear GREEN]
    Check_Events -->|Loop 80| Event_G5[GREEN Line #5 VERY STEEP<br/>S1=GREEN 2 EDGE<br/>Angle=0 NO DATA<br/>>45° very steep]
    Check_Events -->|Loop 85| Event_Clear7[Clear STEEP GREEN]
    Check_Events -->|Loop 90-98| Event_EOM[EOM Rectification<br/>Loop 90: angle=12°<br/>Loop 92: angle=7°<br/>Loop 94: angle=3°<br/>Loop 96: angle=1°<br/>Loop 98: RED+angle=1°<br/>Approaching EOM]
    Check_Events -->|Loop 99| Event_RED[RED EOM<br/>S1=RED, S2=RED, S3=RED<br/>All sensors = 73<br/>Angle=0°<br/>Perfect alignment]

    Event_G1 --> MAZE_Loop
    Event_Clear1 --> MAZE_Loop
    Event_B1 --> MAZE_Loop
    Event_Clear2 --> MAZE_Loop
    Event_G2 --> MAZE_Loop
    Event_Clear3 --> MAZE_Loop
    Event_G3 --> MAZE_Loop
    Event_Clear4 --> MAZE_Loop
    Event_BL1 --> MAZE_Loop
    Event_Clear5 --> MAZE_Loop
    Event_G4 --> MAZE_Loop
    Event_Clear6 --> MAZE_Loop
    Event_G5 --> MAZE_Loop
    Event_Clear7 --> MAZE_Loop
    Event_EOM --> MAZE_Loop
    Event_RED --> MAZE_Loop
    Check_Events -->|Other loops| MAZE_Loop

    End_Test --> Final([Test Complete<br/>Log saved to out.txt])

    style Start fill:#90EE90
    style Final fill:#90EE90
    style IDLE fill:#FFE4B5
    style CAL fill:#ADD8E6
    style MAZE_Step fill:#FFB6C1
    style Event_G1 fill:#98FB98
    style Event_G2 fill:#98FB98
    style Event_G3 fill:#98FB98
    style Event_G4 fill:#98FB98
    style Event_G5 fill:#98FB98
    style Event_B1 fill:#87CEEB
    style Event_BL1 fill:#696969,color:#FFF
    style Event_RED fill:#FFB6C1
    style Event_EOM fill:#FFD700
```

---

## Test Scenario Breakdown

### IDLE Phase (Until SNC Responds)
**Purpose:** Establish initial connection with SNC
- Send `IDLE:HUB:0` packets repeatedly
- Wait for `IDLE:SNC:0` response
- Validates SNC is ready to start

### CAL Phase (Until SNC enters MAZE)
**Purpose:** Calibration and initialization
- **Packets sent each loop:**
  1. `CAL:HUB:0` - State announcement
  2. `MDPS:1` - Battery level (0, 0)
  3. `MDPS:3` - Wheel speeds (0, 0)
  4. `MDPS:4` - Distance (0)
- Wait for SNC to respond with `MAZE:SNC:*`

### MAZE Phase (100 loops)
**Purpose:** Full maze simulation with all line types and scenarios

#### Standard Loop (Every iteration)
**6 Packets sent:**
1. `MDPS:1` - Battery (0, 0)
2. `MDPS:2` - Rotation (0, 0)
3. `MDPS:3` - Speeds (10, 10 mm/s)
4. `MDPS:4` - Distance (incrementing +2cm each loop)
5. `SS:1` - Color sensors (current_color)
6. `SS:2` - Incidence angle (current_angle)

**Turn-based Protocol:**
- After sending all 6 packets, wait 500ms for SNC response
- Gives SNC time to process and respond

#### Event Timeline (Loop-by-loop)

| Loop | Event | Color Encoding | Angle | Description |
|------|-------|---------------|-------|-------------|
| 1-9 | WHITE surface | 0 | 0° | Normal forward driving |
| **10** | **GREEN #1** | 16 (S2=GREEN) | 22° | Moderate angle navigable line |
| 15 | Clear | 0 | 0° | Back to WHITE |
| **25** | **BLUE wall** | 24 (S2=BLUE) | 30° | Wall avoidance - should trigger 90° turn |
| 30 | Clear | 0 | 0° | Back to WHITE |
| **40** | **GREEN #2** | 16 (S2=GREEN) | 35° | Moderate-high angle |
| 45 | Clear | 0 | 0° | Back to WHITE |
| **50** | **GREEN #3 STEEP** | 2 (S1=GREEN EDGE) | 0° | >45° steep - edge sensor triggered, NO angle data |
| 55 | Clear | 0 | 0° | Back to WHITE |
| **60** | **BLACK wall** | 32 (S2=BLACK) | 28° | Wall avoidance |
| 65 | Clear | 0 | 0° | Back to WHITE |
| **70** | **GREEN #4** | 16 (S2=GREEN) | 8° | Small angle - easy crossing |
| 75 | Clear | 0 | 0° | Back to WHITE |
| **80** | **GREEN #5 VERY STEEP** | 2 (S1=GREEN EDGE) | 0° | Very steep angle, edge sensor |
| 85 | Clear | 0 | 0° | Back to WHITE |
| **90-98** | **EOM Rectification** | 16 (S2=GREEN) | 12°→7°→3°→1° | Gradual alignment toward EOM |
| **99** | **RED EOM** | 73 (ALL RED) | 0° | Perfect alignment, all sensors RED |

---

## Test Coverage Summary

✅ **Normal Angles (≤45°):** Loops 10, 25, 40, 60, 70, 90-98
- Tests standard angle detection and correction
- Validates rotation planning and execution

✅ **Steep Angles (>45°):** Loops 50, 80
- Tests edge sensor protocol (S1/S3 detect first)
- Validates distance-based angle inference
- Confirms angle=0 protocol when no data available

✅ **Line Types:**
- GREEN navigable lines: Loops 10, 40, 50, 70, 80, 90-98
- BLUE wall: Loop 25
- BLACK wall: Loop 60
- RED EOM: Loop 99

✅ **EOM Approach:** Loops 90-99
- Tests gradual rectification (12° → 0°)
- Validates crossing threshold (≤5°)
- Confirms EOM detection when aligned

✅ **Turn-based Communication:**
- 500ms wait after each 6-packet sequence
- Gives SNC time to process and respond
- Prevents packet flooding

---

## Total Test Metrics

- **Total loops:** 100
- **Total packets sent:** ~609 (6 packets/loop × 100 loops + IDLE/CAL)
- **Test duration:** ~50-60 seconds
- **Distance simulated:** 44cm → 244cm (200cm total travel)
- **Protocol compliance:** 100% SCS v2.0 compliant
