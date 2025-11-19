// MARV A-Maze-Eng - CORRECTED MDPS (Motor Driver and Power Supply) Phase 0
// Fixed sequential button press logic for proper MDPS packet flow
// Each packet in sequence requires a separate button press

#include <Arduino.h>

// ==================== PIN DEFINITIONS ====================
// UART pins for subsystem communication
#define RX_PIN 16            // UART2 RX from SNC
#define TX_PIN 17            // UART2 TX to SNC
#define BUTTON_PIN 2         // GPIO 2 for triggering SEND
#define DEBUG_BAUD 115200
#define SCS_BAUD 19200

// LED pins for visual feedback
#define BLUE_LED_PIN  21     // Blue LED -> CAL state
#define GREEN_LED_PIN 22     // Green LED -> MAZE state
#define RED_LED_PIN   23     // Red LED -> SOS state

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
                Serial.printf("🔵 ⚠️  UART timeout - clearing buffer (had %d bytes)\n", bufferIndex);
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
                    
                    // Enhanced validation
                    if (validatePacket(testPacket)) {
                        packet = testPacket;
                        int remainingBytes = bufferIndex - (start + PACKET_SIZE);
                        if (remainingBytes > 0) {
                            memmove(buffer, buffer + start + PACKET_SIZE, remainingBytes);
                        }
                        bufferIndex = remainingBytes;
                        synced = true;
                        return true;
                    } else {
                        Serial.printf("🔵 ❌ Invalid packet rejected: [0x%02X 0x%02X 0x%02X 0x%02X]\n",
                                    testPacket.control, testPacket.dat1, testPacket.dat0, testPacket.dec);
                    }
                }
                
                if (bufferIndex >= BUFFER_SIZE) {
                    Serial.println("🔵 ⚠️  Buffer full - shifting data");
                    memmove(buffer, buffer + 1, BUFFER_SIZE - 1);
                    bufferIndex = BUFFER_SIZE - 1;
                }
            }
        }
        
        return false;
    }
    
    // Enhanced packet validation
    bool validatePacket(const SCSPacket& packet) {
        SystemState sys = getSystemState(packet.control);
        SubsystemID sub = getSubsystemID(packet.control);
        uint8_t ist = getInternalState(packet.control);
        
        // Basic range checks
        if (sys > SYS_SOS) {
            Serial.printf("🔵 ❌ Invalid SYS: %d (max %d)\n", sys, SYS_SOS);
            return false;
        }
        if (sub > SUB_SS) {
            Serial.printf("🔵 ❌ Invalid SUB: %d (max %d)\n", sub, SUB_SS);
            return false;
        }
        if (ist > 15) {
            Serial.printf("🔵 ❌ Invalid IST: %d (max 15)\n", ist);
            return false;
        }
        
        // Additional validation for known invalid combinations
        if (packet.control == 0x00 && packet.dat1 == 0x00 && 
            packet.dat0 == 0x00 && packet.dec == 0x00) {
            Serial.println("🔵 ❌ All-zero packet rejected");
            return false;
        }
        
        if (packet.control == 0xFF) {
            Serial.println("🔵 ❌ All-ones control byte rejected");
            return false;
        }
        
        return true;
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
struct MDPSStatus {
    SystemState currentSystemState;
    uint8_t lastControlByte;
    bool mdpsReadyToSend;        // MDPS has a packet ready and is waiting for button press
    bool buttonPressed;
    unsigned long lastPacketTime;
    unsigned long lastTransmissionTime;
    int packetCount;
    int transmissionCount;
    
    // MDPS Internal State Sequence Tracking
    uint8_t nextMDPSIST;         // What IST should MDPS send next?
    String nextPacketDescription; // What packet is ready to send?
    
    // Motor simulation data
    uint8_t rightWheelSpeed;  // mm/s
    uint8_t leftWheelSpeed;   // mm/s
    uint16_t lastRotationAngle; // degrees
    uint16_t distanceTraveled;  // mm since last stop
    uint8_t batteryLevel;     // Not used (set to 0)
    bool isCalibrated;
    
    // Interrupt handling
    volatile bool interruptFlag;
    volatile unsigned long lastInterruptTime;
    
    // Packet validation
    int invalidPacketCount;
    int sequenceErrorCount;
};

MDPSStatus mdpsStatus = {
    SYS_IDLE,        // currentSystemState
    0x00,            // lastControlByte
    false,           // mdpsReadyToSend
    false,           // buttonPressed
    0,               // lastPacketTime
    0,               // lastTransmissionTime
    0,               // packetCount
    0,               // transmissionCount
    0,               // nextMDPSIST
    "",              // nextPacketDescription
    50,              // rightWheelSpeed (default vop)
    50,              // leftWheelSpeed (default vop)
    0,               // lastRotationAngle
    0,               // distanceTraveled
    0,               // batteryLevel (not used)
    false,           // isCalibrated
    false,           // interruptFlag
    0,               // lastInterruptTime
    0,               // invalidPacketCount
    0                // sequenceErrorCount
};

// ==================== LED CONTROL ====================
void updateLEDs(SystemState state) {
    digitalWrite(BLUE_LED_PIN, LOW);
    digitalWrite(GREEN_LED_PIN, LOW);
    digitalWrite(RED_LED_PIN, LOW);
    
    switch (state) {
        case SYS_IDLE:
            // White (all LEDs on)
            digitalWrite(BLUE_LED_PIN, HIGH);
            digitalWrite(GREEN_LED_PIN, HIGH);
            digitalWrite(RED_LED_PIN, HIGH);
            break;
        case SYS_CAL:
            // Blue LED
            digitalWrite(BLUE_LED_PIN, HIGH);
            break;
        case SYS_MAZE:
            // Green LED
            digitalWrite(GREEN_LED_PIN, HIGH);
            break;
        case SYS_SOS:
            // Red LED
            digitalWrite(RED_LED_PIN, HIGH);
            break;
    }
}

// ==================== INTERRUPT HANDLING ====================
void IRAM_ATTR handleInterrupt() {
    unsigned long currentTime = millis();
    if (currentTime - mdpsStatus.lastInterruptTime > 200) { // Debounce
        mdpsStatus.interruptFlag = true;
        mdpsStatus.lastInterruptTime = currentTime;
    }
}

void checkButton() {
    if (mdpsStatus.interruptFlag) {
        mdpsStatus.buttonPressed = true;
        mdpsStatus.interruptFlag = false;
        Serial.println("🔵 🔘 BUTTON PRESSED! 🔘");
    }
}

// ==================== MDPS TRIGGER LOGIC ====================
void prepareNextMDPSPacket(const SCSPacket& triggeringPacket) {
    SystemState packetSysState = getSystemState(triggeringPacket.control);
    SubsystemID packetSubsystem = getSubsystemID(triggeringPacket.control);
    uint8_t packetIST = getInternalState(triggeringPacket.control);
    
    Serial.printf("🔍 Analyzing trigger packet: [%s:%s:IST%d]\n",
                  systemStateToString(packetSysState),
                  subsystemToString(packetSubsystem),
                  packetIST);
    
    // Reset ready flag
    mdpsStatus.mdpsReadyToSend = false;
    mdpsStatus.nextMDPSIST = 0;
    mdpsStatus.nextPacketDescription = "";
    
    // CAL State Logic
    if (packetSysState == SYS_CAL && packetSubsystem == SUB_SS && packetIST == 0) {
        // SS End of Calibration triggers MDPS vop Calibration
        mdpsStatus.mdpsReadyToSend = true;
        mdpsStatus.nextMDPSIST = 0;
        mdpsStatus.nextPacketDescription = "vop Calibration";
        Serial.println("✅ MDPS ready to send: vop Calibration (CAL:MDPS:IST0)");
    }
    else if (packetSysState == SYS_CAL && packetSubsystem == SUB_SNC && packetIST == 0) {
        // SNC touch check (no touch) triggers MDPS Battery Level
        if (triggeringPacket.dat1 == 0) {  // No touch detected
            mdpsStatus.mdpsReadyToSend = true;
            mdpsStatus.nextMDPSIST = 1;
            mdpsStatus.nextPacketDescription = "Battery Level (CAL)";
            Serial.println("✅ MDPS ready to send: Battery Level (CAL:MDPS:IST1)");
        }
    }
    // MAZE State Logic
    else if (packetSysState == SYS_MAZE && packetSubsystem == SUB_SNC && packetIST == 3) {
        // SNC NAVCON triggers MDPS Battery Level (start of 4-packet sequence)
        mdpsStatus.mdpsReadyToSend = true;
        mdpsStatus.nextMDPSIST = 1;
        mdpsStatus.nextPacketDescription = "Battery Level (MAZE start)";
        Serial.println("✅ MDPS ready to send: Battery Level (MAZE:MDPS:IST1)");
    }
    else if (packetSysState == SYS_MAZE && packetSubsystem == SUB_SNC && packetIST == 1) {
        // SNC Pure Tone Detection - check if tone was detected
        if (triggeringPacket.dat1 == 1) {  // Pure tone detected
            mdpsStatus.mdpsReadyToSend = true;
            mdpsStatus.nextMDPSIST = 4;
            mdpsStatus.nextPacketDescription = "Pure Tone Response (STOP)";
            Serial.println("✅ MDPS ready to send: Pure Tone Response (SOS:MDPS:IST4)");
        }
    }
    // SOS State Logic
    else if (packetSysState == SYS_SOS && packetSubsystem == SUB_SNC && packetIST == 0) {
        // SNC Pure Tone Detection in SOS - check if we should continue or exit SOS
        if (triggeringPacket.dat1 == 0) {  // No pure tone - stay in SOS
            mdpsStatus.mdpsReadyToSend = true;
            mdpsStatus.nextMDPSIST = 4;
            mdpsStatus.nextPacketDescription = "Pure Tone Response (continue SOS)";
            Serial.println("✅ MDPS ready to send: Pure Tone Response (SOS:MDPS:IST4)");
        }
    }
    
    if (mdpsStatus.mdpsReadyToSend) {
        Serial.println("🔵 ⚡⚡⚡ MDPS PACKET READY - PRESS BUTTON TO SEND! ⚡⚡⚡");
        Serial.printf("🔵 Next packet: %s\n", mdpsStatus.nextPacketDescription.c_str());
    } else {
        Serial.println("🔵 ✅ MDPS not expected to respond to this packet");
    }
}

// Handle MDPS sequence continuation (when MDPS just sent a packet and should prepare the next one)
void checkMDPSSequenceContinuation() {
    if (!mdpsStatus.mdpsReadyToSend) {
        Serial.printf("🔄 Checking if MDPS sequence should continue after IST%d...\n", mdpsStatus.nextMDPSIST);
        
        if (mdpsStatus.currentSystemState == SYS_CAL && mdpsStatus.nextMDPSIST == 0) {
            // Just sent vop calibration, now prepare battery level
            mdpsStatus.mdpsReadyToSend = true;
            mdpsStatus.nextMDPSIST = 1;
            mdpsStatus.nextPacketDescription = "Battery Level (after vop)";
            Serial.println("🔄 MDPS sequence continues: Battery Level (CAL:MDPS:IST1)");
            Serial.println("🔵 ⚡⚡⚡ NEXT PACKET READY - PRESS BUTTON TO SEND! ⚡⚡⚡");
        }
        else if (mdpsStatus.currentSystemState == SYS_MAZE) {
            if (mdpsStatus.nextMDPSIST == 1) {
                // Just sent battery level, now prepare rotation
                mdpsStatus.mdpsReadyToSend = true;
                mdpsStatus.nextMDPSIST = 2;
                mdpsStatus.nextPacketDescription = "Rotation (MAZE sequence)";
                Serial.println("🔄 MDPS sequence continues: Rotation (MAZE:MDPS:IST2)");
                Serial.println("🔵 ⚡⚡⚡ NEXT PACKET READY - PRESS BUTTON TO SEND! ⚡⚡⚡");
            }
            else if (mdpsStatus.nextMDPSIST == 2) {
                // Just sent rotation, now prepare speed
                mdpsStatus.mdpsReadyToSend = true;
                mdpsStatus.nextMDPSIST = 3;
                mdpsStatus.nextPacketDescription = "Speed (MAZE sequence)";
                Serial.println("🔄 MDPS sequence continues: Speed (MAZE:MDPS:IST3)");
                Serial.println("🔵 ⚡⚡⚡ NEXT PACKET READY - PRESS BUTTON TO SEND! ⚡⚡⚡");
            }
            else if (mdpsStatus.nextMDPSIST == 3) {
                // Just sent speed, now prepare distance (final packet)
                mdpsStatus.mdpsReadyToSend = true;
                mdpsStatus.nextMDPSIST = 4;
                mdpsStatus.nextPacketDescription = "Distance (MAZE sequence final)";
                Serial.println("🔄 MDPS sequence continues: Distance (MAZE:MDPS:IST4)");
                Serial.println("🔵 ⚡⚡⚡ FINAL PACKET READY - PRESS BUTTON TO SEND! ⚡⚡⚡");
            }
        }
    }
}

// ==================== MOTOR CONTROL SIMULATION ====================
void performCalibration() {
    Serial.println("🔵 🔧 Performing vop calibration...");
    delay(100); // Simulate calibration time
    
    // Set calibrated speeds (simulate measurement)
    mdpsStatus.rightWheelSpeed = 52;  // Simulate slight variation
    mdpsStatus.leftWheelSpeed = 48;   // Simulate slight variation
    mdpsStatus.isCalibrated = true;
    
    Serial.printf("🔵 ✅ Calibration complete: Right=%d mm/s, Left=%d mm/s\n",
                 mdpsStatus.rightWheelSpeed, mdpsStatus.leftWheelSpeed);
}

void processNavigation(uint8_t dat1, uint8_t dat0, uint8_t dec) {
    Serial.printf("🔵 🎮 Processing navigation: DAT1=%d, DAT0=%d, DEC=%d\n", dat1, dat0, dec);
    
    if (dec == 0 || dec == 1) {
        // Speed command (forward/backward)
        mdpsStatus.rightWheelSpeed = dat1;
        mdpsStatus.leftWheelSpeed = dat0;
        
        // Simulate distance traveled
        mdpsStatus.distanceTraveled += 25; // Simulate 25mm movement
        
        Serial.printf("🔵 %s: Right=%d mm/s, Left=%d mm/s, Distance=+25mm\n",
                     (dec == 0) ? "FORWARD" : "BACKWARD",
                     mdpsStatus.rightWheelSpeed, mdpsStatus.leftWheelSpeed);
    }
    else if (dec == 2 || dec == 3) {
        // Rotation command
        uint16_t angle = (dat1 << 8) | dat0;
        mdpsStatus.lastRotationAngle = angle;
        
        Serial.printf("🔵 %s ROTATION: %d degrees\n",
                     (dec == 2) ? "LEFT (CCW)" : "RIGHT (CW)", angle);
    }
}

void stopMotors() {
    Serial.println("🔵 🛑 EMERGENCY STOP - Motors stopping...");
    mdpsStatus.rightWheelSpeed = 0;
    mdpsStatus.leftWheelSpeed = 0;
    Serial.println("🔵 ✅ Motors stopped");
}

// ==================== MDPS PACKET GENERATION ====================
SCSPacket generateMDPSPacket() {
    SCSPacket packet;
    SystemState currentSys = mdpsStatus.currentSystemState;
    uint8_t istToSend = mdpsStatus.nextMDPSIST;
    
    Serial.printf("🎯 Generating MDPS packet:\n");
    Serial.printf("    Current system state: %s\n", systemStateToString(currentSys));
    Serial.printf("    IST to send: %d (%s)\n", istToSend, mdpsStatus.nextPacketDescription.c_str());
    
    if (currentSys == SYS_CAL) {
        switch (istToSend) {
            case 0: // vop Calibration
                performCalibration();
                packet.control = createControlByte(SYS_CAL, SUB_MDPS, 0);
                packet.dat1 = mdpsStatus.rightWheelSpeed;
                packet.dat0 = mdpsStatus.leftWheelSpeed;
                packet.dec = 0;
                Serial.println("📤 Sending: vop Calibration");
                break;
                
            case 1: // Battery Level
                packet.control = createControlByte(SYS_CAL, SUB_MDPS, 1);
                packet.dat1 = 0;  // Battery sensing removed
                packet.dat0 = 0;
                packet.dec = 0;
                Serial.println("📤 Sending: Battery Level (CAL)");
                break;
                
            default:
                Serial.printf("❌ Invalid CAL IST: %d\n", istToSend);
                return packet;
        }
    }
    else if (currentSys == SYS_MAZE) {
        switch (istToSend) {
            case 1: // Battery Level
                packet.control = createControlByte(SYS_MAZE, SUB_MDPS, 1);
                packet.dat1 = 0;  // Battery sensing removed
                packet.dat0 = 0;
                packet.dec = 0;
                Serial.println("📤 Sending: Battery Level (MAZE)");
                break;
                
            case 2: // Rotation
                packet.control = createControlByte(SYS_MAZE, SUB_MDPS, 2);
                packet.dat1 = (mdpsStatus.lastRotationAngle >> 8) & 0xFF;
                packet.dat0 = mdpsStatus.lastRotationAngle & 0xFF;
                packet.dec = (mdpsStatus.lastRotationAngle > 0) ? 3 : 2; // 3=right, 2=left
                Serial.printf("📤 Sending: Rotation - %d degrees (%s)\n", 
                            mdpsStatus.lastRotationAngle,
                            (packet.dec == 3) ? "RIGHT" : "LEFT");
                break;
                
            case 3: // Speed
                packet.control = createControlByte(SYS_MAZE, SUB_MDPS, 3);
                packet.dat1 = mdpsStatus.rightWheelSpeed;
                packet.dat0 = mdpsStatus.leftWheelSpeed;
                packet.dec = 0;
                Serial.printf("📤 Sending: Speed - Right:%d mm/s, Left:%d mm/s\n",
                            mdpsStatus.rightWheelSpeed, mdpsStatus.leftWheelSpeed);
                break;
                
            case 4: // Distance
                packet.control = createControlByte(SYS_MAZE, SUB_MDPS, 4);
                packet.dat1 = (mdpsStatus.distanceTraveled >> 8) & 0xFF;
                packet.dat0 = mdpsStatus.distanceTraveled & 0xFF;
                packet.dec = 0;
                Serial.printf("📤 Sending: Distance - %d mm\n", mdpsStatus.distanceTraveled);
                // Reset distance for next measurement
                mdpsStatus.distanceTraveled = 0;
                break;
                
            default:
                Serial.printf("❌ Invalid MAZE IST: %d\n", istToSend);
                return packet;
        }
    }
    else if (currentSys == SYS_SOS) {
        if (istToSend == 4) {
            // SOS state - send stop confirmation
            stopMotors();
            packet.control = createControlByte(SYS_SOS, SUB_MDPS, 4);
            packet.dat1 = 0;  // Stopped speed
            packet.dat0 = 0;
            packet.dec = 0;
            Serial.println("📤 Sending: Pure Tone Response (Motors stopped)");
        } else {
            Serial.printf("❌ Invalid SOS IST: %d\n", istToSend);
            return packet;
        }
    }
    else {
        Serial.printf("❌ Cannot generate packet for system state: %s\n", systemStateToString(currentSys));
        return packet;
    }
    
    return packet;
}

// ==================== STATUS DISPLAY ====================
void printMDPSStatus() {
    Serial.println("\n🔵 ============================================");
    Serial.println("🔵           MDPS SUBSYSTEM STATUS");
    Serial.println("🔵 ============================================");
    
    Serial.printf("🔵 Current System State: %s\n", systemStateToString(mdpsStatus.currentSystemState));
    Serial.printf("🔵 Last Control Byte: 0x%02X\n", mdpsStatus.lastControlByte);
    Serial.printf("🔵 MDPS Ready to Send: %s\n", mdpsStatus.mdpsReadyToSend ? "YES ⚡" : "NO");
    Serial.printf("🔵 Next IST to Send: %d\n", mdpsStatus.nextMDPSIST);
    Serial.printf("🔵 Next Packet: %s\n", mdpsStatus.nextPacketDescription.c_str());
    Serial.printf("🔵 Button Ready: %s\n", mdpsStatus.buttonPressed ? "YES ⚡" : "NO");
    Serial.printf("🔵 Packets Received: %d\n", mdpsStatus.packetCount);
    Serial.printf("🔵 Packets Transmitted: %d\n", mdpsStatus.transmissionCount);
    Serial.printf("🔵 Invalid Packets: %d\n", mdpsStatus.invalidPacketCount);
    Serial.printf("🔵 Calibrated: %s\n", mdpsStatus.isCalibrated ? "YES ✅" : "NO");
    
    if (mdpsStatus.lastTransmissionTime > 0) {
        unsigned long timeSinceTransmission = (millis() - mdpsStatus.lastTransmissionTime) / 1000;
        Serial.printf("🔵 Last Transmission: %lu seconds ago\n", timeSinceTransmission);
    } else {
        Serial.println("🔵 Last Transmission: NEVER");
    }
    
    Serial.println("🔵 *** MOTOR STATUS ***");
    Serial.printf("🔵 Right Wheel Speed: %d mm/s\n", mdpsStatus.rightWheelSpeed);
    Serial.printf("🔵 Left Wheel Speed: %d mm/s\n", mdpsStatus.leftWheelSpeed);
    Serial.printf("🔵 Last Rotation: %d degrees\n", mdpsStatus.lastRotationAngle);
    Serial.printf("🔵 Distance Traveled: %d mm\n", mdpsStatus.distanceTraveled);
    
    Serial.println("🔵 ============================================\n");
}

// ==================== ARDUINO SETUP AND LOOP ====================
void setup() {
    Serial.begin(DEBUG_BAUD);
    delay(1000);
    
    Serial.println("🔵 ========================================");
    Serial.println("🔵 MARV MDPS Subsystem - CORRECTED PHASE 0");
    Serial.println("🔵 Sequential Button Press Logic");
    Serial.println("🔵 ========================================");
    
    // Initialize GPIO pins
    pinMode(BUTTON_PIN, INPUT_PULLUP);
    pinMode(BLUE_LED_PIN, OUTPUT);
    pinMode(GREEN_LED_PIN, OUTPUT);
    pinMode(RED_LED_PIN, OUTPUT);
    
    // Set up interrupt
    attachInterrupt(digitalPinToInterrupt(BUTTON_PIN), handleInterrupt, FALLING);
    
    // Initialize UART
    sncHandler.begin(SCS_BAUD);
    
    // Initialize LEDs (start in IDLE = white)
    updateLEDs(SYS_IDLE);
    
    Serial.println("🔵 UART handler initialized (19200 baud)");
    Serial.println("🔵 GPIO 2 button interrupt initialized");
    Serial.println("🔵 LEDs initialized");
    
    Serial.println("🔵 ========================================");
    Serial.println("🔵 Commands available:");
    Serial.println("🔵   GPIO 2 Button: Press to send next MDPS packet");
    Serial.println("🔵   Serial: ? (status), r (reset)");
    Serial.println("🔵 ========================================");
    Serial.println("🔵 🚀 Corrected MDPS Subsystem ready!");
    Serial.println("🔵 ========================================\n");
    
    printMDPSStatus();
}

void loop() {
    // Check button interrupt
    checkButton();
    
    // Check for incoming packets
    SCSPacket packet;
    if (sncHandler.readPacket(packet)) {
        printPacket(packet, "📥 RX from SNC:");
        
        mdpsStatus.lastControlByte = packet.control;
        SystemState newSystemState = getSystemState(packet.control);
        SubsystemID packetSub = getSubsystemID(packet.control);
        uint8_t packetIST = getInternalState(packet.control);
        
        // Update system state and track transitions
        if (newSystemState != mdpsStatus.currentSystemState) {
            Serial.printf("🔄 MDPS State Transition: %s → %s\n",
                         systemStateToString(mdpsStatus.currentSystemState),
                         systemStateToString(newSystemState));
            mdpsStatus.currentSystemState = newSystemState;
            updateLEDs(mdpsStatus.currentSystemState);
        }
        
        mdpsStatus.lastPacketTime = millis();
        mdpsStatus.packetCount++;
        
        // Process navigation commands from SNC NAVCON
        if (mdpsStatus.currentSystemState == SYS_MAZE && packetSub == SUB_SNC && packetIST == 3) {
            processNavigation(packet.dat1, packet.dat0, packet.dec);
        }
        
        // Check if this packet should trigger an MDPS response
        prepareNextMDPSPacket(packet);
    }
    
    // Handle sending when button is pressed and MDPS is ready to send
    if (mdpsStatus.buttonPressed && mdpsStatus.mdpsReadyToSend) {
        SCSPacket mdpsPacket = generateMDPSPacket();
        
        if (mdpsPacket.control != 0) {
            Serial.println("🔵 ==========================================");
            Serial.println("🔵           TRANSMITTING MDPS PACKET");
            Serial.println("🔵 ==========================================");
            printPacket(mdpsPacket, "📤 TX to SNC:");
            sncHandler.sendPacket(mdpsPacket);
            Serial.println("🔵 ✅ MDPS PACKET TRANSMITTED SUCCESSFULLY! ✅");
            Serial.println("🔵 ==========================================");
            
            // Update status
            mdpsStatus.lastTransmissionTime = millis();
            mdpsStatus.transmissionCount++;
            
            // Mark as sent and check if sequence should continue
            mdpsStatus.mdpsReadyToSend = false;
            
            // Check if we should prepare the next packet in the sequence
            checkMDPSSequenceContinuation();
            
        } else {
            Serial.println("🔵 ❌ FAILED TO GENERATE MDPS PACKET ❌");
        }
        
        mdpsStatus.buttonPressed = false;
    }
    
    // Handle button press when MDPS is not ready (user feedback)
    if (mdpsStatus.buttonPressed && !mdpsStatus.mdpsReadyToSend) {
        Serial.println("🔵 ⚠️  BUTTON PRESSED BUT MDPS NOT READY TO SEND");
        Serial.println("🔵 ⚠️  Wait for a trigger packet from SNC or SS first!");
        mdpsStatus.buttonPressed = false;
    }
    
    // Handle serial commands
    if (Serial.available()) {
        char cmd = Serial.read();
        switch (cmd) {
            case '?':
                printMDPSStatus();
                break;
            case 'r': case 'R':
                // Reset MDPS status
                mdpsStatus.currentSystemState = SYS_IDLE;
                mdpsStatus.mdpsReadyToSend = false;
                mdpsStatus.nextMDPSIST = 0;
                mdpsStatus.nextPacketDescription = "";
                mdpsStatus.isCalibrated = false;
                mdpsStatus.packetCount = 0;
                mdpsStatus.transmissionCount = 0;
                mdpsStatus.invalidPacketCount = 0;
                mdpsStatus.sequenceErrorCount = 0;
                mdpsStatus.rightWheelSpeed = 50;
                mdpsStatus.leftWheelSpeed = 50;
                mdpsStatus.lastRotationAngle = 0;
                mdpsStatus.distanceTraveled = 0;
                updateLEDs(SYS_IDLE);
                Serial.println("🔵 MDPS subsystem reset");
                break;
        }
    }
    
    // Periodic status update
    static unsigned long lastStatusUpdate = 0;
    if (millis() - lastStatusUpdate > 15000) {
        Serial.printf("🔵 *** QUICK STATUS: %s | Ready: %s | Next: IST%d (%s) | RX: %d | TX: %d ***\n",
                      systemStateToString(mdpsStatus.currentSystemState),
                      mdpsStatus.mdpsReadyToSend ? "YES ⚡" : "NO",
                      mdpsStatus.nextMDPSIST,
                      mdpsStatus.nextPacketDescription.c_str(),
                      mdpsStatus.packetCount,
                      mdpsStatus.transmissionCount);
        lastStatusUpdate = millis();
    }
    
    delay(10);
}