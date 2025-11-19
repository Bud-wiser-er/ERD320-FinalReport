// MARV A-Maze-Eng - Enhanced SS (Sensor Subsystem) Phase 0
// Integrated with SNC State Machine for seamless communication
// Focus on SCS compliance and proper state transitions
// FIXED: Incidence angle sending issue resolved

#include <HardwareSerial.h>
#include <math.h>

// ==================== PIN DEFINITIONS ====================
// UART pins for subsystem communication
#define RX_PIN 16            // UART2 RX from SNC
#define TX_PIN 17            // UART2 TX to SNC
#define BUTTON_PIN 0       // GPIO 15 for triggering SEND
//#define END_OF_MAZE_PIN 2    // GPIO 2 for end-of-maze detection
#define SERIAL_BAUD 460800
#define PI 3.14159265358979323846

// ==================== PACKET STRUCTURE ====================
struct SCSPacket {
    uint8_t control;    // CONTROL<31:24>: SYS<1:0> | SUB<1:0> | IST<3:0>
    uint8_t dat1;       // DAT1<23:16>: Upper data byte
    uint8_t dat0;       // DAT0<15:8>: Lower data byte  
    uint8_t dec;        // DEC<7:0>: Decimal/general purpose byte
    
    SCSPacket() : control(0), dat1(0), dat0(0), dec(0) {}
    SCSPacket(uint8_t ctrl, uint8_t d1, uint8_t d0, uint8_t d) 
        : control(ctrl), dat1(d1), dat0(d0), dec(d) {}
};

// ==================== STATE DEFINITIONS ====================
enum SystemState {
    SYS_IDLE = 0,   // 00
    SYS_CAL = 1,    // 01
    SYS_MAZE = 2,   // 10
    SYS_SOS = 3     // 11
};

enum SubsystemID {
    SUB_HUB = 0,    // 00
    SUB_SNC = 1,    // 01
    SUB_MDPS = 2,   // 10
    SUB_SS = 3      // 11
};

int sample_time = 2;

int red_led = 14;
int green_led = 12;
int blue_led = 13;

int sensor_1 = 0;
int sensor_2 = 1;
int sensor_3 = 2;

int red = 0;
int green = 1;
int blue = 2;

int red_threshold[] = {0, 0, 0};

int green_threshold[] = {0, 0, 0};

int blue_threshold[] = {0, 0, 0};

int color_seen[] = {0, 0, 0};

int value = 0;

int has_calibrated = 0;

uint8_t save_dat_1 = 0;

uint8_t save_dat_0 = 0;

uint16_t first_distance = 0;

uint16_t second_distance =0;

uint16_t distance_for_angle_calc = 0;

float distance_between_sensors = 61;

bool toggle_middle_sensor = false;

// ==================== CONTINUOUS SAMPLING CACHE ====================
// Cache for first non-white color detection - NEVER OVERWRITE once set!
uint8_t cached_colors[3] = {0, 0, 0};      // Initialize to WHITE (0,0,0)
bool cached_sensor_set[3] = {false, false, false}; // Track which sensors have been cached
bool has_cached_edge = false;              // Flag indicating edge sensor (s1 or s3) has cached data
bool has_cached_middle = false;            // Flag indicating middle sensor (s2) has cached data
uint16_t cached_edge_distance = 0;         // Distance when edge sensor FIRST detected color
uint16_t cached_middle_distance = 0;       // Distance when middle sensor FIRST detected color
unsigned long last_sample_time = 0;        // For sample rate limiting (milliseconds)
uint16_t currentDistance = 0;              // Current distance from MDPS packets

// ==================== PACKET PARSING FUNCTIONS ====================
SystemState getSystemState(uint8_t control) {
    return (SystemState)((control >> 6) & 0x03);
}

SubsystemID getSubsystemID(uint8_t control) {
    return (SubsystemID)((control >> 4) & 0x03);
}

uint8_t getInternalState(uint8_t control) {
    return control & 0x0F;
}

uint8_t createControlByte(SystemState sys, SubsystemID sub, uint8_t ist) {
    return ((sys & 0x03) << 6) | ((sub & 0x03) << 4) | (ist & 0x0F);
}

// ==================== DEBUG FUNCTIONS ====================
const char* systemStateToString(SystemState state) {
    switch (state) {
        case SYS_IDLE: return "IDLE";
        case SYS_CAL: return "CAL";
        case SYS_MAZE: return "MAZE";
        case SYS_SOS: return "SOS";
        default: return "UNKNOWN";
    }
}

const char* subsystemToString(SubsystemID sub) {
    switch (sub) {
        case SUB_HUB: return "HUB";
        case SUB_SNC: return "SNC";
        case SUB_MDPS: return "MDPS";
        case SUB_SS: return "SS";
        default: return "UNKNOWN";
    }
}

void printPacket(const SCSPacket& packet, const char* direction = "") {
    SystemState sys = getSystemState(packet.control);
    SubsystemID sub = getSubsystemID(packet.control);
    uint8_t ist = getInternalState(packet.control);
    
    Serial.printf("%s [%s:%s:IST%d] Control:0x%02X DAT1:%d DAT0:%d DEC:%d\n",
                  direction,
                  systemStateToString(sys),
                  subsystemToString(sub),
                  ist,
                  packet.control,
                  packet.dat1,
                  packet.dat0,
                  packet.dec);
}

// ==================== SERIAL PACKET HANDLER ====================
#define PACKET_SIZE 4
#define BUFFER_SIZE 16

class SerialPacketHandler {
private:
    HardwareSerial* serial;
    uint8_t buffer[BUFFER_SIZE];
    int bufferIndex;
    unsigned long lastByteTime;
    bool synced;
    int rxPin, txPin;
    
public:
    SerialPacketHandler(HardwareSerial* ser, int rx, int tx) 
        : serial(ser), bufferIndex(0), lastByteTime(0), synced(false), rxPin(rx), txPin(tx) {}
    
    void begin(unsigned long baud) {
        serial->begin(baud, SERIAL_8N1, rxPin, txPin);
        bufferIndex = 0;
        synced = false;
    }
    
    bool readPacket(SCSPacket& packet) {
        while (serial->available()) {
            uint8_t incomingByte = serial->read();
            unsigned long currentTime = millis();
            
            if (currentTime - lastByteTime > 3 && bufferIndex > 0) {  // 3ms timeout for optimal recovery
                bufferIndex = 0;
                synced = false;
            }
            
            lastByteTime = currentTime;
            buffer[bufferIndex] = incomingByte;
            bufferIndex++;
            
            if (bufferIndex >= PACKET_SIZE) {
                for (int start = 0; start <= bufferIndex - PACKET_SIZE; start++) {
                    SCSPacket testPacket;
                    testPacket.control = buffer[start];
                    testPacket.dat1 = buffer[start + 1];
                    testPacket.dat0 = buffer[start + 2];
                    testPacket.dec = buffer[start + 3];
                    
                    // Basic validation
                    SystemState sys = getSystemState(testPacket.control);
                    SubsystemID sub = getSubsystemID(testPacket.control);
                    uint8_t ist = getInternalState(testPacket.control);
                    
                    if (sys <= SYS_SOS && sub <= SUB_SS && ist <= 15) {
                        packet = testPacket;
                        int remainingBytes = bufferIndex - (start + PACKET_SIZE);
                        if (remainingBytes > 0) {
                            memmove(buffer, buffer + start + PACKET_SIZE, remainingBytes);
                        }
                        bufferIndex = remainingBytes;
                        synced = true;
                        return true;
                    }
                }
                
                if (bufferIndex >= BUFFER_SIZE) {
                    memmove(buffer, buffer + 1, BUFFER_SIZE - 1);
                    bufferIndex = BUFFER_SIZE - 1;
                }
            }
        }
        
        return false;
    }
    
    void sendPacket(const SCSPacket& packet) {
        serial->write(packet.control);
        serial->write(packet.dat1);
        serial->write(packet.dat0);
        serial->write(packet.dec);
        serial->flush();
    }
    
    bool isSynced() const { return synced; }
    int getBufferLevel() const { return bufferIndex; }
};

// ==================== GLOBAL INSTANCES ====================
SerialPacketHandler sncHandler(&Serial2, RX_PIN, TX_PIN);

// ==================== SYSTEM STATUS TRACKING ====================
struct SSStatus {
    SystemState currentSystemState;
    uint8_t lastControlByte;
    bool ssExpectedToSend;
    bool buttonPressed;
    unsigned long lastPacketTime;
    unsigned long lastTransmissionTime;
    int packetCount;
    int transmissionCount;
    
    // Simulation data for colors and angles
    uint8_t sensorColors[3];  // 3 sensors: [0]=sensor1, [1]=sensor2, [2]=sensor3
    uint8_t lastIncidenceAngle;
    bool calibrationComplete;
    bool endOfMazeDetected;
    bool endOfMazePinActive;  // Raw GPIO 2 state
    
    // Sequential sending state - FIXED
    enum SendingState {
        SEND_NONE,
        SEND_WAITING_FOR_COLORS,
        SEND_WAITING_FOR_INCIDENCE
    };
    SendingState sendingState;
    
    // Button debounce - ADDED
    unsigned long lastButtonPress;
    bool buttonProcessed;
};

SSStatus ssStatus = {
    SYS_IDLE,        // currentSystemState
    0x00,            // lastControlByte
    false,           // ssExpectedToSend
    false,           // buttonPressed
    0,               // lastPacketTime
    0,               // lastTransmissionTime
    0,               // packetCount
    0,               // transmissionCount
    {0, 0, 0},       // sensorColors (White = 000)
    0,               // lastIncidenceAngle
    false,           // calibrationComplete
    false,           // endOfMazeDetected
    false,           // endOfMazePinActive
    SSStatus::SEND_NONE, // sendingState
    0,               // lastButtonPress
    false            // buttonProcessed
};

// ==================== COLOR ENCODING ====================
// Color codes: W=000, R=001, G=010, B=011, K=100
uint8_t encodeColours1(uint8_t s1, uint8_t s2, uint8_t s3) {
    return ((s1 & 0x03) << 6) | ((s2 & 0x07) << 3) | (s3 & 0x07);
}
uint8_t encodeColours2(uint8_t s1) {
    return (s1 & 0x04) >> 2;
}

void decodeColours(uint8_t encoded, uint8_t* s1, uint8_t* s2, uint8_t* s3) {
    *s1 = (encoded >> 6) & 0x07;
    *s2 = (encoded >> 3) & 0x07;
    *s3 = encoded & 0x07;
}

const char* colorToString(uint8_t color) {
    switch (color) {
        case 0: return "W"; // White
        case 1: return "R"; // Red
        case 2: return "G"; // Green
        case 3: return "B"; // Blue
        case 4: return "K"; // Black
        default: return "?";
    }
}

// ==================== CACHE MANAGEMENT FUNCTIONS ====================
// Check if a color is non-white (navigation-critical color)
bool shouldUpdateCache(uint8_t color) {
    return (color == 1 || color == 2 || color == 3 || color == 4); // R, G, B, or K
}

// Initialize/reset the color cache
void initializeCache() {
    cached_colors[0] = 0; // WHITE
    cached_colors[1] = 0; // WHITE
    cached_colors[2] = 0; // WHITE
    cached_sensor_set[0] = false;
    cached_sensor_set[1] = false;
    cached_sensor_set[2] = false;
    has_cached_edge = false;
    has_cached_middle = false;
    cached_edge_distance = 0;
    cached_middle_distance = 0;
    // Also reset distance tracking variables for next detection
    first_distance = 0;
    second_distance = 0;
    toggle_middle_sensor = false;
    // Note: Don't reset ssStatus.lastIncidenceAngle here - it should persist until sent
}

// Update cache with first non-white color detection - NEVER OVERWRITE!
void updateCache(uint8_t s1, uint8_t s2, uint8_t s3, uint16_t distance) {
    bool updated = false;

    // Cache sensor 1 (edge) if not already cached and detecting non-white
    if (!cached_sensor_set[0] && shouldUpdateCache(s1)) {
        cached_colors[0] = s1;
        cached_sensor_set[0] = true;
        if (!has_cached_edge) {
            cached_edge_distance = distance;
            has_cached_edge = true;
        }
        updated = true;
    }

    // Cache sensor 2 (middle) if not already cached and detecting non-white
    if (!cached_sensor_set[1] && shouldUpdateCache(s2)) {
        cached_colors[1] = s2;
        cached_sensor_set[1] = true;
        if (!has_cached_middle) {
            cached_middle_distance = distance;
            has_cached_middle = true;
        }
        updated = true;
    }

    // Cache sensor 3 (edge) if not already cached and detecting non-white
    if (!cached_sensor_set[2] && shouldUpdateCache(s3)) {
        cached_colors[2] = s3;
        cached_sensor_set[2] = true;
        if (!has_cached_edge) {
            cached_edge_distance = distance;
            has_cached_edge = true;
        }
        updated = true;
    }
    // NO DEBUG OUTPUT - too spammy
}

// ==================== SS EXPECTED TO RESPOND LOGIC ====================
bool isSSExpectedToRespond(uint8_t controlByte) {
    SystemState sys = getSystemState(controlByte);
    SubsystemID sub = getSubsystemID(controlByte);
    uint8_t ist = getInternalState(controlByte);

    // Serial.printf("🔍 Checking if SS should respond to: [%s:%s:IST%d] Control:0x%02X\n",
    //               systemStateToString(sys), subsystemToString(sub), ist, controlByte);  // Disabled for performance

    // Based on SCS state diagram, SS responds when:
    // 1. After receiving SNC touch in IDLE (which causes transition to CAL) - SS sends End of Calibration
    if (sys == SYS_IDLE && sub == SUB_SNC && ist == 0) {
        // Serial.println("✅ SS should send: End of Calibration (after SNC touch)");  // Disabled for performance
        return true;
    }
    // 2. CAL: After MDPS battery level (IST1) - SS sends Colors (CAL)
    else if (sys == SYS_CAL && sub == SUB_MDPS && ist == 1) {
        // Serial.println("✅ SS should send: Colors (CAL) (after MDPS battery level)");  // Disabled for performance
        return true;
    }
    // 3. MAZE: After MDPS distance (IST4) - SS sends Colors (MAZE) or End-of-Maze
    else if (sys == SYS_MAZE && sub == SUB_MDPS && ist == 4) {
        // Serial.println("✅ SS should send: Colors (MAZE) or End-of-Maze (after MDPS distance)");  // Disabled for performance
        return true;
    }
    // Note: SS does NOT respond to its own packets - incidence angle is sent on next web button press

    // Log what we're NOT responding to for clarity
    if (sys == SYS_CAL && sub == SUB_MDPS && ist == 0) {
        // Serial.println("ℹ️  MDPS vop calibration - SS does NOT respond, waiting for MDPS battery level");  // Disabled for performance
    }
    else if (sys == SYS_MAZE && sub == SUB_MDPS && ist == 1) {
        // Serial.println("ℹ️  MDPS battery level (MAZE) - SS does NOT respond");  // Disabled for performance
    }
    else if (sys == SYS_MAZE && sub == SUB_MDPS && ist == 2) {
        // Serial.println("ℹ️  MDPS rotation - SS does NOT respond");  // Disabled for performance
    }
    else if (sys == SYS_MAZE && sub == SUB_MDPS && ist == 3) {
        // Serial.println("ℹ️  MDPS speed - SS does NOT respond");  // Disabled for performance
    }
    else if (sys == SYS_MAZE && sub == SUB_SS && ist == 2) {
        // Serial.println("ℹ️  SS just sent Incidence Angle - SS does NOT respond to its own packet");  // Disabled for performance
    }
    else {
        // Serial.println("❌ SS should NOT respond to this packet");  // Disabled for performance
    }
    
    return false;
}

// ==================== SS PACKET GENERATION ====================
SCSPacket generateSSPacket() {
    SCSPacket packet;
    SystemState currentSys = ssStatus.currentSystemState;
    SystemState packetSys = getSystemState(ssStatus.lastControlByte);
    SubsystemID lastSub = getSubsystemID(ssStatus.lastControlByte);
    uint8_t lastIST = getInternalState(ssStatus.lastControlByte);
    
    // Serial.printf("🎯 Generating SS packet:\n");  // Disabled for performance
    // Serial.printf("    Current SS state: %s\n", systemStateToString(currentSys));
    // Serial.printf("    Last packet: [%s:%s:IST%d]\n",
    //               systemStateToString(packetSys), subsystemToString(lastSub), lastIST);
    // Serial.printf("    Sending state: %d\n", ssStatus.sendingState);
    
    // Handle initial transition to CAL - SS sends End of Calibration
    if (packetSys == SYS_IDLE && lastSub == SUB_SNC && lastIST == 0 && currentSys == SYS_CAL) {
        packet.control = createControlByte(SYS_CAL, SUB_SS, 0);
        packet.dat1 = 0;
        packet.dat0 = 0;
        packet.dec = 0;
        ssStatus.calibrationComplete = true;
        ssStatus.sendingState = SSStatus::SEND_NONE;
        // Serial.println("📤 Sending: End of Calibration (initial CAL entry)");  // Disabled for performance
        return packet;
    }
    
    // FIXED: Check sending state FIRST before checking last packet
    if (ssStatus.sendingState == SSStatus::SEND_WAITING_FOR_INCIDENCE) {
        // Next button press after Colors - Send Incidence Angle
        packet.control = createControlByte(SYS_MAZE, SUB_SS, 2);
        packet.dat1 = ssStatus.lastIncidenceAngle;
        packet.dat0 = 0;
        packet.dec = 0;
        ssStatus.sendingState = SSStatus::SEND_NONE; // Clear state after sending

        // ====== DEBUG OUTPUT FOR ANGLE TRANSMISSION (ONLY NON-ZERO) ======
        if (ssStatus.lastIncidenceAngle > 0) {
            Serial.println("╔════════════════════════════════════════════════════════════╗");
            Serial.println("║         📤 TRANSMITTING NON-ZERO ANGLE! 📤               ║");
            Serial.println("╚════════════════════════════════════════════════════════════╝");
            Serial.printf("📡 PACKET DETAILS:\n");
            Serial.printf("   ├─ Control Byte: 0x%02X [MAZE:SS:IST2]\n", packet.control);
            Serial.printf("   ├─ DAT1 (angle): %d degrees (0x%02X)\n", ssStatus.lastIncidenceAngle, packet.dat1);
            Serial.printf("   ├─ DAT0:         0x%02X\n", packet.dat0);
            Serial.printf("   └─ DEC:          0x%02X\n", packet.dec);
            Serial.printf("🔢 STORED LATCH VALUES:\n");
            Serial.printf("   ├─ ssStatus.lastIncidenceAngle: %d degrees\n", ssStatus.lastIncidenceAngle);
            Serial.printf("   ├─ first_distance:              %d mm\n", first_distance);
            Serial.printf("   ├─ second_distance:             %d mm\n", second_distance);
            Serial.printf("   └─ distance_for_angle_calc:     %d mm\n", distance_for_angle_calc);
            Serial.println("════════════════════════════════════════════════════════════");
        }

        return packet;
    }
    
    // Handle responses based on current system state and last received packet
    if (currentSys == SYS_CAL) {
        if (lastSub == SUB_MDPS && lastIST == 1) {
            // After MDPS battery level - Send Colors (CAL)
            packet.control = createControlByte(SYS_CAL, SUB_SS, 1);
            packet.dat1 = encodeColours2(ssStatus.sensorColors[0]);
            packet.dat0 = encodeColours1(ssStatus.sensorColors[0], ssStatus.sensorColors[1], ssStatus.sensorColors[2]); // Red, White, Blue
            packet.dec = 0;
            ssStatus.sendingState = SSStatus::SEND_NONE;
            // Serial.println("📤 Generating: Colors (CAL) - Red, White, Blue");  // Disabled for performance
        }
    }
    else if (currentSys == SYS_MAZE) {
        if (lastSub == SUB_MDPS && lastIST == 4) {
            // After MDPS distance - Check GPIO 2 to determine what to send
            // Serial.println("🔍 MDPS distance received - checking end-of-maze GPIO 2 state");  // Disabled for performance

            if (ssStatus.endOfMazePinActive) {
                // GPIO 2 is active - send End-of-Maze
                ssStatus.endOfMazeDetected = true;
                packet.control = createControlByte(SYS_MAZE, SUB_SS, 3);
                packet.dat1 = 0;
                packet.dat0 = 0;
                packet.dec = 0;
                ssStatus.sendingState = SSStatus::SEND_NONE;
                // Serial.println("📤 Generating: End-of-Maze (GPIO 2 is ACTIVE)");  // Disabled for performance
            } else {
                // GPIO 2 is not active - send Colors (MAZE)
                ssStatus.endOfMazeDetected = false;
                packet.control = createControlByte(SYS_MAZE, SUB_SS, 1);
                packet.dat1 = encodeColours2(ssStatus.sensorColors[0]);
                packet.dat0 = encodeColours1(ssStatus.sensorColors[0], ssStatus.sensorColors[1], ssStatus.sensorColors[2]);
                packet.dec = 0;
                ssStatus.sendingState = SSStatus::SEND_WAITING_FOR_INCIDENCE; // Set state for next send

                // ====== SHOW LATCHED ANGLE FOR NEXT SEND (ONLY IF NON-ZERO) ======
                if (ssStatus.lastIncidenceAngle > 0) {
                    Serial.println("╔════════════════════════════════════════════════════════════╗");
                    Serial.printf("║   🔔 LATCHED ANGLE FOR NEXT SEND: %d degrees 🔔          ║\n", ssStatus.lastIncidenceAngle);
                    Serial.println("╚════════════════════════════════════════════════════════════╝");
                }

                // Clear cache after successful color packet generation
                initializeCache();
            }
        }
    }
    
    return packet;
}

// ==================== GPIO MONITORING ====================
// void checkEndOfMazePin() {
//     static bool lastPinState = HIGH;
//     bool currentPinState = digitalRead(END_OF_MAZE_PIN);
    
//     // Update the raw pin state
//     ssStatus.endOfMazePinActive = (currentPinState == LOW); // Active low
    
//     // Log pin state changes for debugging
//     if (lastPinState != currentPinState) {
//         Serial.printf("🟠 🔍 End-of-Maze PIN GPIO 2: %s\n", 
//                      ssStatus.endOfMazePinActive ? "ACTIVE (LOW)" : "INACTIVE (HIGH)");
//     }
    
//     lastPinState = currentPinState;
// }

// FIXED: Better button handling with proper debounce
// void checkButton() {
//     static bool lastButtonState = HIGH;
//     bool currentButtonState = digitalRead(BUTTON_PIN);
//     unsigned long currentTime = millis();
    
//     // Button pressed (falling edge) with debounce
//     if (lastButtonState == HIGH && currentButtonState == LOW) {
//         if (currentTime - ssStatus.lastButtonPress > 200) { // 200ms debounce
//             ssStatus.buttonPressed = true;
//             ssStatus.buttonProcessed = false;
//             ssStatus.lastButtonPress = currentTime;
//             Serial.println("🟠 🔘 BUTTON PRESSED! SS PACKET READY TO TRANSMIT! 🔘");
//         }
//     }
    
//     lastButtonState = currentButtonState;
// }

// ==================== SIMULATION FUNCTIONS ====================
void updateSimulationData() {
    // Simulate different sensor readings based on system state and time
    unsigned long currentTime = millis();
    uint8_t timePattern = (currentTime / 5000) % 4; // Change every 5 seconds
    
    switch (ssStatus.currentSystemState) {
        case SYS_CAL:
            // In calibration, show standard colors
            ssStatus.sensorColors[0] = 1; // Red
            ssStatus.sensorColors[1] = 0; // White  
            ssStatus.sensorColors[2] = 3; // Blue
            ssStatus.lastIncidenceAngle = 0;
            break;
            
        case SYS_MAZE:
            // In maze, simulate various line detection scenarios
            switch (timePattern) {
                case 0: // Green line detected
                    ssStatus.sensorColors[0] = 0; // White
                    ssStatus.sensorColors[1] = 2; // Green
                    ssStatus.sensorColors[2] = 0; // White
                    ssStatus.lastIncidenceAngle = 15;
                    break;
                case 1: // Black line at angle
                    ssStatus.sensorColors[0] = 4; // Black
                    ssStatus.sensorColors[1] = 4; // Black
                    ssStatus.sensorColors[2] = 0; // White
                    ssStatus.lastIncidenceAngle = 25;
                    break;
                case 2: // Blue line
                    ssStatus.sensorColors[0] = 0; // White
                    ssStatus.sensorColors[1] = 3; // Blue
                    ssStatus.sensorColors[2] = 0; // White
                    ssStatus.lastIncidenceAngle = 5;
                    break;
                case 3: // Red line (end of maze)
                    ssStatus.sensorColors[0] = 1; // Red
                    ssStatus.sensorColors[1] = 1; // Red
                    ssStatus.sensorColors[2] = 1; // Red
                    ssStatus.lastIncidenceAngle = 0;
                    ssStatus.endOfMazeDetected = true;
                    break;
            }
            break;
            
        default:
            // IDLE or SOS - all white
            ssStatus.sensorColors[0] = 0; // White
            ssStatus.sensorColors[1] = 0; // White
            ssStatus.sensorColors[2] = 0; // White
            ssStatus.lastIncidenceAngle = 0;
            break;
    }
}

// ==================== STATUS DISPLAY ====================
void printSSStatus() {
    // Disabled for performance
    // Serial.println("\n🟠 ============================================");
    // Serial.println("🟠           SS SUBSYSTEM STATUS");
    // Serial.println("🟠 ============================================");

    // Serial.printf("🟠 Current System State: %s\n", systemStateToString(ssStatus.currentSystemState));
    // Serial.printf("🟠 Last Control Byte: 0x%02X\n", ssStatus.lastControlByte);
    // Serial.printf("🟠 SS Expected to Send: %s\n", ssStatus.ssExpectedToSend ? "YES ⚡" : "NO");
    // Serial.printf("🟠 Button Ready: %s\n", ssStatus.buttonPressed ? "YES ⚡" : "NO");
    // Serial.printf("🟠 Sending State: %d\n", ssStatus.sendingState);
    // Serial.printf("🟠 Packets Received: %d\n", ssStatus.packetCount);
    // Serial.printf("🟠 Packets Transmitted: %d\n", ssStatus.transmissionCount);
    // Serial.printf("🟠 Calibration Complete: %s\n", ssStatus.calibrationComplete ? "YES ✅" : "NO");
    // Serial.printf("🟠 End-of-Maze PIN (GPIO 2): %s\n", ssStatus.endOfMazePinActive ? "ACTIVE ⚡" : "INACTIVE");
    // Serial.printf("🟠 End of Maze Detected: %s\n", ssStatus.endOfMazeDetected ? "YES ✅" : "NO");

    // if (ssStatus.lastTransmissionTime > 0) {
    //     unsigned long timeSinceTransmission = (millis() - ssStatus.lastTransmissionTime) / 1000;
    //     Serial.printf("🟠 Last Transmission: %lu seconds ago\n", timeSinceTransmission);
    // } else {
    //     Serial.println("🟠 Last Transmission: NEVER");
    // }

    // Serial.println("🟠 *** SENSOR READINGS ***");
    // Serial.printf("🟠 Sensor 1: %s, Sensor 2: %s, Sensor 3: %s\n",
    //               colorToString(ssStatus.sensorColors[0]),
    //               colorToString(ssStatus.sensorColors[1]),
    //               colorToString(ssStatus.sensorColors[2]));
    // Serial.printf("🟠 Last Incidence Angle: %d degrees\n", ssStatus.lastIncidenceAngle);

    // Serial.println("🟠 ============================================\n");
}

// ==================== ARDUINO SETUP AND LOOP ====================
void setup() {
    pinMode(12, OUTPUT);
    pinMode(13, OUTPUT);
    pinMode(14, OUTPUT);
    pinMode(23, OUTPUT);
    pinMode(22, OUTPUT);
    pinMode(21, OUTPUT);
    pinMode(19, OUTPUT);
    pinMode(18, OUTPUT);
    pinMode(5, OUTPUT);
    pinMode(25, OUTPUT);
    pinMode(32, OUTPUT);
    pinMode(33, OUTPUT);
    pinMode(2, OUTPUT);
    digitalWrite(2, LOW);
    digitalWrite(25, LOW);
    digitalWrite(32, LOW);
    digitalWrite(33, LOW);
    digitalWrite(23, LOW);
    digitalWrite(22, LOW);
    digitalWrite(21, LOW);
    digitalWrite(19, LOW);
    digitalWrite(18, LOW);
    digitalWrite(5, LOW);
    Serial.begin(460800);
    // delay(1000);  // Removed for faster startup

    // Serial.println("🟠 ========================================");  // Disabled for performance
    // Serial.println("🟠 MARV SS Subsystem - ENHANCED PHASE 0");
    // Serial.println("🟠 Integrated with SNC State Machine");
    // Serial.println("🟠 FIXED: Incidence angle sending issue");
    // Serial.println("🟠 ========================================");

    pinMode(BUTTON_PIN, INPUT_PULLUP);
    //pinMode(END_OF_MAZE_PIN, INPUT_PULLUP);
    sncHandler.begin(SERIAL_BAUD);

    // Serial.println("🟠 UART handler initialized (19200 baud)");  // Disabled for performance
    // Serial.println("🟠 Button initialized on GPIO 15");
    // Serial.println("🟠 End-of-Maze pin initialized on GPIO 2");

    // Serial.println("🟠 ========================================");
    // Serial.println("🟠 Commands available:");
    // Serial.println("🟠   GPIO 15: Press button to send SS packet");
    // Serial.println("🟠   GPIO 2:  Pull LOW to activate end-of-maze detection");
    // Serial.println("🟠   Serial: ? (status), r (reset), m (toggle end-of-maze)");
    // Serial.println("🟠 ========================================");
    // Serial.println("🟠 🚀 Enhanced SS Subsystem ready!");
    // Serial.println("🟠 ========================================\n");

    // printSSStatus();  // Disabled for performance
}

void loop() {
    // Update simulation data and check GPIO pins
    //updateSimulationData();
    //checkEndOfMazePin();
    //checkButton();

    // ==================== CONTINUOUS BACKGROUND COLOR SAMPLING ====================
    // Sample colors EVERY LOOP ITERATION in MAZE state for maximum responsiveness (no artificial delay)
    // This achieves 1000+ Hz sampling rate - instant line detection with zero missed detections!
    if (ssStatus.currentSystemState == SYS_MAZE) {
        // Perform color detection on all 3 sensors - NO DELAY, NO THROTTLING
        uint8_t s1 = color_detection(sensor_1);
        uint8_t s2 = color_detection(sensor_2);
        uint8_t s3 = color_detection(sensor_3);

        // Update cache with first non-white color detected (preserves first detection, never overwrites)
        updateCache(s1, s2, s3, currentDistance);
    }

    // Check for incoming packets
    SCSPacket packet;
    if (sncHandler.readPacket(packet)) {
        // printPacket(packet, "📥 RX from SNC:");  // Disabled for performance

        ssStatus.lastControlByte = packet.control;
        SystemState packetSystemState = getSystemState(packet.control);
        SubsystemID packetSubsystem = getSubsystemID(packet.control);
        uint8_t packetIST = getInternalState(packet.control);
        ssStatus.lastPacketTime = millis();
        ssStatus.packetCount++;
        save_dat_1 = packet.dat1;
        save_dat_0 = packet.dat0;

        // Update current distance if this is an MDPS distance packet (IST4)
        if (packetSubsystem == SUB_MDPS && packetIST == 4) {
            currentDistance = ((uint16_t)packet.dat1 << 8) | packet.dat0;
        }

        // Handle state transitions based on SNC packets
        if (packetSystemState == SYS_IDLE && packetSubsystem == SUB_SNC && packetIST == 0) {
            if (packet.dat1 == 1) {
                ssStatus.currentSystemState = SYS_CAL;
                ssStatus.endOfMazePinActive = false;
                has_calibrated = 0;
            } else {
                ssStatus.currentSystemState = SYS_IDLE;
            }
        }
        else if (packetSystemState == SYS_CAL && packetSubsystem == SUB_SNC && packetIST == 0) {
            if (packet.dat1 == 1) {
                ssStatus.currentSystemState = SYS_MAZE;
            } else {
                ssStatus.currentSystemState = SYS_CAL;
            }
        }
        else if (packetSystemState == SYS_MAZE && packetSubsystem == SUB_SNC && packetIST == 1) {
            if (packet.dat1 == 1) {
                ssStatus.currentSystemState = SYS_SOS;
            } else {
                ssStatus.currentSystemState = SYS_MAZE;
            }
        }
        else if (packetSystemState == SYS_SOS && packetSubsystem == SUB_SNC && packetIST == 0) {
            if (packet.dat1 == 1) {
                ssStatus.currentSystemState = SYS_MAZE;
            } else {
                ssStatus.currentSystemState = SYS_SOS;
            }
        }
        else {
            // For non-transition packets, just track the system state from the packet
            ssStatus.currentSystemState = packetSystemState;
        }

        // Serial.printf("🎯 SS Current system state: %s\n", systemStateToString(ssStatus.currentSystemState));  // Disabled for performance

        // Check if SS should respond (now using the CURRENT system state, not packet state)
        if (isSSExpectedToRespond(packet.control)) {
            ssStatus.ssExpectedToSend = true;
            //Serial.println("🟠 🕒 SS IS NOW EXPECTED TO SEND! PRESS GPIO 15 BUTTON! 🕒");
            //Serial.println("🟠 ⚡⚡⚡ READY TO TRANSMIT - WAITING FOR BUTTON ⚡⚡⚡");
        } else {
            ssStatus.ssExpectedToSend = false;
            //Serial.println("🟠 ✅ SS not expected to send - standing by.");
        }
    }

    if(ssStatus.lastControlByte == 16 && ssStatus.currentSystemState == SYS_CAL && has_calibrated == 0){
        // when prev controll byte is 16 and current sys state is cal it will calibrate 
        digitalWrite(2, HIGH);
        calibration();
        has_calibrated = 1;
    } else if(ssStatus.lastControlByte == 97 && ssStatus.currentSystemState == SYS_CAL){
        // when prev control byte is 97 from mdps and current state is calli system will color detect and send color 
        digitalWrite(2, HIGH);
        ssStatus.sensorColors[0] = color_detection(sensor_1); 
        ssStatus.sensorColors[1] = color_detection(sensor_2);  
        ssStatus.sensorColors[2] = color_detection(sensor_3);

    } else if(ssStatus.lastControlByte == 164 && ssStatus.currentSystemState == SYS_MAZE){
        // when last control byte recieved is 164 it will use cached colors or detect if cache empty
        digitalWrite(2, HIGH);

        bool has_new_detection = false; // Track if we have actual color detection (not all white)

        // Use cached colors if available (first non-white detection)
        // Otherwise perform real-time detection (fallback for all-white scenarios)
        if (has_cached_edge || has_cached_middle) {
            // Use cached colors - fill in white for sensors that never detected
            ssStatus.sensorColors[0] = cached_sensor_set[0] ? cached_colors[0] : 0; // WHITE if not cached
            ssStatus.sensorColors[1] = cached_sensor_set[1] ? cached_colors[1] : 0; // WHITE if not cached
            ssStatus.sensorColors[2] = cached_sensor_set[2] ? cached_colors[2] : 0; // WHITE if not cached

            has_new_detection = true; // We have cached non-white colors

            // Use cached distances for angle calculation
            first_distance = cached_edge_distance;
            second_distance = cached_middle_distance;

            // Set toggle if BOTH edge and middle sensors detected WITH VALID distances (needed for angle calc)
            if (has_cached_edge && has_cached_middle &&
                first_distance > 0 && second_distance > 0 && second_distance > first_distance) {
                toggle_middle_sensor = true;
            } else {
                toggle_middle_sensor = false;
            }
        } else {
            // No cached non-white - perform direct detection (likely all white)
            ssStatus.sensorColors[0] = color_detection(sensor_1);
            ssStatus.sensorColors[1] = color_detection(sensor_2);
            ssStatus.sensorColors[2] = color_detection(sensor_3);

            // Check if we detected any non-white colors
            if (shouldUpdateCache(ssStatus.sensorColors[sensor_1]) ||
                shouldUpdateCache(ssStatus.sensorColors[sensor_2]) ||
                shouldUpdateCache(ssStatus.sensorColors[sensor_3])) {
                has_new_detection = true;
            }

            // Track distances for real-time detection
            // FIXED: Use currentDistance (updated from MDPS packets) instead of save_dat_1/dat_0
            // FIXED: Prevent overwrite if both edge sensors detect
            // Check edge sensors first (s1 or s3) for first_distance
            if (ssStatus.sensorColors[sensor_1] == 2 || ssStatus.sensorColors[sensor_1] == 3 || ssStatus.sensorColors[sensor_1] == 4){
                if (first_distance == 0) {  // Only set if not already set
                    first_distance = currentDistance;
                }
            }
            if (ssStatus.sensorColors[sensor_3] == 2 || ssStatus.sensorColors[sensor_3] == 3 || ssStatus.sensorColors[sensor_3] == 4){
                if (first_distance == 0) {  // Only set if not already set
                    first_distance = currentDistance;
                }
            }
            // Check middle sensor for second_distance and angle calculation trigger
            if (ssStatus.sensorColors[sensor_2] == 2 || ssStatus.sensorColors[sensor_2] == 3 || ssStatus.sensorColors[sensor_2] == 4){
                // If we have edge sensor detection, this is second_distance
                // If not, this is a straight-on detection
                if (first_distance > 0) {
                    second_distance = currentDistance;
                    toggle_middle_sensor = true;
                } else {
                    // Straight-on detection (middle only)
                    first_distance = currentDistance;
                    second_distance = first_distance;
                    toggle_middle_sensor = false; // No angle, will be 0
                }
            }
        }

        // STEP 1: Calculate angle FIRST (if applicable) before checking EOM
        // ONLY update angle if we have a new detection (not all white)

        if (has_new_detection) {
            // We have a new color detection - update angle accordingly
            if (toggle_middle_sensor == true) {
                // We have both edge and middle sensor detections - calculate angle
                if (first_distance > 0 && second_distance > first_distance) {
                    distance_for_angle_calc = second_distance - first_distance;
                    float b_extra_distance_due_to_angle = sqrtf(distance_for_angle_calc*distance_for_angle_calc + distance_between_sensors*distance_between_sensors) - distance_between_sensors;
                    float angle = asinf(distance_for_angle_calc/(b_extra_distance_due_to_angle + distance_between_sensors))*180/PI;
                    uint8_t angle_8 = (uint8_t)angle;
                    ssStatus.lastIncidenceAngle = angle_8;

                    // ====== DEBUG OUTPUT FOR NON-ZERO ANGLE ======
                    if (angle_8 > 0) {
                        Serial.println("╔════════════════════════════════════════════════════════════╗");
                        Serial.println("║           🔺 NON-ZERO ANGLE DETECTED! 🔺                  ║");
                        Serial.println("╚════════════════════════════════════════════════════════════╝");
                        Serial.printf("📊 CALCULATION VARIABLES (LATCH VALUES):\n");
                        Serial.printf("   ├─ first_distance (edge):    %d mm\n", first_distance);
                        Serial.printf("   ├─ second_distance (middle): %d mm\n", second_distance);
                        Serial.printf("   ├─ distance_for_angle_calc:  %d mm (delta)\n", distance_for_angle_calc);
                        Serial.printf("   ├─ distance_between_sensors: %.2f mm\n", distance_between_sensors);
                        Serial.printf("   ├─ b_extra (calc):           %.2f mm\n", b_extra_distance_due_to_angle);
                        Serial.printf("   ├─ Raw angle (float):        %.2f degrees\n", angle);
                        Serial.printf("   └─ Final angle (uint8):      %d degrees ✅\n", angle_8);
                        Serial.printf("🎨 COLORS DETECTED:\n");
                        Serial.printf("   └─ [%s, %s, %s]\n",
                                     colorToString(ssStatus.sensorColors[0]),
                                     colorToString(ssStatus.sensorColors[1]),
                                     colorToString(ssStatus.sensorColors[2]));
                        Serial.printf("📦 CACHE STATE:\n");
                        Serial.printf("   ├─ has_cached_edge:          %d\n", has_cached_edge);
                        Serial.printf("   ├─ has_cached_middle:        %d\n", has_cached_middle);
                        Serial.printf("   ├─ cached_edge_distance:     %d mm\n", cached_edge_distance);
                        Serial.printf("   └─ cached_middle_distance:   %d mm\n", cached_middle_distance);
                        Serial.println("════════════════════════════════════════════════════════════");
                    }
                } else {
                    // Invalid distance data - set angle to 0
                    ssStatus.lastIncidenceAngle = 0;
                }
                toggle_middle_sensor = false; // Reset flag after calculation
            } else {
                // No middle sensor detected OR straight-on approach - angle is 0 for THIS detection
                ssStatus.lastIncidenceAngle = 0;
            }
        }
        // No debug output for preserving angle or zero angles

        // STEP 2: NOW check for end-of-maze (with freshly calculated angle)
        bool all_red = (ssStatus.sensorColors[sensor_1] == 1 &&
                        ssStatus.sensorColors[sensor_2] == 1 &&
                        ssStatus.sensorColors[sensor_3] == 1);
        bool any_red = (ssStatus.sensorColors[sensor_1] == 1 ||
                        ssStatus.sensorColors[sensor_2] == 1 ||
                        ssStatus.sensorColors[sensor_3] == 1);

        if (all_red) {
            ssStatus.endOfMazePinActive = true;
        } else if (any_red && ssStatus.lastIncidenceAngle < 5) {
            ssStatus.endOfMazePinActive = true;
        } else {
            ssStatus.endOfMazePinActive = false;
        }

    }
    // ❌ REMOVED: Angle calculation moved inside control byte 164 block to ensure correct ordering




    // FIXED: Enhanced button handling - Check for both expected responses AND incidence angle state
    //if (ssStatus.buttonPressed && !ssStatus.buttonProcessed) {
        bool shouldSend = false;
        
        // Case 1: SS is expected to send based on received packets
        if (ssStatus.ssExpectedToSend) {
            shouldSend = true;
            //Serial.println("🟠 📤 Button pressed - SS expected to send based on received packet");
        }
        // Case 2: SS is waiting to send incidence angle (FIXED - this was missing!)
        else if (ssStatus.sendingState == SSStatus::SEND_WAITING_FOR_INCIDENCE) {
            shouldSend = true;
            ssStatus.ssExpectedToSend = true; // Set this so the packet generation works
            //Serial.println("🟠 📤 Button pressed - SS ready to send INCIDENCE ANGLE (sequential send)");
        }
        
        if (shouldSend) {
            SCSPacket ssPacket = generateSSPacket();
            digitalWrite(2, LOW);
            if (ssPacket.control != 0) {
                // Serial.println("🟠 ==========================================");  // Disabled for performance
                // Serial.println("🟠           TRANSMITTING SS PACKET");
                // Serial.println("🟠 ==========================================");
                // printPacket(ssPacket, "📤 TX to SNC:");
                digitalWrite(2, LOW);
                sncHandler.sendPacket(ssPacket);
                // Serial.println("🟠 ✅ SS PACKET TRANSMITTED SUCCESSFULLY! ✅");
                // Serial.println("🟠 ==========================================");
                
                // Update status
                ssStatus.lastTransmissionTime = millis();
                ssStatus.transmissionCount++;
                
                // FIXED: Always reset these flags after sending
                ssStatus.buttonPressed = false;
                ssStatus.buttonProcessed = true;

                // Special case: If we just sent colors in MAZE, prepare for next incidence angle send
                // Note: sendingState will be SEND_NONE by now (cleared in generateSSPacket)
                // So we need to check a different way or just always reset
                // Serial.println("🔔 Colors sent - SS is now ready to send incidence angle on NEXT button press!");  // Disabled for performance
                ssStatus.ssExpectedToSend = false; // Reset after sending to prevent continuous resending
                
            } else {
                Serial.println("🟠 ❌ FAILED TO GENERATE SS PACKET ❌");
                ssStatus.buttonPressed = false;
                ssStatus.buttonProcessed = true;
                ssStatus.ssExpectedToSend = false;
            }
        } //else {
        //     Serial.println("🟠 🔘 Button pressed but SS not expected to send - ignoring");
        //     ssStatus.buttonPressed = false;
        //     ssStatus.buttonProcessed = true;
        // }
    //}
    
    // Reset button processed flag after some time to allow new button presses
    // if (ssStatus.buttonProcessed && (millis() - ssStatus.lastButtonPress > 500)) {
    //     ssStatus.buttonProcessed = false;
    // }
    
    // Handle serial commands
    if (Serial.available()) {
        char cmd = Serial.read();
        switch (cmd) {
            case '?':
                printSSStatus();
                break;
            case 'r': case 'R':
                // Reset SS status
                ssStatus.currentSystemState = SYS_IDLE;
                ssStatus.ssExpectedToSend = false;
                ssStatus.calibrationComplete = false;
                ssStatus.endOfMazeDetected = false;
                ssStatus.packetCount = 0;
                ssStatus.sendingState = SSStatus::SEND_NONE;
                ssStatus.buttonPressed = false;
                ssStatus.buttonProcessed = false;
                Serial.println("🟠 SS subsystem reset");
                break;
            case 'm': case 'M':
                // Toggle end-of-maze detection (for testing without GPIO)
                ssStatus.endOfMazePinActive = !ssStatus.endOfMazePinActive;
                Serial.printf("🟠 End-of-maze PIN simulation: %s\n", 
                            ssStatus.endOfMazePinActive ? "ACTIVE" : "INACTIVE");
                break;
        }
    }
    
    // Periodic status update - Disabled for performance
    // static unsigned long lastStatusUpdate = 0;
    // if (millis() - lastStatusUpdate > 5000) { // Every 5 seconds
    //     Serial.printf("🟠 *** QUICK STATUS: %s | Expected: %s | RX: %d | TX: %d | SendState: %d | LastByte: %d | s1: %d | s2: %d | s3: %d | fd: %d | sd %d | angle: %d***\n",
    //                   systemStateToString(ssStatus.currentSystemState),
    //                   ssStatus.ssExpectedToSend ? "YES ⚡" : "NO",
    //                   ssStatus.packetCount,
    //                   ssStatus.transmissionCount,
    //                   ssStatus.sendingState,
    //                   ssStatus.lastControlByte,
    //                   ssStatus.sensorColors[0],
    //                   ssStatus.sensorColors[1],
    //                   ssStatus.sensorColors[2],
    //                   first_distance,
    //                   second_distance,
    //                   ssStatus.lastIncidenceAngle);
    //     lastStatusUpdate = millis();
    // }

    // delay(10);  // Removed for better performance
}


void calibration()//CCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCC
{
    int red_on_red[] = {0, 0, 0};
    int red_on_green[] = {0, 0, 0};
    int red_on_blue[] = {0, 0, 0};


    int green_on_red[] = {0, 0, 0};
    int green_on_green[] = {0, 0, 0};
    int green_on_blue[] = {0, 0, 0};


    int blue_on_red[] = {0, 0, 0};
    int blue_on_green[] = {0, 0, 0};
    int blue_on_blue[] = {0, 0, 0};
    digitalWrite(red_led, LOW);
    digitalWrite(green_led, LOW);
    digitalWrite(blue_led, LOW);

    for (int i = 0; i <= 20; i++)
    {
      digitalWrite(red_led, HIGH);
      delay(100);
      digitalWrite(red_led, LOW);
      delay(100);
    }
    red_on_red[sensor_1] = flash_n_scan(red, sensor_1);
    red_on_red[sensor_2] = flash_n_scan(red, sensor_2);
    red_on_red[sensor_3] = flash_n_scan(red, sensor_3);
    green_on_red[sensor_1] = flash_n_scan(green, sensor_1);
    green_on_red[sensor_2] = flash_n_scan(green, sensor_2);
    green_on_red[sensor_3] = flash_n_scan(green, sensor_3);
    blue_on_red[sensor_1] = flash_n_scan(blue, sensor_1);
    blue_on_red[sensor_2] = flash_n_scan(blue, sensor_2);
    blue_on_red[sensor_3] = flash_n_scan(blue, sensor_3);
    digitalWrite(red_led, LOW);
    digitalWrite(green_led, LOW);
    digitalWrite(blue_led, LOW);


    for (int i = 0; i <= 20; i++)
    {
      digitalWrite(green_led, HIGH);
      delay(100);
      digitalWrite(green_led, LOW);
      delay(100);
    }
    red_on_green[sensor_1] = flash_n_scan(red, sensor_1);
    red_on_green[sensor_2] = flash_n_scan(red, sensor_2);
    red_on_green[sensor_3] = flash_n_scan(red, sensor_3);
    green_on_green[sensor_1] = flash_n_scan(green, sensor_1);
    green_on_green[sensor_2] = flash_n_scan(green, sensor_2);
    green_on_green[sensor_3] = flash_n_scan(green, sensor_3);
    blue_on_green[sensor_1] = flash_n_scan(blue, sensor_1);
    blue_on_green[sensor_2] = flash_n_scan(blue, sensor_2);
    blue_on_green[sensor_3] = flash_n_scan(blue, sensor_3);
    digitalWrite(red_led, LOW);
    digitalWrite(green_led, LOW);
    digitalWrite(blue_led, LOW);


    for (int i = 0; i <= 20; i++)
    {
      digitalWrite(blue_led, HIGH);
      delay(100);
      digitalWrite(blue_led, LOW);
      delay(100);
    }
    red_on_blue[sensor_1] = flash_n_scan(red, sensor_1);
    red_on_blue[sensor_2] = flash_n_scan(red, sensor_2);
    red_on_blue[sensor_3] = flash_n_scan(red, sensor_3);
    green_on_blue[sensor_1] = flash_n_scan(green, sensor_1);
    green_on_blue[sensor_2] = flash_n_scan(green, sensor_2);
    green_on_blue[sensor_3] = flash_n_scan(green, sensor_3);
    blue_on_blue[sensor_1] = flash_n_scan(blue, sensor_1);
    blue_on_blue[sensor_2] = flash_n_scan(blue, sensor_2);
    blue_on_blue[sensor_3] = flash_n_scan(blue, sensor_3);
    digitalWrite(red_led, LOW);
    digitalWrite(green_led, LOW);
    digitalWrite(blue_led, LOW);


    if (red_on_green[sensor_1]> red_on_blue[sensor_1]) { 
      red_threshold[sensor_1] = (red_on_red[sensor_1] + red_on_green[sensor_1])/2;
    } else{ 
      red_threshold[sensor_1] = (red_on_red[sensor_1] + red_on_blue[sensor_1])/2;
    }

    if (red_on_green[sensor_2]> red_on_blue[sensor_2]) { 
      red_threshold[sensor_2] = (red_on_red[sensor_2] + red_on_green[sensor_2])/2;
    } else{ 
      red_threshold[sensor_2] = (red_on_red[sensor_2] + red_on_blue[sensor_2])/2;
    }

    if (red_on_green[sensor_3]> red_on_blue[sensor_3]) { 
      red_threshold[sensor_3] = (red_on_red[sensor_3] + red_on_green[sensor_3])/2;
    } else{ 
      red_threshold[sensor_3] = (red_on_red[sensor_3] + red_on_blue[sensor_3])/2;
    }

    if (green_on_red[sensor_1] > green_on_blue[sensor_1]) {
      green_threshold[sensor_1] = (green_on_green[sensor_1] +green_on_red[sensor_1])/2;
    } else {
      green_threshold[sensor_1] = (green_on_green[sensor_1] + green_on_blue[sensor_1])/2;
    }

    if (green_on_red[sensor_2] > green_on_blue[sensor_2]) {
      green_threshold[sensor_2] = (green_on_green[sensor_2] +green_on_red[sensor_2])/2;
    } else {
      green_threshold[sensor_2] = (green_on_green[sensor_2] + green_on_blue[sensor_2])/2;
    }

    if (green_on_red[sensor_3] > green_on_blue[sensor_3]) {
      green_threshold[sensor_3] = (green_on_green[sensor_3] +green_on_red[sensor_3])/2;
    } else {
      green_threshold[sensor_3] = (green_on_green[sensor_3] + green_on_blue[sensor_3])/2;
    }

    if (blue_on_green[sensor_1] > blue_on_red[sensor_1]) {
      blue_threshold[sensor_1] = (blue_on_blue[sensor_1] + blue_on_green[sensor_1])/2;
    } else {
      blue_threshold[sensor_1] = (blue_on_blue[sensor_1] + blue_on_red[sensor_1])/2;
    }

    if (blue_on_green[sensor_2] > blue_on_red[sensor_2]) {
      blue_threshold[sensor_2] = (blue_on_blue[sensor_2] + blue_on_green[sensor_2])/2;
    } else {
      blue_threshold[sensor_2] = (blue_on_blue[sensor_2] + blue_on_red[sensor_2])/2;
    }

    if (blue_on_green[sensor_3] > blue_on_red[sensor_3]) {
      blue_threshold[sensor_3] = (blue_on_blue[sensor_3] + blue_on_green[sensor_3])/2;
    } else {
      blue_threshold[sensor_3] = (blue_on_blue[sensor_3] + blue_on_red[sensor_3])/2;
    }
}//CCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCc

uint8_t color_detection(int sensor)
{
  int red_test = 0;
  int green_test = 0;
  int blue_test = 0;

  red_test = flash_n_scan(red, sensor);
  green_test = flash_n_scan(green, sensor);
  blue_test = flash_n_scan(blue, sensor);


  if (red_test >= red_threshold[sensor]){red_test = 1;display_array(sensor, red, 1);} else { red_test = 0;display_array(sensor, red, 0); }
  if (green_test >= green_threshold[sensor]){green_test = 1;display_array(sensor, green, 1);} else { green_test = 0;display_array(sensor, green, 0); }
  if (blue_test >= blue_threshold[sensor]){blue_test = 1;display_array(sensor, blue, 1);} else { blue_test = 0;display_array(sensor, blue, 0); }

  
  if ( red_test == 1 && green_test == 0 && blue_test ==0){return 1;}
  else if ( red_test == 0 && green_test == 1 && blue_test ==0){return 2;}
  else if ( red_test == 0 && green_test == 0 && blue_test ==1){return 3;}
  else if ( red_test == 0 && green_test == 0 && blue_test ==0){return 4;}
  else {return 0;}
}

void display_array(int sensor, int color, int status)
{
    if (sensor == sensor_1){
      if (color == red){
        digitalWrite(19, status);
      }else if(color == green){
        digitalWrite(18, status);
      }else if(color == blue){
        digitalWrite(5, status);
      }
    } else if (sensor == sensor_2){
      if (color == red){
        digitalWrite(23, status);
      }else if(color == green){
        digitalWrite(22, status);
      }else if(color == blue){
        digitalWrite(21, status);
      }
    } else if (sensor == sensor_3){
      if (color == red){
        digitalWrite(25, status);
      }else if(color == green){
        digitalWrite(32, status);
      }else if(color == blue){
        digitalWrite(33, status);
      }
    }

}

int flash_n_scan(int color, int sensor)//ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff
{
  digitalWrite(red_led, LOW);
  digitalWrite(green_led, LOW);
  digitalWrite(blue_led, LOW);
  if ( color == 0)
  {
    digitalWrite(red_led, HIGH);
  } else if(color == 1)
  {
    digitalWrite(green_led, HIGH);
  } else if(color == 2)
  {
    digitalWrite(blue_led, HIGH);
  }
  delay(sample_time);
  if (sensor == 0){return analogRead(36);}
  else if(sensor == 1){return analogRead(39);}
  else if(sensor == 2){return analogRead(34);} 
}//fffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff
