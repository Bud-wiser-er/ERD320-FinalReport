#include <HardwareSerial.h>

// ==================== PIN DEFINITIONS ====================
// UART pins for subsystem communication
#define RX_SS   21
#define TX_SS   22
#define RX_MDPS 16
#define TX_MDPS 17

// GPIO Command pins (inputs from WiFi ESP32)
#define CMD_TOUCH_PIN     4   // Input from WiFi ESP32 GPIO 4
#define CMD_TONE_PIN      2   // Input from WiFi ESP32 GPIO 2
#define CMD_SEND_PIN      15  // Input from WiFi ESP32 GPIO 15

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
SerialPacketHandler ssHandler(&Serial1, RX_SS, TX_SS);
SerialPacketHandler mdpsHandler(&Serial2, RX_MDPS, TX_MDPS);

// ==================== SYSTEM STATUS TRACKING ====================
struct SystemStatus {
    SystemState currentSystemState;
    unsigned long lastTransitionTime;
    
    // Next expected packet information
    SystemState nextExpectedSystemState;
    SubsystemID nextExpectedSubsystem;
    uint8_t nextExpectedIST;
    String nextExpectedDescription;
    
    // Manual control flags (from WiFi interface)
    bool touchDetected;
    bool pureToneDetected;
    bool manualSendTrigger;
    
    // State tracking
    bool waitingForSecondTouch; // In CAL state, waiting for 2nd touch
    bool justSentPureToneDetection; // NEW: Track if we just sent pure tone detection
    
    // Error tracking
    int unexpectedPacketCount;
    int gpioCommandCount;
    
    // Last packets for display (changed from int to String)
    String lastSSPacket;
    String lastMDPSPacket;
    String lastSNCPacket;
};

SystemStatus systemStatus = {
    SYS_IDLE,        // currentSystemState - Start in IDLE
    0,               // lastTransitionTime
    SYS_IDLE,        // nextExpectedSystemState
    SUB_SNC,         // nextExpectedSubsystem  
    0,               // nextExpectedIST
    "Touch Detection", // nextExpectedDescription
    false,           // touchDetected
    false,           // pureToneDetected
    false,           // manualSendTrigger
    false,           // waitingForSecondTouch
    false,           // justSentPureToneDetection - NEW
    0,               // unexpectedPacketCount
    0,               // gpioCommandCount
    "",              // lastSSPacket
    "",              // lastMDPSPacket
    ""               // lastSNCPacket
};
// ==================== NEXT EXPECTED STATE LOGIC ====================
void updateNextExpectedState() {
    // Determine what should come next based on current system state and SCS flow
    switch (systemStatus.currentSystemState) {
        case SYS_IDLE:
            systemStatus.nextExpectedSystemState = SYS_IDLE;
            systemStatus.nextExpectedSubsystem = SUB_SNC;
            systemStatus.nextExpectedIST = 0;
            systemStatus.nextExpectedDescription = "Touch Detection (to start calibration)";
            break;
            
        case SYS_CAL:
            if (!systemStatus.waitingForSecondTouch) {
                systemStatus.nextExpectedSystemState = SYS_CAL;
                systemStatus.nextExpectedSubsystem = SUB_SS;
                systemStatus.nextExpectedIST = 0;
                systemStatus.nextExpectedDescription = "SS End of Calibration (initial)";
            } else {
                systemStatus.nextExpectedSystemState = SYS_CAL;
                systemStatus.nextExpectedSubsystem = SUB_SNC;
                systemStatus.nextExpectedIST = 0;
                systemStatus.nextExpectedDescription = "Touch Detection (2nd touch to enter MAZE)";
            }
            break;
            
        case SYS_MAZE:
            systemStatus.nextExpectedSystemState = SYS_MAZE;
            systemStatus.nextExpectedSubsystem = SUB_SNC;
            systemStatus.nextExpectedIST = 1;
            systemStatus.nextExpectedDescription = "Pure Tone Detection";
            break;
            
        case SYS_SOS:
            systemStatus.nextExpectedSystemState = SYS_SOS;
            systemStatus.nextExpectedSubsystem = SUB_SNC;
            systemStatus.nextExpectedIST = 0;
            systemStatus.nextExpectedDescription = "Pure Tone Detection (to return to MAZE)";
            break;
    }
}

void updateNextExpectedBasedOnLastPacket(const SCSPacket& lastPacket) {
    SystemState packetSysState = getSystemState(lastPacket.control);
    SubsystemID packetSubsystem = getSubsystemID(lastPacket.control);
    uint8_t packetIST = getInternalState(lastPacket.control);
    
    Serial.printf("🔍 Updating next expected based on: [%s:%s:IST%d]\n",
                  systemStateToString(packetSysState),
                  subsystemToString(packetSubsystem),
                  packetIST);
    
    // IDLE State Logic
    if (packetSysState == SYS_IDLE && packetSubsystem == SUB_SNC && packetIST == 0) {
        if (lastPacket.dat1 == 1) {
            systemStatus.nextExpectedSystemState = SYS_CAL;
            systemStatus.nextExpectedSubsystem = SUB_SS;
            systemStatus.nextExpectedIST = 0;
            systemStatus.nextExpectedDescription = "SS End of Calibration";
        } else {
            systemStatus.nextExpectedSystemState = SYS_IDLE;
            systemStatus.nextExpectedSubsystem = SUB_SNC;
            systemStatus.nextExpectedIST = 0;
            systemStatus.nextExpectedDescription = "Touch Detection (to start calibration)";
        }
    }
    // CAL State Logic
    else if (packetSysState == SYS_CAL) {
        if (packetSubsystem == SUB_SS && packetIST == 0) {
            systemStatus.nextExpectedSystemState = SYS_CAL;
            systemStatus.nextExpectedSubsystem = SUB_MDPS;
            systemStatus.nextExpectedIST = 0;
            systemStatus.nextExpectedDescription = "MDPS vop Calibration";
            systemStatus.waitingForSecondTouch = false;
        }
        else if (packetSubsystem == SUB_MDPS && packetIST == 0) {
            systemStatus.nextExpectedSystemState = SYS_CAL;
            systemStatus.nextExpectedSubsystem = SUB_MDPS;
            systemStatus.nextExpectedIST = 1;
            systemStatus.nextExpectedDescription = "MDPS Battery Level";
        }
        else if (packetSubsystem == SUB_MDPS && packetIST == 1) {
            systemStatus.nextExpectedSystemState = SYS_CAL;
            systemStatus.nextExpectedSubsystem = SUB_SS;
            systemStatus.nextExpectedIST = 1;
            systemStatus.nextExpectedDescription = "SS Colors (CAL)";
            systemStatus.waitingForSecondTouch = true;
        }
        else if (packetSubsystem == SUB_SS && packetIST == 1) {
            systemStatus.nextExpectedSystemState = SYS_CAL;
            systemStatus.nextExpectedSubsystem = SUB_SNC;
            systemStatus.nextExpectedIST = 0;
            systemStatus.nextExpectedDescription = "Touch Detection (2nd touch to enter MAZE)";
        }
        else if (packetSubsystem == SUB_SNC && packetIST == 0) {
            if (lastPacket.dat1 == 1) {
                systemStatus.nextExpectedSystemState = SYS_MAZE;
                systemStatus.nextExpectedSubsystem = SUB_SNC;
                systemStatus.nextExpectedIST = 1;
                systemStatus.nextExpectedDescription = "Pure Tone Detection (MAZE)";
            } else {
                systemStatus.nextExpectedSystemState = SYS_CAL;
                systemStatus.nextExpectedSubsystem = SUB_MDPS;
                systemStatus.nextExpectedIST = 1;
                systemStatus.nextExpectedDescription = "MDPS Battery Level (loop)";
            }
        }
    }
    // MAZE State Logic  
    else if (packetSysState == SYS_MAZE) {
        if (packetSubsystem == SUB_SNC && packetIST == 1) {
            if (lastPacket.dat1 == 1) {
                // *** PURE TONE DETECTED - Set flag and expect MDPS SOS response ***
                systemStatus.justSentPureToneDetection = true;
                systemStatus.nextExpectedSystemState = SYS_SOS;
                systemStatus.nextExpectedSubsystem = SUB_MDPS;
                systemStatus.nextExpectedIST = 4;
                systemStatus.nextExpectedDescription = "MDPS Pure Tone Response (stop motors)";
                Serial.println("🔧 PURE TONE FLAG SET: Next MDPS IST4 will be SOS response");
            } else {
                systemStatus.justSentPureToneDetection = false;
                systemStatus.nextExpectedSystemState = SYS_MAZE;
                systemStatus.nextExpectedSubsystem = SUB_SNC;
                systemStatus.nextExpectedIST = 2;
                systemStatus.nextExpectedDescription = "Touch Detection (MAZE)";
            }
        }
        else if (packetSubsystem == SUB_SNC && packetIST == 2) {
            systemStatus.justSentPureToneDetection = false;
            if (lastPacket.dat1 == 1) {
                systemStatus.nextExpectedSystemState = SYS_IDLE;
                systemStatus.nextExpectedSubsystem = SUB_SNC;
                systemStatus.nextExpectedIST = 0;
                systemStatus.nextExpectedDescription = "Touch Detection (IDLE after manual exit)";
            } else {
                systemStatus.nextExpectedSystemState = SYS_MAZE;
                systemStatus.nextExpectedSubsystem = SUB_SNC;
                systemStatus.nextExpectedIST = 3;
                systemStatus.nextExpectedDescription = "Navigation Control (NAVCON)";
            }
        }
        else if (packetSubsystem == SUB_SNC && packetIST == 3) {
            systemStatus.justSentPureToneDetection = false;
            systemStatus.nextExpectedSystemState = SYS_MAZE;
            systemStatus.nextExpectedSubsystem = SUB_MDPS;
            systemStatus.nextExpectedIST = 1;
            systemStatus.nextExpectedDescription = "MDPS Battery/Level (MAZE)";
        }
        else if (packetSubsystem == SUB_MDPS && packetIST == 1) {
            systemStatus.justSentPureToneDetection = false;
            systemStatus.nextExpectedSystemState = SYS_MAZE;
            systemStatus.nextExpectedSubsystem = SUB_MDPS;
            systemStatus.nextExpectedIST = 2;
            systemStatus.nextExpectedDescription = "MDPS Rotation (MAZE)";
        }
        else if (packetSubsystem == SUB_MDPS && packetIST == 2) {
            systemStatus.justSentPureToneDetection = false;
            systemStatus.nextExpectedSystemState = SYS_MAZE;
            systemStatus.nextExpectedSubsystem = SUB_MDPS;
            systemStatus.nextExpectedIST = 3;
            systemStatus.nextExpectedDescription = "MDPS Speed (MAZE)";
        }
        else if (packetSubsystem == SUB_MDPS && packetIST == 3) {
            systemStatus.justSentPureToneDetection = false;
            systemStatus.nextExpectedSystemState = SYS_MAZE;
            systemStatus.nextExpectedSubsystem = SUB_MDPS;
            systemStatus.nextExpectedIST = 4;
            systemStatus.nextExpectedDescription = "MDPS Distance (MAZE)";
        }
        else if (packetSubsystem == SUB_MDPS && packetIST == 4) {
            // *** CRITICAL FIX: Check if this is a SOS response or regular MAZE distance ***
            if (systemStatus.justSentPureToneDetection) {
                // This is a SOS Pure Tone Response, not a regular MAZE distance
                systemStatus.justSentPureToneDetection = false; // Clear the flag
                systemStatus.nextExpectedSystemState = SYS_SOS;
                systemStatus.nextExpectedSubsystem = SUB_SNC;
                systemStatus.nextExpectedIST = 0;
                systemStatus.nextExpectedDescription = "Pure Tone Detection (to exit SOS)";
                Serial.println("🔧 FIXED: MDPS IST4 was SOS response, now expecting SNC pure tone in SOS");
            } else {
                // This is a regular MAZE distance packet
                systemStatus.nextExpectedSystemState = SYS_MAZE;
                systemStatus.nextExpectedSubsystem = SUB_SS;
                systemStatus.nextExpectedIST = 1;
                systemStatus.nextExpectedDescription = "SS Colors (MAZE) or SS End-of-Maze";
                Serial.println("🔧 Regular MAZE distance packet, expecting SS colors");
            }
        }
        else if (packetSubsystem == SUB_SS && packetIST == 1) {
            systemStatus.justSentPureToneDetection = false;
            systemStatus.nextExpectedSystemState = SYS_MAZE;
            systemStatus.nextExpectedSubsystem = SUB_SS;
            systemStatus.nextExpectedIST = 2;
            systemStatus.nextExpectedDescription = "SS Incidence Angle";
        }
        else if (packetSubsystem == SUB_SS && packetIST == 2) {
            systemStatus.justSentPureToneDetection = false;
            systemStatus.nextExpectedSystemState = SYS_MAZE;
            systemStatus.nextExpectedSubsystem = SUB_SNC;
            systemStatus.nextExpectedIST = 1;
            systemStatus.nextExpectedDescription = "Pure Tone Detection (loop)";
        }
        else if (packetSubsystem == SUB_SS && packetIST == 3) {
            systemStatus.justSentPureToneDetection = false;
            systemStatus.nextExpectedSystemState = SYS_IDLE;
            systemStatus.nextExpectedSubsystem = SUB_SNC;
            systemStatus.nextExpectedIST = 0;
            systemStatus.nextExpectedDescription = "Touch Detection (IDLE after maze completion)";
        }
    }
    // SOS State Logic
    else if (packetSysState == SYS_SOS) {
        if (packetSubsystem == SUB_MDPS && packetIST == 4) {
            // After MDPS stops the motors, wait for SNC pure tone detection
            systemStatus.nextExpectedSystemState = SYS_SOS;
            systemStatus.nextExpectedSubsystem = SUB_SNC;
            systemStatus.nextExpectedIST = 0;
            systemStatus.nextExpectedDescription = "Pure Tone Detection (to exit SOS)";
        }
        else if (packetSubsystem == SUB_SNC && packetIST == 0) {
            if (lastPacket.dat1 == 1) {
                // Pure tone detected - go back to MAZE and start with pure tone detection
                systemStatus.nextExpectedSystemState = SYS_MAZE;
                systemStatus.nextExpectedSubsystem = SUB_SNC;
                systemStatus.nextExpectedIST = 1;
                systemStatus.nextExpectedDescription = "Pure Tone Detection (MAZE after SOS exit)";
            } else {
                // No pure tone - stay in SOS waiting for pure tone
                systemStatus.nextExpectedSystemState = SYS_SOS;
                systemStatus.nextExpectedSubsystem = SUB_SNC;
                systemStatus.nextExpectedIST = 0;
                systemStatus.nextExpectedDescription = "Pure Tone Detection (continue waiting in SOS)";
            }
        }
    }
    
    Serial.printf("🎯 Next expected: [%s:%s:IST%d] - %s\n",
                  systemStateToString(systemStatus.nextExpectedSystemState),
                  subsystemToString(systemStatus.nextExpectedSubsystem),
                  systemStatus.nextExpectedIST,
                  systemStatus.nextExpectedDescription.c_str());
}
// ==================== STATE TRANSITION LOGIC ====================
void processStateTransition(const SCSPacket& packet) {
    SystemState packetSysState = getSystemState(packet.control);
    SubsystemID packetSubsystem = getSubsystemID(packet.control);
    uint8_t packetIST = getInternalState(packet.control);
    
    Serial.printf("🔄 Processing packet for state transition: [%s:%s:IST%d]\n",
                  systemStateToString(packetSysState),
                  subsystemToString(packetSubsystem),
                  packetIST);
    
    // Update expectations first
    updateNextExpectedBasedOnLastPacket(packet);
    
    // Check for actual state transitions
    if (packetSubsystem == SUB_SNC && packetSysState == SYS_IDLE && packetIST == 0) {
        if (packet.dat1 == 1) {
            systemStatus.currentSystemState = SYS_CAL;
            systemStatus.waitingForSecondTouch = false;
            systemStatus.lastTransitionTime = millis();
            Serial.println("🔄 STATE TRANSITION: IDLE → CAL (First touch detected)");
        }
    }
    else if (packetSubsystem == SUB_SNC && packetSysState == SYS_CAL && packetIST == 0) {
        if (packet.dat1 == 1) {
            systemStatus.currentSystemState = SYS_MAZE;
            systemStatus.lastTransitionTime = millis();
            Serial.println("🔄 STATE TRANSITION: CAL → MAZE (Second touch detected)");
        }
    }
    else if (packetSubsystem == SUB_SNC && packetSysState == SYS_MAZE && packetIST == 1) {
        if (packet.dat1 == 1) {
            systemStatus.currentSystemState = SYS_SOS;
            systemStatus.lastTransitionTime = millis();
            Serial.println("🔄 STATE TRANSITION: MAZE → SOS (Pure tone detected)");
        }
    }
    // *** FIX: Add the missing MAZE → IDLE transition for touch detection ***
    else if (packetSubsystem == SUB_SNC && packetSysState == SYS_MAZE && packetIST == 2) {
        if (packet.dat1 == 1) {
            systemStatus.currentSystemState = SYS_IDLE;
            systemStatus.lastTransitionTime = millis();
            Serial.println("🔄 STATE TRANSITION: MAZE → IDLE (Touch detected in MAZE)");
        }
    }
    else if (packetSubsystem == SUB_SNC && packetSysState == SYS_SOS && packetIST == 0) {
        if (packet.dat1 == 1) {
            systemStatus.currentSystemState = SYS_MAZE;
            systemStatus.lastTransitionTime = millis();
            Serial.println("🔄 STATE TRANSITION: SOS → MAZE (Pure tone detected)");
        }
    }
    else if (packetSubsystem == SUB_SS && packetSysState == SYS_MAZE && packetIST == 3) {
        systemStatus.currentSystemState = SYS_IDLE;
        systemStatus.lastTransitionTime = millis();
        Serial.println("🔄 STATE TRANSITION: MAZE → IDLE (End of maze detected)");
    }
    // *** NEW FIX: Handle SS Incidence Angle - NO state transition ***
    else if (packetSubsystem == SUB_SS && packetSysState == SYS_MAZE && packetIST == 2) {
        // SS Incidence Angle received - stay in MAZE state, no transition
        Serial.println("🔄 SS Incidence Angle received - staying in MAZE state (no transition)");
        // Do not change systemStatus.currentSystemState - stay in MAZE
    }
    // *** ADDITIONAL FIX: Handle SS Colors (MAZE) - NO state transition ***
    else if (packetSubsystem == SUB_SS && packetSysState == SYS_MAZE && packetIST == 1) {
        // SS Colors (MAZE) received - stay in MAZE state, no transition
        Serial.println("🔄 SS Colors (MAZE) received - staying in MAZE state (no transition)");
        // Do not change systemStatus.currentSystemState - stay in MAZE
    }
    
    // IMPORTANT: No other packets should cause state transitions!
    // MDPS packets within MAZE state should NOT change the system state
    // Only the packets listed above cause actual state transitions
    
    Serial.printf("🎯 Current system state: %s\n", systemStateToString(systemStatus.currentSystemState));
}

// ==================== GPIO COMMAND FUNCTIONS ====================
void setupGPIOCommands() {
    Serial.println("🔥 Setting up GPIO command inputs...");
    
    pinMode(CMD_TOUCH_PIN, INPUT_PULLDOWN);
    pinMode(CMD_TONE_PIN, INPUT_PULLDOWN);
    pinMode(CMD_SEND_PIN, INPUT_PULLDOWN);
    
    Serial.println("🔥 GPIO command pins initialized:");
    Serial.println("🔥   Touch Command = GPIO 4 (Input)");
    Serial.println("🔥   Pure Tone     = GPIO 2 (Input)");
    Serial.println("🔥   Send Packet   = GPIO 15 (Input)");
}

bool checkWiFiCommands() {
    bool commandReceived = false;
    
    // Debug GPIO status every 10 seconds
    static unsigned long lastDebug = 0;
    if (millis() - lastDebug > 10000) {
        Serial.printf("🔥 GPIO Status: Touch(4)=%d, Tone(2)=%d, Send(15)=%d\n", 
                     digitalRead(CMD_TOUCH_PIN), 
                     digitalRead(CMD_TONE_PIN), 
                     digitalRead(CMD_SEND_PIN));
        lastDebug = millis();
    }
    
    // Check for touch command (GPIO 4)
    if (digitalRead(CMD_TOUCH_PIN) == HIGH) {
        systemStatus.touchDetected = true;
        systemStatus.gpioCommandCount++;
        Serial.println("🔥 ✅ TOUCH command received via GPIO 4!");
        commandReceived = true;
        
        while (digitalRead(CMD_TOUCH_PIN) == HIGH) {
            delay(1);
        }
        Serial.println("🔥 Touch pulse completed");
    }
    
    // Check for pure tone command (GPIO 2)
    if (digitalRead(CMD_TONE_PIN) == HIGH) {
        systemStatus.pureToneDetected = true;
        systemStatus.gpioCommandCount++;
        Serial.println("🔥 ✅ PURE TONE command received via GPIO 2!");
        commandReceived = true;
        
        while (digitalRead(CMD_TONE_PIN) == HIGH) {
            delay(1);
        }
        Serial.println("🔥 Pure tone pulse completed");
    }
    
    // Check for send packet command (GPIO 15)
    if (digitalRead(CMD_SEND_PIN) == HIGH) {
        systemStatus.manualSendTrigger = true;
        systemStatus.gpioCommandCount++;
        Serial.println("🔥 ✅ SEND PACKET command received via GPIO 15!");
        commandReceived = true;
        
        while (digitalRead(CMD_SEND_PIN) == HIGH) {
            delay(1);
        }
        Serial.println("🔥 Send packet pulse completed");
    }
    
    return commandReceived;
}

// ==================== SNC PACKET GENERATION ====================
SCSPacket generateSNCPacket() {
    SCSPacket packet;
    
    switch (systemStatus.currentSystemState) {
        case SYS_IDLE:
            packet.control = createControlByte(SYS_IDLE, SUB_SNC, 0);
            packet.dat1 = systemStatus.touchDetected ? 1 : 0;
            packet.dat0 = 50;
            packet.dec = 0;
            if (systemStatus.touchDetected) {
                systemStatus.touchDetected = false;
            }
            break;
            
        case SYS_CAL:
            packet.control = createControlByte(SYS_CAL, SUB_SNC, 0);
            packet.dat1 = systemStatus.touchDetected ? 1 : 0;
            packet.dat0 = 0;
            packet.dec = 0;
            if (systemStatus.touchDetected) {
                systemStatus.touchDetected = false;
            }
            break;
            
        case SYS_MAZE:
            if (systemStatus.nextExpectedSubsystem == SUB_SNC && systemStatus.nextExpectedIST == 1) {
                packet.control = createControlByte(SYS_MAZE, SUB_SNC, 1);
                packet.dat1 = systemStatus.pureToneDetected ? 1 : 0;
                packet.dat0 = 0;
                packet.dec = 0;
                // *** Set the flag when sending pure tone detection ***
                if (systemStatus.pureToneDetected) {
                    systemStatus.justSentPureToneDetection = true;
                    systemStatus.pureToneDetected = false;
                    Serial.println("🔧 SNC: Setting pure tone flag - next MDPS IST4 will be SOS response");
                }
            }
            else if (systemStatus.nextExpectedSubsystem == SUB_SNC && systemStatus.nextExpectedIST == 2) {
                packet.control = createControlByte(SYS_MAZE, SUB_SNC, 2);
                packet.dat1 = systemStatus.touchDetected ? 1 : 0;
                packet.dat0 = 0;
                packet.dec = 0;
                if (systemStatus.touchDetected) {
                    systemStatus.touchDetected = false;
                }
            }
            else if (systemStatus.nextExpectedSubsystem == SUB_SNC && systemStatus.nextExpectedIST == 3) {
                packet.control = createControlByte(SYS_MAZE, SUB_SNC, 3);
                packet.dat1 = 50;
                packet.dat0 = 50;
                packet.dec = 0;
            }
            else {
                packet.control = createControlByte(SYS_MAZE, SUB_SNC, 1);
                packet.dat1 = systemStatus.pureToneDetected ? 1 : 0;
                packet.dat0 = 0;
                packet.dec = 0;
                if (systemStatus.pureToneDetected) {
                    systemStatus.justSentPureToneDetection = true;
                    systemStatus.pureToneDetected = false;
                }
            }
            break;
            
        case SYS_SOS:
            packet.control = createControlByte(SYS_SOS, SUB_SNC, 0);
            packet.dat1 = systemStatus.pureToneDetected ? 1 : 0;
            packet.dat0 = 0;
            packet.dec = 0;
            if (systemStatus.pureToneDetected) {
                systemStatus.pureToneDetected = false;
            }
            break;
    }
    
    return packet;
}

// ==================== STATUS REPORTING ====================
void printSystemStatus() {
    Serial.println("\n🔥 ============================================");
    Serial.println("🔥           MARV SYSTEM STATUS");
    Serial.println("🔥 ============================================");
    
    Serial.println("🎯 *** CURRENT STATE MACHINE STATUS ***");
    Serial.printf("🎯 ║ CURRENT SYSTEM STATE: %s\n", systemStateToString(systemStatus.currentSystemState));
    Serial.printf("🎯 ║ NEXT EXPECTED SUBSYSTEM: %s\n", subsystemToString(systemStatus.nextExpectedSubsystem));
    Serial.printf("🎯 ║ NEXT EXPECTED IST: %d\n", systemStatus.nextExpectedIST);
    Serial.printf("🎯 ║ EXPECTING: %s\n", systemStatus.nextExpectedDescription.c_str());
    Serial.println("🎯 ******************************************");
    
    Serial.println("");
    Serial.println("📊 Additional Status Information:");
    Serial.printf("📊 Waiting for 2nd Touch: %s\n", systemStatus.waitingForSecondTouch ? "YES" : "NO");
    Serial.printf("📊 Unexpected Packets: %d\n", systemStatus.unexpectedPacketCount);
    Serial.printf("📊 GPIO Commands Received: %d\n", systemStatus.gpioCommandCount);
    Serial.printf("📊 Touch Ready: %s, Pure Tone Ready: %s, Send Ready: %s\n",
                 systemStatus.touchDetected ? "YES" : "NO",
                 systemStatus.pureToneDetected ? "YES" : "NO",
                 systemStatus.manualSendTrigger ? "YES" : "NO");
    Serial.printf("📊 SS Buffer: %d bytes, MDPS Buffer: %d bytes\n",
                 ssHandler.getBufferLevel(),
                 mdpsHandler.getBufferLevel());
    
    unsigned long uptime = millis();
    Serial.printf("📊 System Uptime: %lu seconds\n", uptime / 1000);
    
    if (systemStatus.lastTransitionTime > 0) {
        unsigned long timeSinceTransition = (millis() - systemStatus.lastTransitionTime) / 1000;
        Serial.printf("📊 Time since last transition: %lu seconds\n", timeSinceTransition);
    }
    
    Serial.println("🔥 ============================================\n");
}

void printCompactStatus() {
    Serial.println("\n🎯 *** QUICK STATUS ***");
    Serial.printf("🎯 STATE: %s → EXPECTING: %s:IST%d (%s)\n", 
                  systemStateToString(systemStatus.currentSystemState),
                  subsystemToString(systemStatus.nextExpectedSubsystem),
                  systemStatus.nextExpectedIST,
                  systemStatus.nextExpectedDescription.c_str());
    Serial.println("🎯 *******************\n");
}

void simulateTouch() {
    systemStatus.touchDetected = true;
    Serial.println("🔥 MANUAL: Touch detected via serial");
}

void simulatePureTone() {
    systemStatus.pureToneDetected = true;
    Serial.println("🔥 MANUAL: Pure tone detected via serial");
}

void manualSendTrigger() {
    systemStatus.manualSendTrigger = true;
    Serial.println("🔥 MANUAL: Send trigger activated via serial");
}

// ==================== ARDUINO SETUP AND LOOP ====================
void setup() {
    Serial.begin(115200);
    delay(1000);
    
    Serial.println("🔥 ========================================");
    Serial.println("🔥 MARV SNC Subsystem - PHASE 0");
    Serial.println("🔥 Main ESP32 with State Transitions");
    Serial.println("🔥 ========================================");
    
    ssHandler.begin(19200);
    mdpsHandler.begin(19200);
    Serial.println("🔥 UART handlers initialized (19200 baud)");
    
    setupGPIOCommands();
    updateNextExpectedState();
    
    Serial.println("🔥 ========================================");
    Serial.println("🔥 Commands available:");
    Serial.println("🔥   Serial: T (touch), P (pure tone), S (send), ? (status)");
    Serial.println("🔥   GPIO: Connect WiFi ESP32 to pins 13, 2, 15");
    Serial.println("🔥 ========================================");
    Serial.println("🔥 ESP32 with State Transitions ready!");
    Serial.println("🔥 ========================================\n");
    
    printSystemStatus();
}

void loop() {
    checkWiFiCommands();
    
    SCSPacket packet;
    if (ssHandler.readPacket(packet)) {
        printPacket(packet, "📥 RX SS:");
        systemStatus.lastSSPacket = "Received SS packet";
        processStateTransition(packet);
        mdpsHandler.sendPacket(packet);
        Serial.println("📤 Forwarded SS packet to MDPS");
    }
    
    if (mdpsHandler.readPacket(packet)) {
        printPacket(packet, "📥 RX MDPS:");
        systemStatus.lastMDPSPacket = "Received MDPS packet";
        processStateTransition(packet);
        ssHandler.sendPacket(packet);
        Serial.println("📤 Forwarded MDPS packet to SS");
    }
    
    // Auto-send SNC packet when it's expected next
if (systemStatus.nextExpectedSubsystem == SUB_SNC) {
    static unsigned long lastAutoSend = 0;
    static bool idleSentOnce = false;  // Track if we sent in IDLE already
    unsigned long now = millis();
    
    // In IDLE: Only send once, then wait for touch
    if (systemStatus.currentSystemState == SYS_IDLE) {
        if (!idleSentOnce || systemStatus.touchDetected) {
            // Send if: first time in IDLE OR touch detected
            SCSPacket sncPacket = generateSNCPacket();
            
            if (sncPacket.control != 0) {
                printPacket(sncPacket, "📤 TX SNC (AUTO):");
                systemStatus.lastSNCPacket = "Sent SNC packet (auto)";
                processStateTransition(sncPacket);
                ssHandler.sendPacket(sncPacket);
                mdpsHandler.sendPacket(sncPacket);
                Serial.println("🔥 ✅ Auto SNC packet sent");
                
                idleSentOnce = true;  // Mark that we sent in IDLE
                if (systemStatus.touchDetected) {
                    idleSentOnce = false;  // Reset for next IDLE cycle
                }
            }
        }
    } 
    else {
        // Other states: Send with rate limiting
        idleSentOnce = false;  // Reset when leaving IDLE
        
        unsigned long interval = 500;  // Normal interval for other states
        if (now - lastAutoSend >= interval) {
            SCSPacket sncPacket = generateSNCPacket();
            
            if (sncPacket.control != 0) {
                printPacket(sncPacket, "📤 TX SNC (AUTO):");
                systemStatus.lastSNCPacket = "Sent SNC packet (auto)";
                processStateTransition(sncPacket);
                ssHandler.sendPacket(sncPacket);
                mdpsHandler.sendPacket(sncPacket);
                Serial.println("🔥 ✅ Auto SNC packet sent");
                lastAutoSend = now;
            }
        }
    }
}
    
    if (Serial.available()) {
        char cmd = Serial.read();
        switch (cmd) {
            case 't': case 'T':
                simulateTouch();
                break;
            case 'p': case 'P':
                simulatePureTone();
                break;
            case 's': case 'S':
                manualSendTrigger();
                break;
            case '?':
                printSystemStatus();
                break;
        }
    }
    
    static unsigned long lastStatusUpdate = 0;
    static unsigned long lastCompactUpdate = 0;
    
    if (millis() - lastCompactUpdate > 10000) {
        printCompactStatus();
        lastCompactUpdate = millis();
    }
    
    if (millis() - lastStatusUpdate > 30000) {
        printSystemStatus();
        lastStatusUpdate = millis();
    }
    
    delay(10);
}