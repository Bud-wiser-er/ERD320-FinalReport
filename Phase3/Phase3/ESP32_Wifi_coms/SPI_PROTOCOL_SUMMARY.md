# SPI Communication Protocol Summary
**MARV Main ESP32 ↔ WiFi ESP32 Communication**

---

## Overview

The SPI protocol enables the **Main ESP32** (MARV brain) to send diagnostic and telemetry data to the **WiFi ESP32** (network interface) over a serial connection using SPI hardware.

**Key Concept:** SPI hardware sends packets over GPIO pins, which are then forwarded to a remote PC via WiFi.

---

## Hardware Configuration

### Main ESP32 (Master/Sender)
```
GPIO 15 (CS)   → Chip Select (active LOW)
GPIO 18 (SCK)  → SPI Clock
GPIO 23 (MOSI) → Master Out Slave In (data line)
GPIO 19 (MISO) → Master In Slave Out (unused)
```

### WiFi ESP32 (Slave/Receiver)
```
Receives SPI packets on matching pins
Forwards packets to PC via WiFi/Serial
```

### SPI Settings
- **Speed:** 2 MHz
- **Mode:** SPI_MODE0 (CPOL=0, CPHA=0)
- **Bit Order:** MSB First
- **Frame Format:** Custom packet-based protocol

---

## Packet Structure

### Complete Packet Layout (257 bytes total)

```
┌─────────────────────────────────────────────────────────────┐
│                    SPI PACKET (257 bytes)                   │
├─────────────────────────────────────────────────────────────┤
│  HEADER (8 bytes)                                           │
│  ┌──────────────────────────────────────────────────────┐  │
│  │ sync1 (0xAA)           - 1 byte                      │  │
│  │ sync2 (0x55)           - 1 byte                      │  │
│  │ packet_type            - 1 byte (identifies payload) │  │
│  │ data_length            - 1 byte (payload size)       │  │
│  │ sequence               - 2 bytes (packet counter)    │  │
│  │ flags                  - 1 byte (reserved)           │  │
│  │ checksum_header        - 1 byte (XOR of above)       │  │
│  └──────────────────────────────────────────────────────┘  │
├─────────────────────────────────────────────────────────────┤
│  PAYLOAD (248 bytes max)                                    │
│  ┌──────────────────────────────────────────────────────┐  │
│  │ Varies by packet_type (see Packet Types below)      │  │
│  │ Padded with zeros if < 248 bytes                    │  │
│  └──────────────────────────────────────────────────────┘  │
├─────────────────────────────────────────────────────────────┤
│  checksum_payload (1 byte) - XOR of payload bytes           │
└─────────────────────────────────────────────────────────────┘
```

---

## How Both Sides Know What's Being Sent

### Step 1: Packet Type Identification

The **packet_type** field (byte 3 in header) identifies what data structure is in the payload:

```cpp
enum PacketType : uint8_t {
    PKT_SYSTEM_STATE      = 0x01,  // System state update
    PKT_TOUCH_DETECTED    = 0x02,  // Touch sensor event
    PKT_PURE_TONE         = 0x03,  // Pure tone detected
    PKT_SENSOR_COLORS     = 0x10,  // SS: Color readings
    PKT_INCIDENCE_ANGLE   = 0x11,  // SS: Angle measurement
    PKT_END_OF_MAZE       = 0x12,  // SS: EOM detected
    PKT_WHEEL_SPEEDS      = 0x20,  // MDPS: Current speeds
    PKT_DISTANCE          = 0x21,  // MDPS: Distance traveled
    PKT_ROTATION_ANGLE    = 0x22,  // MDPS: Rotation feedback
    PKT_LINE_DETECTION    = 0x30,  // NAVCON: Line detected
    PKT_NAVCON_STATE      = 0x31,  // NAVCON: State change
    PKT_ROTATION_COMMAND  = 0x32,  // NAVCON: Rotation planned
    PKT_ROTATION_FEEDBACK = 0x33,  // NAVCON: Rotation result
    PKT_ANGLE_EVALUATION  = 0x34,  // NAVCON: Angle decision
    PKT_DEBUG_MESSAGE     = 0x40,  // Debug text
    PKT_HEARTBEAT         = 0x42   // Keep-alive
};
```

### Step 2: Sender Builds Packet

**Main ESP32 (Sender) Example:**
```cpp
// Want to send sensor colors: S1=GREEN, S2=WHITE, S3=WHITE

1. Call: spiComm.sendSensorColors(COLOR_GREEN, COLOR_WHITE, COLOR_WHITE);

2. buildHeader(PKT_SENSOR_COLORS, sizeof(SensorColorsPayload));
   → Writes header:
     sync1 = 0xAA
     sync2 = 0x55
     packet_type = 0x10 (PKT_SENSOR_COLORS)
     data_length = 8 (size of SensorColorsPayload)
     sequence = 1234 (auto-incremented)
     flags = 0
     checksum_header = XOR(0xAA, 0x55, 0x10, 0x08, ...) = 0xE7

3. Fill payload with SensorColorsPayload struct:
   payload[0-3]  = timestamp (4 bytes, millis())
   payload[4]    = sensor1_color = 2 (GREEN)
   payload[5]    = sensor2_color = 0 (WHITE)
   payload[6]    = sensor3_color = 0 (WHITE)
   payload[7]    = reserved = 0
   payload[8-247] = 0 (padding)

4. Calculate payload checksum:
   checksum_payload = XOR(all 248 payload bytes)

5. Send via SPI:
   digitalWrite(CS, LOW);
   SPI.transfer(&packet, 257 bytes);
   digitalWrite(CS, HIGH);
```

### Step 3: Receiver Decodes Packet

**WiFi ESP32 (Receiver) Example:**
```cpp
1. Receive 257 bytes via SPI interrupt

2. Validate sync bytes:
   if (packet.header.sync1 != 0xAA || packet.header.sync2 != 0x55) {
       reject_packet();
   }

3. Verify header checksum:
   calculated = XOR(header bytes 0-6)
   if (calculated != packet.header.checksum_header) {
       reject_packet();
   }

4. Verify payload checksum:
   calculated = XOR(payload bytes 0-247)
   if (calculated != packet.checksum_payload) {
       reject_packet();
   }

5. Identify packet type:
   switch (packet.header.packet_type) {
       case PKT_SENSOR_COLORS:
           // Cast payload to correct struct
           SensorColorsPayload* data = (SensorColorsPayload*)packet.payload;

           // Extract data
           uint32_t timestamp = data->timestamp;
           uint8_t s1 = data->sensor1_color;  // = 2 (GREEN)
           uint8_t s2 = data->sensor2_color;  // = 0 (WHITE)
           uint8_t s3 = data->sensor3_color;  // = 0 (WHITE)

           // Forward to PC via Serial/WiFi
           Serial.printf("COLORS: S1=%d S2=%d S3=%d @ %lu\n", s1, s2, s3, timestamp);
           break;
   }
```

---

## Key Packet Types and Payloads

### 1. PKT_SENSOR_COLORS (0x10) - 8 bytes
```cpp
struct SensorColorsPayload {
    uint32_t timestamp;      // When colors were read
    uint8_t sensor1_color;   // S1: 0=WHITE, 1=RED, 2=GREEN, 3=BLUE, 4=BLACK
    uint8_t sensor2_color;   // S2: (same encoding)
    uint8_t sensor3_color;   // S3: (same encoding)
    uint8_t reserved;
};
```

**Use:** Sends current color sensor readings from SS subsystem.

---

### 2. PKT_INCIDENCE_ANGLE (0x11) - 12 bytes
```cpp
struct IncidenceAnglePayload {
    uint32_t timestamp;      // When angle was calculated
    uint16_t angle;          // Incidence angle in degrees (0-90)
    uint8_t first_sensor;    // Which sensor detected first (1, 2, or 3)
    uint8_t second_sensor;   // Which sensor confirmed (usually 2)
    uint8_t sensors_used;    // Bitmask: bit 0=S1, bit 1=S2, bit 2=S3
    uint8_t reserved[3];
};
```

**Use:** Sends angle measurement from SS when line is detected at an angle.

---

### 3. PKT_WHEEL_SPEEDS (0x20) - 8 bytes
```cpp
struct WheelSpeedsPayload {
    uint32_t timestamp;      // When speeds were read
    uint8_t vR;              // Right wheel speed (mm/s)
    uint8_t vL;              // Left wheel speed (mm/s)
    uint8_t vop_setpoint;    // Target speed (VOP)
    uint8_t reserved;
};
```

**Use:** Sends current motor speeds from MDPS.

---

### 4. PKT_DISTANCE (0x21) - 8 bytes
```cpp
struct DistancePayload {
    uint32_t timestamp;      // When distance was measured
    uint16_t distance_mm;    // Distance in millimeters
    uint8_t reserved[2];
};
```

**Use:** Sends distance traveled since last stop/reset.

---

### 5. PKT_ROTATION_ANGLE (0x22) - 8 bytes
```cpp
struct RotationAnglePayload {
    uint32_t timestamp;      // When rotation completed
    uint16_t angle;          // Rotation angle in degrees
    uint8_t direction;       // 2=LEFT, 3=RIGHT
    uint8_t reserved;
};
```

**Use:** MDPS feedback confirming rotation completed.

---

### 6. PKT_LINE_DETECTION (0x30) - 12 bytes
```cpp
struct LineDetectionPayload {
    uint32_t timestamp;      // When line was detected
    uint8_t color;           // Line color (0-4)
    uint8_t first_sensor;    // Which sensor detected (1/2/3)
    uint16_t angle;          // Incidence angle
    uint8_t line_type;       // 0=NONE, 1=RED_GREEN, 2=BLACK_BLUE
    uint8_t reserved[3];
};
```

**Use:** NAVCON announces line detection with classification.

---

### 7. PKT_NAVCON_STATE (0x31) - 40 bytes
```cpp
struct NavconStatePayload {
    uint32_t timestamp;      // When state changed
    uint8_t old_state;       // Previous NAVCON state (0-6)
    uint8_t new_state;       // New NAVCON state (0-6)
    uint16_t reason_code;    // Why state changed
    char reason_text[32];    // Human-readable reason
};
```

**NAVCON States:**
```
0 = NAVCON_FORWARD_SCAN       - Driving forward, scanning for lines
1 = NAVCON_STOP               - Stopping motors
2 = NAVCON_REVERSE            - Reversing away from line
3 = NAVCON_STOP_BEFORE_ROTATE - Stopping before rotation
4 = NAVCON_ROTATE             - Executing rotation
5 = NAVCON_EVALUATE_CORRECTION - Checking if rotation was sufficient
6 = NAVCON_CROSSING_LINE      - Crossing a RED/GREEN line
```

**Use:** Tracks NAVCON state machine transitions for debugging.

---

### 8. PKT_ROTATION_COMMAND (0x32) - 12 bytes
```cpp
struct RotationCommandPayload {
    uint32_t timestamp;      // When command issued
    uint16_t target_angle;   // Rotation to execute (degrees)
    uint8_t direction;       // 2=LEFT, 3=RIGHT
    uint8_t command_reason;  // Why rotating (e.g., line correction)
    uint16_t original_angle; // Original line angle detected
    uint16_t corrections_done; // Number of corrections so far
};
```

**Use:** NAVCON planning/commanding a rotation.

---

### 9. PKT_DEBUG_MESSAGE (0x40) - 120 bytes
```cpp
struct DebugMessagePayload {
    uint32_t timestamp;      // When message generated
    uint8_t severity;        // 0=INFO, 1=WARN, 2=ERROR
    char message[115];       // Debug text (null-terminated)
};
```

**Use:** Send arbitrary debug messages to PC.

---

## Processing Flow

### Main ESP32 (Sender)
```
┌─────────────────────────────────────────────────────────┐
│ 1. Event occurs (e.g., line detected)                  │
│                                                          │
│ 2. Call appropriate send function:                      │
│    spiComm.sendLineDetection(color, sensor, angle, type)│
│                                                          │
│ 3. buildHeader() fills packet header:                   │
│    - packet_type identifies payload structure           │
│    - data_length = sizeof(payload struct)               │
│    - sequence counter increments                        │
│    - checksum calculated                                │
│                                                          │
│ 4. Fill payload with struct data:                       │
│    - Cast tx_packet.payload to correct struct pointer   │
│    - Populate all fields                                │
│                                                          │
│ 5. sendPacket() transmits:                              │
│    - Calculate payload checksum                         │
│    - CS LOW → SPI transfer 257 bytes → CS HIGH          │
│                                                          │
│ 6. Packet sent! WiFi ESP32 receives and forwards to PC │
└─────────────────────────────────────────────────────────┘
```

### WiFi ESP32 (Receiver)
```
┌─────────────────────────────────────────────────────────┐
│ 1. SPI interrupt: 257 bytes received                    │
│                                                          │
│ 2. Validate packet:                                     │
│    - Check sync bytes (0xAA, 0x55)                      │
│    - Verify header checksum                             │
│    - Verify payload checksum                            │
│                                                          │
│ 3. Identify packet type from header.packet_type         │
│                                                          │
│ 4. Cast payload to correct struct:                      │
│    switch (packet_type) {                               │
│        case PKT_SENSOR_COLORS:                          │
│            SensorColorsPayload* p = (...)payload;       │
│            process_colors(p->sensor1, p->sensor2, ...); │
│            break;                                        │
│        case PKT_LINE_DETECTION:                         │
│            LineDetectionPayload* p = (...)payload;      │
│            process_line(p->color, p->angle, ...);       │
│            break;                                        │
│    }                                                     │
│                                                          │
│ 5. Forward data to PC via WiFi/Serial:                  │
│    - Format as JSON/CSV/binary                          │
│    - Send over TCP/UDP/Serial connection                │
└─────────────────────────────────────────────────────────┘
```

---

## Example: Complete Transaction

### Scenario: NAVCON detects GREEN line at 22° angle

**Step 1: Main ESP32 sends line detection**
```cpp
// In NAVCON code:
spiComm.sendLineDetection(COLOR_GREEN, 2, 22, LINE_RED_GREEN);
```

**Step 2: Packet construction**
```
Header (8 bytes):
  [0] sync1 = 0xAA
  [1] sync2 = 0x55
  [2] packet_type = 0x30 (PKT_LINE_DETECTION)
  [3] data_length = 12
  [4-5] sequence = 0x04D2 (1234)
  [6] flags = 0x00
  [7] checksum_header = 0xE1 (XOR of above)

Payload (248 bytes):
  [0-3]   timestamp = 0x0001E240 (123456 ms)
  [4]     color = 2 (GREEN)
  [5]     first_sensor = 2 (S2)
  [6-7]   angle = 0x0016 (22°)
  [8]     line_type = 1 (LINE_RED_GREEN)
  [9-11]  reserved = 0x00
  [12-247] padding = 0x00

Checksum (1 byte):
  [248] checksum_payload = 0xA3 (XOR of payload)

Total: 257 bytes transmitted via SPI
```

**Step 3: WiFi ESP32 receives and decodes**
```cpp
// Receiver validates and parses:
LineDetectionPayload* data = (LineDetectionPayload*)packet.payload;

Serial.printf("LINE DETECTED @ %lu ms:\n", data->timestamp);
Serial.printf("  Color: %d (GREEN)\n", data->color);
Serial.printf("  Sensor: %d (S2)\n", data->first_sensor);
Serial.printf("  Angle: %d degrees\n", data->angle);
Serial.printf("  Type: %d (RED_GREEN navigable)\n", data->line_type);
```

**Step 4: Forwarded to PC**
```json
{
  "packet_type": "LINE_DETECTION",
  "timestamp": 123456,
  "color": "GREEN",
  "sensor": "S2",
  "angle": 22,
  "line_type": "RED_GREEN"
}
```

---

## Key Features

### 1. **Self-Describing Packets**
Each packet contains:
- Sync bytes for frame detection (0xAA 0x55)
- Packet type to identify payload structure
- Data length for validation
- Checksums to detect transmission errors

### 2. **Fixed-Size Transmission**
- All packets are exactly 257 bytes
- Simplifies SPI transfer logic
- Receiver knows exactly how much data to expect
- Payload padded with zeros if < 248 bytes

### 3. **Type Safety**
- Sender and receiver both have matching struct definitions
- Casting `packet.payload` to correct struct type ensures correct field alignment
- `__attribute__((packed))` prevents compiler padding mismatches

### 4. **Sequence Counter**
- Each packet has incrementing sequence number
- Receiver can detect missed packets
- Useful for debugging transmission issues

### 5. **Timestamp Synchronization**
- Every payload includes `millis()` timestamp
- PC can reconstruct event timeline
- Helps correlate events across subsystems

---

## Performance

- **SPI Speed:** 2 MHz
- **Packet Size:** 257 bytes
- **Transmission Time:** ~1.03 ms per packet
- **Overhead:** 9 bytes (header + checksum) = 3.5%
- **Max Throughput:** ~970 packets/second (theoretical)
- **Actual Rate:** ~100-200 packets/second (limited by event frequency)

---

## Summary

**The protocol works by:**
1. **Main ESP32** experiences an event (line detected, speed change, etc.)
2. Calls `spiComm.sendXXX()` with event data
3. `buildHeader()` sets `packet_type` field to identify payload structure
4. Payload filled with matching struct data
5. Checksums calculated for error detection
6. 257 bytes sent via SPI (CS LOW → transfer → CS HIGH)
7. **WiFi ESP32** receives packet, validates checksums
8. Reads `packet_type` to know which struct to cast payload to
9. Extracts data from struct fields
10. Forwards formatted data to PC via WiFi/Serial

**Both sides know what's being sent because:**
- `packet_type` byte explicitly identifies payload format
- Both Main and WiFi ESP32 have identical struct definitions
- Fixed packet structure ensures predictable data layout
- Checksums validate data integrity

This creates a robust, self-describing protocol that can transmit complex diagnostic data from MARV to a remote PC for real-time monitoring and debugging!
