// MARV A-Maze-Eng - FIXED SS (Sensor Subsystem) Phase 0
// Integrated with SNC State Machine for seamless communication
// FIXED: SOS state logic - SS sends NO packets during SOS state
// FIXED: Incidence angle sending issue resolved

#include <HardwareSerial.h>

// ==================== PIN DEFINITIONS ====================
// UART pins for subsystem communication
#define RX_PIN 16            // UART2 RX from SNC
#define TX_PIN 17            // UART2 TX to SNC
#define BUTTON_PIN 15        // GPIO 15 for triggering SEND
#define END_OF_MAZE_PIN 2    // GPIO 2 for end-of-maze detection
#define SERIAL_BAUD 19200

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
            
            if (currentTime - lastByteTime > 100 && bufferIndex > 0) {
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
uint8_t encodeColours(uint8_t s1, uint8_t s2, uint8_t s3) {
    return ((s1 & 0x07) << 6) | ((s2 & 0x07) << 3) | (s3 & 0x07);
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

// ==================== FIXED SS EXPECTED TO RESPOND LOGIC ====================
bool isSSExpectedToRespond(uint8_t controlByte) {
    SystemState sys = getSystemState(controlByte);
    SubsystemID sub = getSubsystemID(controlByte);
    uint8_t ist = getInternalState(controlByte);
    
    Serial.printf("🔍 Checking if SS should respond to: [%s:%s:IST%d] Control:0x%02X\n",
                  systemStateToString(sys), subsystemToString(sub), ist, controlByte);
    
    // *** CRITICAL FIX: SS sends NO packets in IDLE or SOS states ***
    if (ssStatus.currentSystemState == SYS_IDLE) {
        Serial.println("❌ SS in IDLE state - NO packets allowed");
        return false;
    }
    
    if (ssStatus.currentSystemState == SYS_SOS) {
        Serial.println("❌ SS in SOS state - NO packets allowed (motors stopped, waiting for pure tone)");
        return false;
    }
    
    // Based on SCS state diagram, SS responds when:
    // 1. After receiving SNC touch in IDLE (which causes transition to CAL) - SS sends End of Calibration
    if (sys == SYS_IDLE && sub == SUB_SNC && ist == 0) {
        Serial.println("✅ SS should send: End of Calibration (after SNC touch)");
        return true;
    }
    // 2. CAL: After MDPS battery level (IST1) - SS sends Colors (CAL)
    else if (sys == SYS_CAL && sub == SUB_MDPS && ist == 1) {
        Serial.println("✅ SS should send: Colors (CAL) (after MDPS battery level)");
        return true;
    }
    // 3. MAZE: After MDPS distance (IST4) - SS sends Colors (MAZE) or End-of-Maze
    // *** IMPORTANT: Only if SS is currently in MAZE state ***
    else if (sys == SYS_MAZE && sub == SUB_MDPS && ist == 4 && ssStatus.currentSystemState == SYS_MAZE) {
        Serial.println("✅ SS should send: Colors (MAZE) or End-of-Maze (after MDPS distance)");
        return true;
    }
    // *** FIX: Handle SOS MDPS packets correctly ***
    else if (sys == SYS_SOS && sub == SUB_MDPS && ist == 4) {
        Serial.println("❌ MDPS SOS response - SS does NOT respond (system in SOS state)");
        return false;
    }
    
    // Log what we're NOT responding to for clarity
    if (sys == SYS_CAL && sub == SUB_MDPS && ist == 0) {
        Serial.println("ℹ️  MDPS vop calibration - SS does NOT respond, waiting for MDPS battery level");
    }
    else if (sys == SYS_MAZE && sub == SUB_MDPS && ist == 1) {
        Serial.println("ℹ️  MDPS battery level (MAZE) - SS does NOT respond");
    }
    else if (sys == SYS_MAZE && sub == SUB_MDPS && ist == 2) {
        Serial.println("ℹ️  MDPS rotation - SS does NOT respond");
    }
    else if (sys == SYS_MAZE && sub == SUB_MDPS && ist == 3) {
        Serial.println("ℹ️  MDPS speed - SS does NOT respond");
    }
    else if (sys == SYS_MAZE && sub == SUB_SS && ist == 2) {
        Serial.println("ℹ️  SS just sent Incidence Angle - SS does NOT respond to its own packet");
    }
    else if (sys == SYS_MAZE && sub == SUB_MDPS && ist == 4 && ssStatus.currentSystemState != SYS_MAZE) {
        Serial.printf("ℹ️  MDPS distance packet but SS not in MAZE state (SS in %s) - SS does NOT respond\n", 
                     systemStateToString(ssStatus.currentSystemState));
    }
    else {
        Serial.println("❌ SS should NOT respond to this packet");
    }
    
    return false;
}

// ==================== FIXED SS PACKET GENERATION ====================
SCSPacket generateSSPacket() {
    SCSPacket packet;
    SystemState currentSys = ssStatus.currentSystemState;
    SystemState packetSys = getSystemState(ssStatus.lastControlByte);
    SubsystemID lastSub = getSubsystemID(ssStatus.lastControlByte);
    uint8_t lastIST = getInternalState(ssStatus.lastControlByte);
    
    Serial.printf("🎯 Generating SS packet:\n");
    Serial.printf("    Current SS state: %s\n", systemStateToString(currentSys));
    Serial.printf("    Last packet: [%s:%s:IST%d]\n", 
                  systemStateToString(packetSys), subsystemToString(lastSub), lastIST);
    Serial.printf("    Sending state: %d\n", ssStatus.sendingState);
    
    // *** CRITICAL: SS cannot send packets in IDLE or SOS states ***
    if (currentSys == SYS_IDLE || currentSys == SYS_SOS) {
        Serial.printf("❌ SS cannot send packets in %s state!\n", systemStateToString(currentSys));
        return packet;  // Return empty packet
    }
    
    // Handle initial transition to CAL - SS sends End of Calibration
    if (packetSys == SYS_IDLE && lastSub == SUB_SNC && lastIST == 0 && currentSys == SYS_CAL) {
        packet.control = createControlByte(SYS_CAL, SUB_SS, 0);
        packet.dat1 = 0;
        packet.dat0 = 0;
        packet.dec = 0;
        ssStatus.calibrationComplete = true;
        ssStatus.sendingState = SSStatus::SEND_NONE;
        Serial.println("📤 Sending: End of Calibration (initial CAL entry)");
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
        Serial.printf("📤 Generating: Incidence Angle - %d degrees (sequential send after Colors)\n", ssStatus.lastIncidenceAngle);
        return packet;
    }
    
    // Handle responses based on current system state and last received packet
    if (currentSys == SYS_CAL) {
        if (lastSub == SUB_MDPS && lastIST == 1) {
            // After MDPS battery level - Send Colors (CAL)
            packet.control = createControlByte(SYS_CAL, SUB_SS, 1);
            packet.dat1 = 0;
            packet.dat0 = encodeColours(1, 0, 3); // Red, White, Blue
            packet.dec = 0;
            ssStatus.sendingState = SSStatus::SEND_NONE;
            Serial.println("📤 Generating: Colors (CAL) - Red, White, Blue");
        }
    }
    else if (currentSys == SYS_MAZE) {
        // *** FIXED: Only respond to MAZE MDPS distance, not SOS MDPS distance ***
        if (lastSub == SUB_MDPS && lastIST == 4 && packetSys == SYS_MAZE) {
            // After MDPS distance IN MAZE STATE - Check GPIO 2 to determine what to send
            Serial.println("🔍 MDPS MAZE distance received - checking end-of-maze GPIO 2 state");
            
            if (ssStatus.endOfMazePinActive) {
                // GPIO 2 is active - send End-of-Maze
                ssStatus.endOfMazeDetected = true;
                packet.control = createControlByte(SYS_MAZE, SUB_SS, 3);
                packet.dat1 = 0;
                packet.dat0 = 0;
                packet.dec = 0;
                ssStatus.sendingState = SSStatus::SEND_NONE;
                Serial.println("📤 Generating: End-of-Maze (GPIO 2 is ACTIVE)");
            } else {
                // GPIO 2 is not active - send Colors (MAZE)
                ssStatus.endOfMazeDetected = false;
                packet.control = createControlByte(SYS_MAZE, SUB_SS, 1);
                packet.dat1 = 0;
                packet.dat0 = encodeColours(ssStatus.sensorColors[0], 
                                          ssStatus.sensorColors[1], 
                                          ssStatus.sensorColors[2]);
                packet.dec = 0;
                ssStatus.sendingState = SSStatus::SEND_WAITING_FOR_INCIDENCE; // Set state for next send
                Serial.printf("📤 Generating: Colors (MAZE) - %s, %s, %s (GPIO 2 is INACTIVE)\n",
                            colorToString(ssStatus.sensorColors[0]),
                            colorToString(ssStatus.sensorColors[1]),
                            colorToString(ssStatus.sensorColors[2]));
                Serial.println("🔔 Next button press will send incidence angle");
            }
        }
        // *** NEW: Handle case where we received SOS MDPS packet ***
        else if (lastSub == SUB_MDPS && lastIST == 4 && packetSys == SYS_SOS) {
            Serial.println("🔍 MDPS SOS response received - SS will NOT send any packet (SOS state)");
            // Don't generate any packet - return empty
            return packet;
        }
    }
    
    return packet;
}

// ==================== FIXED STATE TRANSITION LOGIC ====================
void handleStateTransitions(const SCSPacket& packet) {
    SystemState packetSystemState = getSystemState(packet.control);
    SubsystemID packetSubsystem = getSubsystemID(packet.control);
    uint8_t packetIST = getInternalState(packet.control);
    
    // Handle state transitions based on SNC packets AND system state changes
    if (packetSystemState == SYS_IDLE && packetSubsystem == SUB_SNC && packetIST == 0) {
        if (packet.dat1 == 1) {
            // Touch detected - system transitioning to CAL
            ssStatus.currentSystemState = SYS_CAL;
            Serial.println("🔄 SS STATE TRANSITION: IDLE → CAL (Touch detected by SNC)");
        } else {
            ssStatus.currentSystemState = SYS_IDLE;
        }
    }
    else if (packetSystemState == SYS_CAL && packetSubsystem == SUB_SNC && packetIST == 0) {
        if (packet.dat1 == 1) {
            // Second touch detected - system transitioning to MAZE
            ssStatus.currentSystemState = SYS_MAZE;
            Serial.println("🔄 SS STATE TRANSITION: CAL → MAZE (Second touch detected by SNC)");
        } else {
            ssStatus.currentSystemState = SYS_CAL;
        }
    }
    else if (packetSystemState == SYS_MAZE && packetSubsystem == SUB_SNC && packetIST == 1) {
        if (packet.dat1 == 1) {
            // Pure tone detected - system transitioning to SOS
            ssStatus.currentSystemState = SYS_SOS;
            Serial.println("🔄 SS STATE TRANSITION: MAZE → SOS (Pure tone detected by SNC)");
        } else {
            ssStatus.currentSystemState = SYS_MAZE;
        }
    }
    // *** FIX: Handle MDPS SOS response properly ***
    else if (packetSystemState == SYS_SOS && packetSubsystem == SUB_MDPS && packetIST == 4) {
        // MDPS sent SOS response - SS should stay in SOS state
        ssStatus.currentSystemState = SYS_SOS;
        Serial.println("🔄 SS: Received MDPS SOS response - staying in SOS state");
    }
    else if (packetSystemState == SYS_SOS && packetSubsystem == SUB_SNC && packetIST == 0) {
        if (packet.dat1 == 1) {
            // Pure tone detected in SOS - system transitioning back to MAZE
            ssStatus.currentSystemState = SYS_MAZE;
            Serial.println("🔄 SS STATE TRANSITION: SOS → MAZE (Pure tone detected by SNC)");
        } else {
            ssStatus.currentSystemState = SYS_SOS;
        }
    }
    else {
        // *** FIX: Don't automatically copy packet state - be more careful ***
        // Only update state for actual state transition packets, not all packets
        if (packetSubsystem == SUB_SNC) {
            // SNC packets can indicate state transitions
            ssStatus.currentSystemState = packetSystemState;
        }
        // For MDPS and SS packets, don't change state unless it's a specific transition
    }
    
    Serial.printf("🎯 SS Current system state: %s\n", systemStateToString(ssStatus.currentSystemState));
}

// ==================== GPIO MONITORING ====================
void checkEndOfMazePin() {
    static bool lastPinState = HIGH;
    bool currentPinState = digitalRead(END_OF_MAZE_PIN);
    
    // Update the raw pin state
    ssStatus.endOfMazePinActive = (currentPinState == LOW); // Active low
    
    // Log pin state changes for debugging
    if (lastPinState != currentPinState) {
        Serial.printf("🟠 🔍 End-of-Maze PIN GPIO 2: %s\n", 
                     ssStatus.endOfMazePinActive ? "ACTIVE (LOW)" : "INACTIVE (HIGH)");
    }
    
    lastPinState = currentPinState;
}

// FIXED: Better button handling with proper debounce
void checkButton() {
    static bool lastButtonState = HIGH;
    bool currentButtonState = digitalRead(BUTTON_PIN);
    unsigned long currentTime = millis();
    
    // Button pressed (falling edge) with debounce
    if (lastButtonState == HIGH && currentButtonState == LOW) {
        if (currentTime - ssStatus.lastButtonPress > 200) { // 200ms debounce
            ssStatus.buttonPressed = true;
            ssStatus.buttonProcessed = false;
            ssStatus.lastButtonPress = currentTime;
            Serial.println("🟠 🔘 BUTTON PRESSED! SS PACKET READY TO TRANSMIT! 🔘");
        }
    }
    
    lastButtonState = currentButtonState;
}

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
    Serial.println("\n🟠 ============================================");
    Serial.println("🟠           SS SUBSYSTEM STATUS");
    Serial.println("🟠 ============================================");
    
    Serial.printf("🟠 Current System State: %s\n", systemStateToString(ssStatus.currentSystemState));
    Serial.printf("🟠 Last Control Byte: 0x%02X\n", ssStatus.lastControlByte);
    Serial.printf("🟠 SS Expected to Send: %s\n", ssStatus.ssExpectedToSend ? "YES ⚡" : "NO");
    Serial.printf("🟠 Button Ready: %s\n", ssStatus.buttonPressed ? "YES ⚡" : "NO");
    Serial.printf("🟠 Sending State: %d\n", ssStatus.sendingState);
    Serial.printf("🟠 Packets Received: %d\n", ssStatus.packetCount);
    Serial.printf("🟠 Packets Transmitted: %d\n", ssStatus.transmissionCount);
    Serial.printf("🟠 Calibration Complete: %s\n", ssStatus.calibrationComplete ? "YES ✅" : "NO");
    Serial.printf("🟠 End-of-Maze PIN (GPIO 2): %s\n", ssStatus.endOfMazePinActive ? "ACTIVE ⚡" : "INACTIVE");
    Serial.printf("🟠 End of Maze Detected: %s\n", ssStatus.endOfMazeDetected ? "YES ✅" : "NO");
    
    if (ssStatus.lastTransmissionTime > 0) {
        unsigned long timeSinceTransmission = (millis() - ssStatus.lastTransmissionTime) / 1000;
        Serial.printf("🟠 Last Transmission: %lu seconds ago\n", timeSinceTransmission);
    } else {
        Serial.println("🟠 Last Transmission: NEVER");
    }
    
    Serial.println("🟠 *** SENSOR READINGS ***");
    Serial.printf("🟠 Sensor 1: %s, Sensor 2: %s, Sensor 3: %s\n",
                  colorToString(ssStatus.sensorColors[0]),
                  colorToString(ssStatus.sensorColors[1]),
                  colorToString(ssStatus.sensorColors[2]));
    Serial.printf("🟠 Last Incidence Angle: %d degrees\n", ssStatus.lastIncidenceAngle);
    
    Serial.println("🟠 ============================================\n");
}

// ==================== ARDUINO SETUP AND LOOP ====================
void setup() {
    Serial.begin(115200);
    delay(1000);
    
    Serial.println("🟠 ========================================");
    Serial.println("🟠 MARV SS Subsystem - FIXED PHASE 0");
    Serial.println("🟠 Integrated with SNC State Machine");
    Serial.println("🟠 FIXED: SOS state logic - SS silent in SOS");
    Serial.println("🟠 ========================================");
    
    pinMode(BUTTON_PIN, INPUT_PULLUP);
    pinMode(END_OF_MAZE_PIN, INPUT_PULLUP);
    sncHandler.begin(SERIAL_BAUD);
    
    Serial.println("🟠 UART handler initialized (19200 baud)");
    Serial.println("🟠 Button initialized on GPIO 15");
    Serial.println("🟠 End-of-Maze pin initialized on GPIO 2");
    
    Serial.println("🟠 ========================================");
    Serial.println("🟠 Commands available:");
    Serial.println("🟠   GPIO 15: Press button to send SS packet");
    Serial.println("🟠   GPIO 2:  Pull LOW to activate end-of-maze detection");
    Serial.println("🟠   Serial: ? (status), r (reset), m (toggle end-of-maze)");
    Serial.println("🟠 ========================================");
    Serial.println("🟠 🚀 Fixed SS Subsystem ready!");
    Serial.println("🟠 ========================================\n");
    
    printSSStatus();
}

void loop() {
    // Update simulation data and check GPIO pins
    updateSimulationData();
    checkEndOfMazePin();
    checkButton();
    
    // Check for incoming packets
    SCSPacket packet;
    if (sncHandler.readPacket(packet)) {
        printPacket(packet, "📥 RX from SNC:");
        
        ssStatus.lastControlByte = packet.control;
        SystemState packetSystemState = getSystemState(packet.control);
        SubsystemID packetSubsystem = getSubsystemID(packet.control);
        uint8_t packetIST = getInternalState(packet.control);
        ssStatus.lastPacketTime = millis();
        ssStatus.packetCount++;
        
        // Handle state transitions
        handleStateTransitions(packet);
        
        // Check if SS should respond (now using the CURRENT system state, not packet state)
        if (isSSExpectedToRespond(packet.control)) {
            ssStatus.ssExpectedToSend = true;
            Serial.println("🟠 🕒 SS IS NOW EXPECTED TO SEND! PRESS GPIO 15 BUTTON! 🕒");
            Serial.println("🟠 ⚡⚡⚡ READY TO TRANSMIT - WAITING FOR BUTTON ⚡⚡⚡");
        } else {
            ssStatus.ssExpectedToSend = false;
            Serial.println("🟠 ✅ SS not expected to send - standing by.");
        }
    }
    
    // FIXED: Enhanced button handling - Check for both expected responses AND incidence angle state
    if (ssStatus.buttonPressed && !ssStatus.buttonProcessed) {
        bool shouldSend = false;
        
        // Case 1: SS is expected to send based on received packets
        if (ssStatus.ssExpectedToSend) {
            shouldSend = true;
            Serial.println("🟠 📤 Button pressed - SS expected to send based on received packet");
        }
        // Case 2: SS is waiting to send incidence angle (FIXED - this was missing!)
        else if (ssStatus.sendingState == SSStatus::SEND_WAITING_FOR_INCIDENCE) {
            shouldSend = true;
            ssStatus.ssExpectedToSend = true; // Set this so the packet generation works
            Serial.println("🟠 📤 Button pressed - SS ready to send INCIDENCE ANGLE (sequential send)");
        }
        
        if (shouldSend) {
            SCSPacket ssPacket = generateSSPacket();
            
            if (ssPacket.control != 0) {
                Serial.println("🟠 ==========================================");
                Serial.println("🟠           TRANSMITTING SS PACKET");
                Serial.println("🟠 ==========================================");
                printPacket(ssPacket, "📤 TX to SNC:");
                sncHandler.sendPacket(ssPacket);
                Serial.println("🟠 ✅ SS PACKET TRANSMITTED SUCCESSFULLY! ✅");
                Serial.println("🟠 ==========================================");
                
                // Update status
                ssStatus.lastTransmissionTime = millis();
                ssStatus.transmissionCount++;
                
                // FIXED: Always reset these flags after sending
                ssStatus.buttonPressed = false;
                ssStatus.buttonProcessed = true;
                
                // Special case: If we just sent colors in MAZE, prepare for next incidence angle send
                if (ssStatus.sendingState == SSStatus::SEND_WAITING_FOR_INCIDENCE) {
                    Serial.println("🔔 Colors sent - SS is now ready to send incidence angle on NEXT button press!");
                    ssStatus.ssExpectedToSend = true;  // Keep ready for next send
                } else {
                    ssStatus.ssExpectedToSend = false; // Normal case - done sending
                }
                
            } else {
                Serial.println("🟠 ❌ FAILED TO GENERATE SS PACKET ❌");
                ssStatus.buttonPressed = false;
                ssStatus.buttonProcessed = true;
                ssStatus.ssExpectedToSend = false;
            }
        } else {
            Serial.println("🟠 🔘 Button pressed but SS not expected to send - ignoring");
            ssStatus.buttonPressed = false;
            ssStatus.buttonProcessed = true;
        }
    }
    
    // Reset button processed flag after some time to allow new button presses
    if (ssStatus.buttonProcessed && (millis() - ssStatus.lastButtonPress > 500)) {
        ssStatus.buttonProcessed = false;
    }
    
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
    
    // Periodic status update
    static unsigned long lastStatusUpdate = 0;
    if (millis() - lastStatusUpdate > 15000) { // Every 15 seconds
        Serial.printf("🟠 *** QUICK STATUS: %s | Expected: %s | RX: %d | TX: %d | SendState: %d ***\n",
                      systemStateToString(ssStatus.currentSystemState),
                      ssStatus.ssExpectedToSend ? "YES ⚡" : "NO",
                      ssStatus.packetCount,
                      ssStatus.transmissionCount,
                      ssStatus.sendingState);
        lastStatusUpdate = millis();
    }
    
    delay(10);
}