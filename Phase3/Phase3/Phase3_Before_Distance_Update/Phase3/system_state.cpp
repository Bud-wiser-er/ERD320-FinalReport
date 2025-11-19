#include "system_state.h"
#include "navcon_core.h"

// ==================== GLOBAL SYSTEM STATUS ====================
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
    false,           // justSentPureToneDetection
    false,           // needsIdlePacket
    0,               // unexpectedPacketCount
    0,               // gpioCommandCount
    "",              // lastSSPacket
    "",              // lastMDPSPacket
    "",              // lastSNCPacket
    false            // eomLatched
};

// Static variables for auto-send logic
static unsigned long lastAutoSend = 0;
static bool idleSentOnce = false;

// ==================== SYSTEM STATE INITIALIZATION ====================
void initializeSystemState() {
    systemStatus.currentSystemState = SYS_IDLE;
    systemStatus.lastTransitionTime = 0;
    updateNextExpectedState();
    systemStatus.eomLatched = false;
    systemStatus.needsIdlePacket = false;
    Serial.println("System State Manager Initialized");
}

// ==================== NEXT EXPECTED STATE LOGIC ====================
void updateNextExpectedState() {
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
    
    Serial.printf("Updating next expected based on: [%s:%s:IST%d]\n",
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
                systemStatus.justSentPureToneDetection = true;
                systemStatus.nextExpectedSystemState = SYS_SOS;
                systemStatus.nextExpectedSubsystem = SUB_MDPS;
                systemStatus.nextExpectedIST = 4;
                systemStatus.nextExpectedDescription = "MDPS Pure Tone Response (stop motors)";
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
            if (systemStatus.justSentPureToneDetection) {
                systemStatus.justSentPureToneDetection = false;
                systemStatus.nextExpectedSystemState = SYS_SOS;
                systemStatus.nextExpectedSubsystem = SUB_SNC;
                systemStatus.nextExpectedIST = 0;
                systemStatus.nextExpectedDescription = "Pure Tone Detection (to exit SOS)";
            } else {
                systemStatus.nextExpectedSystemState = SYS_MAZE;
                systemStatus.nextExpectedSubsystem = SUB_SS;
                systemStatus.nextExpectedIST = 1;
                systemStatus.nextExpectedDescription = "SS Colors (MAZE) or SS End-of-Maze";
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
            systemStatus.nextExpectedSystemState = SYS_SOS;
            systemStatus.nextExpectedSubsystem = SUB_SNC;
            systemStatus.nextExpectedIST = 0;
            systemStatus.nextExpectedDescription = "Pure Tone Detection (to exit SOS)";
        }
        else if (packetSubsystem == SUB_SNC && packetIST == 0) {
            if (lastPacket.dat1 == 1) {
                systemStatus.nextExpectedSystemState = SYS_MAZE;
                systemStatus.nextExpectedSubsystem = SUB_SNC;
                systemStatus.nextExpectedIST = 1;
                systemStatus.nextExpectedDescription = "Pure Tone Detection (MAZE after SOS exit)";
            } else {
                systemStatus.nextExpectedSystemState = SYS_SOS;
                systemStatus.nextExpectedSubsystem = SUB_SNC;
                systemStatus.nextExpectedIST = 0;
                systemStatus.nextExpectedDescription = "Pure Tone Detection (continue waiting in SOS)";
            }
        }
    }
    
    Serial.printf("Next expected: [%s:%s:IST%d] - %s\n",
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
    
    Serial.printf("Processing packet for state transition: [%s:%s:IST%d]\n",
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
            systemStatus.eomLatched = false;  // Clear end-of-maze flag for new run
            Serial.println("STATE TRANSITION: IDLE → CAL (First touch detected)");

            // Reset NAVCON when entering CAL state
            extern NavconStatus navcon_status;
            navcon_status.reset();
            Serial.println("NAVCON: Reset for CAL state");
        }
    }
    else if (packetSubsystem == SUB_SNC && packetSysState == SYS_CAL && packetIST == 0) {
        if (packet.dat1 == 1) {
            systemStatus.currentSystemState = SYS_MAZE;
            systemStatus.lastTransitionTime = millis();
            Serial.println("STATE TRANSITION: CAL → MAZE (Second touch detected)");
        }
    }
    else if (packetSubsystem == SUB_SNC && packetSysState == SYS_MAZE && packetIST == 1) {
        if (packet.dat1 == 1) {
            systemStatus.currentSystemState = SYS_SOS;
            systemStatus.lastTransitionTime = millis();
            Serial.println("STATE TRANSITION: MAZE → SOS (Pure tone detected)");
        }
    }
    else if (packetSubsystem == SUB_SNC && packetSysState == SYS_MAZE && packetIST == 2) {
        if (packet.dat1 == 1) {
            systemStatus.currentSystemState = SYS_IDLE;
            systemStatus.lastTransitionTime = millis();
            systemStatus.needsIdlePacket = true;  // Flag to send IDLE:SNC:IST0 with dat1=0
            Serial.println("STATE TRANSITION: MAZE → IDLE (Touch detected in MAZE)");
            Serial.println("  -> Will send IDLE:SNC:IST0 packet next");
        }
    }
    else if (packetSubsystem == SUB_SNC && packetSysState == SYS_SOS && packetIST == 0) {
        if (packet.dat1 == 1) {
            systemStatus.currentSystemState = SYS_MAZE;
            systemStatus.lastTransitionTime = millis();
            Serial.println("STATE TRANSITION: SOS → MAZE (Pure tone detected)");
        }
    }
    else if (packetSubsystem == SUB_SS && packetSysState == SYS_MAZE && packetIST == 3) {
        systemStatus.currentSystemState = SYS_IDLE;
        systemStatus.lastTransitionTime = millis();
        systemStatus.eomLatched = true;             // end of maze reached, this is to ensure nothing else gets sent again

        Serial.println("STATE TRANSITION: MAZE → IDLE (End of maze detected)");
        Serial.println("[EOM] systemStatus.eomLatched = TRUE");

        // Reset NAVCON when returning to IDLE state
        extern NavconStatus navcon_status;
        navcon_status.reset();
        Serial.println("NAVCON: Reset for IDLE state");
    }
    
    Serial.printf("Current system state: %s\n", systemStateToString(systemStatus.currentSystemState));
}

// ==================== SNC PACKET GENERATION ====================
bool shouldSendSNCPacket() {
    // After EOM, send NOTHING - no packets at all
    if (systemStatus.eomLatched) {
        return false;
    }
    return (systemStatus.nextExpectedSubsystem == SUB_SNC);
}

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
            // Clear needsIdlePacket flag after sending
            if (systemStatus.needsIdlePacket) {
                systemStatus.needsIdlePacket = false;
                Serial.println("  -> Sent IDLE:SNC:IST0 packet after MAZE→IDLE transition");
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
                if (systemStatus.pureToneDetected) {
                    systemStatus.justSentPureToneDetection = true;
                    systemStatus.pureToneDetected = false;
                    Serial.println("SNC: Setting pure tone flag - next MDPS IST4 will be SOS response");
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
                // THIS IS WHERE NAVCON IS CALLED
                Serial.println("NAVCON CALLED: Running enhanced navigation logic");
                return runEnhancedNavcon();  // Return NAVCON packet directly
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

// ==================== AUTO-SEND TIMING FUNCTIONS ====================
bool shouldSendSNCPacketNow() {
    // Allow IDLE packets even after EOM (to restart the system)
    if (systemStatus.eomLatched && systemStatus.currentSystemState != SYS_IDLE) {
        return false;
    }
    unsigned long now = millis();
    
    // In IDLE: Only send once, then wait for touch
    // ALSO send if needsIdlePacket flag is set (after MAZE→IDLE transition)
    if (systemStatus.currentSystemState == SYS_IDLE) {
        if (!idleSentOnce || systemStatus.touchDetected || systemStatus.needsIdlePacket) {
            return true;
        }
        return false;
    } 
    else {
        // Other states: Send with rate limiting
        idleSentOnce = false;  // Reset when leaving IDLE
        
        // NAVCON gets immediate response (no rate limiting)
        if (systemStatus.currentSystemState == SYS_MAZE && 
            systemStatus.nextExpectedIST == 3) {
            return true;
        }
        
        // Regular SNC packets with rate limiting
        unsigned long interval = 500;  // Normal interval for other states
        if (now - lastAutoSend >= interval) {
            return true;
        }
    }
    
    return false;
}

void updateAutoSendState() {
    unsigned long now = millis();
    
    if (systemStatus.currentSystemState == SYS_IDLE) {
        if (systemStatus.touchDetected) {
            idleSentOnce = false;  // Reset for next IDLE cycle
        } else {
            idleSentOnce = true;   // Mark that we sent in IDLE
        }
    }
    
    lastAutoSend = now;
}

// ==================== STATUS REPORTING ====================
void printSystemStatus() {
    Serial.println("\n============================================");
    Serial.println("           MARV SYSTEM STATUS");
    Serial.println("============================================");
    
    Serial.println("*** CURRENT STATE MACHINE STATUS ***");
    Serial.printf("║ CURRENT SYSTEM STATE: %s\n", systemStateToString(systemStatus.currentSystemState));
    Serial.printf("║ NEXT EXPECTED SUBSYSTEM: %s\n", subsystemToString(systemStatus.nextExpectedSubsystem));
    Serial.printf("║ NEXT EXPECTED IST: %d\n", systemStatus.nextExpectedIST);
    Serial.printf("║ EXPECTING: %s\n", systemStatus.nextExpectedDescription.c_str());
    Serial.println("******************************************");
    
    Serial.println("");
    Serial.println("Additional Status Information:");
    Serial.printf("🎉 END OF MAZE: %s\n", systemStatus.eomLatched ? "✅ YES - MAZE COMPLETE!" : "NO");
    Serial.printf("Waiting for 2nd Touch: %s\n", systemStatus.waitingForSecondTouch ? "YES" : "NO");
    Serial.printf("Unexpected Packets: %d\n", systemStatus.unexpectedPacketCount);
    Serial.printf("GPIO Commands Received: %d\n", systemStatus.gpioCommandCount);
    Serial.printf("Touch Ready: %s, Pure Tone Ready: %s, Send Ready: %s\n",
                 systemStatus.touchDetected ? "YES" : "NO",
                 systemStatus.pureToneDetected ? "YES" : "NO",
                 systemStatus.manualSendTrigger ? "YES" : "NO");
    
    unsigned long uptime = millis();
    Serial.printf("System Uptime: %lu seconds\n", uptime / 1000);
    
    if (systemStatus.lastTransitionTime > 0) {
        unsigned long timeSinceTransition = (millis() - systemStatus.lastTransitionTime) / 1000;
        Serial.printf("Time since last transition: %lu seconds\n", timeSinceTransition);
    }
    
    Serial.println("============================================\n");
}

void printCompactStatus() {
    Serial.println("\n*** QUICK STATUS ***");
    if (systemStatus.eomLatched) {
        Serial.println("🎉 ✅ MAZE COMPLETE - END OF MAZE REACHED! 🎉");
    }
    Serial.printf("STATE: %s → EXPECTING: %s:IST%d (%s)\n",
                  systemStateToString(systemStatus.currentSystemState),
                  subsystemToString(systemStatus.nextExpectedSubsystem),
                  systemStatus.nextExpectedIST,
                  systemStatus.nextExpectedDescription.c_str());
    Serial.println("*******************\n");
}

void updateStatusDisplay() {
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
}

// ==================== MANUAL CONTROL FUNCTIONS ====================
void simulateTouch() {
    systemStatus.touchDetected = true;
    Serial.println("MANUAL: Touch detected via serial");
}

void simulatePureTone() {
    systemStatus.pureToneDetected = true;
    Serial.println("MANUAL: Pure tone detected via serial");
}

void manualSendTrigger() {
    systemStatus.manualSendTrigger = true;
    Serial.println("MANUAL: Send trigger activated via serial");
}

void handleSerialCommands() {
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
            case 'n': case 'N':
                printNavconDebugInfo();
                break;
        }
    }
}