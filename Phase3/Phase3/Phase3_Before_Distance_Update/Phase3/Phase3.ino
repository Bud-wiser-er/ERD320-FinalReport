/*
 * MARV SNC Subsystem - Phase 0 with Enhanced NAVCON
 * Main ESP32 Controller with Modular Architecture
 * 
 * Files in this project:
 * - Phase0_Main.ino        (this file - main control loop)
 * - navcon_core.h/.cpp     (NAVCON navigation system)
 * - scs_protocol.h/.cpp    (SCS packet handling)
 * - system_state.h/.cpp    (System state management)
 * - gpio_commands.h/.cpp   (GPIO pin handling)
 */

// ==================== INCLUDES ====================
#include "scs_protocol.h"
#include "system_state.h"
#include "navcon_core.h"
#include "gpio_commands.h"
#include "edge_case_matrix.h"

#include "spi_protocol.h"
// Note: spi_protocol_impl.cpp will be automatically included by Arduino IDE

SPIClass spi(VSPI);
MarvSPIComm spi_comm(&spi, 5);

// SPI Communication Rate Control
struct {
    unsigned long lastSystemState = 0;
    unsigned long lastSensorData = 0;
    unsigned long lastMovementData = 0;
    unsigned long lastHeartbeat = 0;
    uint8_t updateCounter = 0;
} spiTiming;

// Cache latest sensor and movement data from packets
struct SPIDataCache {
    uint8_t sensor1_color = 0;
    uint8_t sensor2_color = 0;
    uint8_t sensor3_color = 0;
    uint8_t wheelSpeedR = 0;
    uint8_t wheelSpeedL = 0;
    uint8_t wheelSetpoint = 0;
    uint16_t distance_mm = 0;
    uint16_t incidence_angle = 0;
    uint16_t rotation_angle = 0;      // Last rotation angle
    uint8_t rotation_direction = 0;   // 2=LEFT, 3=RIGHT
    bool endOfMazeDetected = false;   // End of maze flag
    bool hasNewSensorData = false;
    bool hasNewMovementData = false;
    bool hasNewRotation = false;      // Flag for new rotation data
};
SPIDataCache spiDataCache;
// ==================== PIN DEFINITIONS ====================
// UART pins for subsystem communication
#define RX_SS   21
#define TX_SS   22
#define RX_MDPS 16
#define TX_MDPS 17

// ==================== GLOBAL INSTANCES ====================
SerialPacketHandler ssHandler(&Serial1, RX_SS, TX_SS);
SerialPacketHandler mdpsHandler(&Serial2, RX_MDPS, TX_MDPS);

// ==================== ARDUINO SETUP ====================
void setup() {
    Serial.begin(115200);
    delay(1000);
    
    Serial.println("========================================");
    Serial.println("MARV SNC Subsystem - PHASE 0 + NAVCON");
    Serial.println("Main ESP32 with Modular Architecture");
    Serial.println("========================================");
    
    // Initialize UART handlers
    ssHandler.begin(19200);
    mdpsHandler.begin(19200);
    Serial.println("UART handlers initialized (19200 baud)");
    
    // Initialize all modules
    setupGPIOCommands();
    initializeSystemState();
    initializeNavcon();
    // Intialize the SPI connection
    spi_comm.begin();
    
    Serial.println("========================================");
    Serial.println("Commands available:");
    Serial.println("   Serial: T (touch), P (pure tone), S (send), ? (status)");
    Serial.println("   Serial: N (NAVCON debug)");
    Serial.println("   GPIO: Connect WiFi ESP32 to pins 4, 2, 15");
    Serial.println("========================================");
    Serial.println("System ready!");
    Serial.println("========================================\n");
    
    printSystemStatus();
}

// ==================== MAIN CONTROL LOOP ====================
void loop() {
    // Check for GPIO commands from WiFi ESP32
    checkWiFiCommands();

    // Check pure tone ADC input (every loop for high responsiveness)
    // The flag will be sent in the next SNC packet when it's our turn
    checkPureToneADC();

    // Send periodic SPI updates to WiFi ESP32 (100Hz rate = 10ms interval)
    static unsigned long lastSPIUpdate = 0;
    if (millis() - lastSPIUpdate >= 10) { // 100Hz update rate (10ms interval)
        sendSPIUpdates();
        lastSPIUpdate = millis();
    }

    SCSPacket packet;
    
    // ==================== HANDLE SS PACKETS ====================
    if (ssHandler.readPacket(packet)) {
        // If EOM detected, do ABSOLUTELY NOTHING - no forwarding, no processing, NOTHING
        if (systemStatus.eomLatched) {
            return;  // STOP - do NOTHING after EOM
        }

        printPacket(packet, "RX SS:");
        systemStatus.lastSSPacket = "Received SS packet";

        // Cache sensor data for SPI display
        SystemState pktState = getSystemState(packet.control);
        SubsystemID pktSub = getSubsystemID(packet.control);
        uint8_t pktIST = getInternalState(packet.control);

        // Log ALL SS packet data for debugging
        // Serial.printf("[SS-PKT] State=%d Sub=%d IST=%d | dat1=%d dat0=%d dec=%d\n",
        //              pktState, pktSub, pktIST, packet.dat1, packet.dat0, packet.dec);

        // Check for end-of-maze (SS sends IST=3 in MAZE state)
        if (pktSub == SUB_SS && pktState == SYS_MAZE && pktIST == 3) {
            if (!systemStatus.eomLatched) {
                Serial.println("🎉 END OF MAZE DETECTED (via Serial1) - Setting flags!");

                // Set end of maze flags ONCE
                spiDataCache.endOfMazeDetected = true;
                systemStatus.eomLatched = true;

                // Transition SNC to IDLE (but don't send IDLE packet)
                systemStatus.currentSystemState = SYS_IDLE;
                systemStatus.nextExpectedSubsystem = SUB_SNC;
                systemStatus.nextExpectedIST = 0;

                // Send WiFi notification ONCE
                spi_comm.sendSensorColors(COLOR_RED, COLOR_RED, COLOR_RED);
                delayMicroseconds(500);
                spi_comm.sendEndOfMaze();

                Serial.println("EOM flags set. SNC transitioned to IDLE. SNC will NOT transmit any packets.");
            }

            // STOP - do NOT forward anything after EOM
            return;
        }

        // Extract sensor data based on SS IST
        if (pktSub == SUB_SS) {
            switch(pktIST) {
                case 1: // Sensor colors in dat0 and dat1
                    // Colors are packed in 16 bits: (dat1 << 8) | dat0
                    // Each color is 3 bits: S1 (bits 6-8), S2 (bits 3-5), S3 (bits 0-2)
                    // Color values: 0=WHITE, 1=RED, 2=GREEN, 3=BLUE, 4=BLACK
                    {
                        uint16_t colorData = (packet.dat1 << 8) | packet.dat0;
                        spiDataCache.sensor1_color = (colorData >> 6) & 0x07;  // Sensor 1
                        spiDataCache.sensor2_color = (colorData >> 3) & 0x07;  // Sensor 2
                        spiDataCache.sensor3_color = colorData & 0x07;          // Sensor 3
                        spiDataCache.hasNewSensorData = true;

                        Serial.printf("[SS-COLOR] State=%d Received: S1=%d S2=%d S3=%d (colorData=0x%04X)\n",
                                     systemStatus.currentSystemState,
                                     spiDataCache.sensor1_color, spiDataCache.sensor2_color,
                                     spiDataCache.sensor3_color, colorData);

                        // Send immediately
                        spi_comm.sendSensorColors(
                            (Color)spiDataCache.sensor1_color,
                            (Color)spiDataCache.sensor2_color,
                            (Color)spiDataCache.sensor3_color
                        );
                    }
                    break;

                case 2: // Incidence angle in dat1
                    spiDataCache.incidence_angle = packet.dat1;
                    // Serial.printf("  -> Incidence angle: %d degrees\n", packet.dat1);
                    // Send angle data immediately
                    spi_comm.sendIncidenceAngle(packet.dat1, 0, 0, 0);
                    break;

                default:
                    // Serial.printf("  -> Unknown SS IST=%d\n", pktIST);
                    break;
            }
        }
        // Process MDPS packets that come through SS serial (forwarded from MDPS)
        else if (pktSub == SUB_MDPS) {
            switch(pktIST) {
                case 1: // Battery/Level indicator
                    // Ignore - not needed for display
                    break;

                case 2: // Rotation angle (NAVCON)
                    spiDataCache.rotation_angle = (packet.dat1 << 8) | packet.dat0;
                    spiDataCache.rotation_direction = packet.dec;
                    spiDataCache.hasNewRotation = true;
                    break;

                case 3: // Movement speeds (NAVCON forward)
                    spiDataCache.wheelSpeedR = packet.dat1;
                    spiDataCache.wheelSpeedL = packet.dat0;
                    spiDataCache.wheelSetpoint = packet.dat1;
                    spiDataCache.hasNewMovementData = true;
                    // Send immediately to display
                    spi_comm.sendWheelSpeeds(packet.dat1, packet.dat0, packet.dat1);
                    break;

                case 4: // Distance traveled
                    spiDataCache.distance_mm = (packet.dat1 << 8) | packet.dat0;
                    break;

                default:
                    break;
            }
        }

        // Process state transitions
        processStateTransition(packet);

        // Update NAVCON with incoming data
        handleNavconIncomingData(packet);

        // Forward to MDPS
        mdpsHandler.sendPacket(packet);
        Serial.println("Forwarded SS packet to MDPS");
    }
    
    // ==================== HANDLE MDPS PACKETS ====================
    if (mdpsHandler.readPacket(packet)) {
        // If EOM detected, do ABSOLUTELY NOTHING - no forwarding, no processing, NOTHING
        if (systemStatus.eomLatched) {
            return;  // STOP - do NOTHING after EOM
        }

        printPacket(packet, "RX MDPS:");
        systemStatus.lastMDPSPacket = "Received MDPS packet";

        // Cache movement data for SPI display
        SystemState pktState = getSystemState(packet.control);
        SubsystemID pktSub = getSubsystemID(packet.control);
        uint8_t pktIST = getInternalState(packet.control);

        // Log ALL MDPS packet data for debugging
        // Serial.printf("[MDPS-PKT] State=%d Sub=%d IST=%d | dat1=%d dat0=%d dec=%d\n",
        //              pktState, pktSub, pktIST, packet.dat1, packet.dat0, packet.dec);

        // Check for end-of-maze (SS sends IST=3 in MAZE state) - same check as SS handler
        if (pktSub == SUB_SS && pktState == SYS_MAZE && pktIST == 3) {
            if (!systemStatus.eomLatched) {
                Serial.println("🎉 END OF MAZE DETECTED (via Serial2) - Setting flags!");

                // Set end of maze flags ONCE
                spiDataCache.endOfMazeDetected = true;
                systemStatus.eomLatched = true;

                // Transition SNC to IDLE (but don't send IDLE packet)
                systemStatus.currentSystemState = SYS_IDLE;
                systemStatus.nextExpectedSubsystem = SUB_SNC;
                systemStatus.nextExpectedIST = 0;

                // Send WiFi notification ONCE
                spi_comm.sendSensorColors(COLOR_RED, COLOR_RED, COLOR_RED);
                delayMicroseconds(500);
                spi_comm.sendEndOfMaze();

                Serial.println("EOM flags set. SNC transitioned to IDLE. SNC will NOT transmit any packets.");
            }

            // STOP - do NOT forward anything after EOM
            return;
        }

        // Process packets by subsystem (MDPS packets AND forwarded SS packets)
        if (pktSub == SUB_MDPS) {
            // MDPS packets - DIFFERENT ISTs have DIFFERENT meanings!
            switch(pktIST) {
                case 1: // Battery/Level indicator (NOT wheel speeds!)
                    // dat1 = battery level (e.g., 90, 80)
                    // dat0 = reserved/level indicator
                    // DO NOT treat as wheel speeds - ignore this packet
                    // Serial.printf("  -> Battery level: %d%%\n", packet.dat1);
                    break;

                case 2: // Rotation angle (NAVCON)
                    // dat0 contains rotation angle in degrees (low byte)
                    // dat1 contains rotation angle (high byte) or direction
                    // dec contains direction (2=LEFT/CCW, 3=RIGHT/CW)
                    spiDataCache.rotation_angle = (packet.dat1 << 8) | packet.dat0;
                    spiDataCache.rotation_direction = packet.dec;
                    spiDataCache.hasNewRotation = true;
                    // Serial.printf("  -> Rotation: %d degrees, direction=%d\n",
                    //              spiDataCache.rotation_angle, spiDataCache.rotation_direction);
                    break;

                case 3: // Movement speeds (NAVCON forward)
                    spiDataCache.wheelSpeedR = packet.dat1;
                    spiDataCache.wheelSpeedL = packet.dat0;
                    spiDataCache.wheelSetpoint = packet.dat1; // Use dat1 as setpoint
                    spiDataCache.hasNewMovementData = true;
                    // Serial.printf("  -> Movement speeds: R=%d L=%d\n", packet.dat1, packet.dat0);
                    // Send immediately to display
                    spi_comm.sendWheelSpeeds(packet.dat1, packet.dat0, packet.dat1);
                    break;

                case 4: // Distance traveled
                    // Distance is in dat0 (low byte) for short distances
                    // or (dat1 << 8) | dat0 for longer distances
                    spiDataCache.distance_mm = (packet.dat1 << 8) | packet.dat0;
                    // Serial.printf("  -> Distance: %d mm\n", spiDataCache.distance_mm);
                    // Don't send here - will be sent in periodic update to avoid lag
                    break;

                default:
                    // Serial.printf("  -> Unknown MDPS IST=%d\n", pktIST);
                    break;
            }
        }
        else if (pktSub == SUB_SS) {
            // SS packets forwarded through MDPS - process sensor data
            switch(pktIST) {
                case 1: // Sensor colors in dat0 and dat1
                    {
                        uint16_t colorData = (packet.dat1 << 8) | packet.dat0;
                        spiDataCache.sensor1_color = (colorData >> 6) & 0x07;  // Sensor 1
                        spiDataCache.sensor2_color = (colorData >> 3) & 0x07;  // Sensor 2
                        spiDataCache.sensor3_color = colorData & 0x07;          // Sensor 3
                        spiDataCache.hasNewSensorData = true;

                        Serial.printf("[MDPS-SS-COLOR] State=%d Received: S1=%d S2=%d S3=%d (colorData=0x%04X)\n",
                                     systemStatus.currentSystemState,
                                     spiDataCache.sensor1_color, spiDataCache.sensor2_color,
                                     spiDataCache.sensor3_color, colorData);

                        // Send immediately
                        spi_comm.sendSensorColors(
                            (Color)spiDataCache.sensor1_color,
                            (Color)spiDataCache.sensor2_color,
                            (Color)spiDataCache.sensor3_color
                        );
                    }
                    break;

                case 2: // Incidence angle
                    spiDataCache.incidence_angle = packet.dat1;
                    spi_comm.sendIncidenceAngle(packet.dat1, 0, 0, 0);
                    break;
            }
        }

        // Process state transitions
        processStateTransition(packet);

        // Update NAVCON with incoming data
        handleNavconIncomingData(packet);

        // Forward to SS
        ssHandler.sendPacket(packet);
        Serial.println("Forwarded MDPS packet to SS");
    }
    
    // ==================== SEND SNC PACKETS ====================
    if (shouldSendSNCPacket()) {
        if (shouldSendSNCPacketNow()) {
            SCSPacket sncPacket = generateSNCPacket();
            
            if (sncPacket.control != 0) {
                // Determine packet type for logging
                const char* packetType = "TX SNC";
                if (systemStatus.currentSystemState == SYS_MAZE && 
                    systemStatus.nextExpectedIST == 3) {
                    packetType = "TX NAVCON";
                }
                
                printPacket(sncPacket, packetType);
                systemStatus.lastSNCPacket = "Sent SNC packet";
                
                // Process our own packet
                processStateTransition(sncPacket);
                
                // Send to both subsystems
                ssHandler.sendPacket(sncPacket);
                mdpsHandler.sendPacket(sncPacket);
                
                Serial.println("SNC packet sent");
                
                // Update auto-send state
                updateAutoSendState();
            }
        }
    }
    
    // ==================== HANDLE SERIAL COMMANDS ====================
    handleSerialCommands();
    
    // ==================== PERIODIC STATUS UPDATES ====================
    updateStatusDisplay();
    
    // Small delay to prevent overwhelming the system
    delay(10);
}

// ==================== INTEGRATION NOTES ====================
/*
 * SYSTEM ARCHITECTURE SUMMARY:
 * 
 * 1. **Main Loop (this file)**:
 *    - Handles UART communication
 *    - Coordinates all modules
 *    - Manages packet forwarding
 * 
 * 2. **NAVCON Module (navcon_core.h/.cpp)**:
 *    - Complete navigation state machine
 *    - Line detection and angle correction
 *    - Called automatically when MAZE:SNC:IST=3
 * 
 * 3. **SCS Protocol (scs_protocol.h/.cpp)**:
 *    - Packet parsing and creation
 *    - Serial communication handling
 *    - Debug utilities
 * 
 * 4. **System State (system_state.h/.cpp)**:
 *    - State transition management
 *    - SNC packet generation
 *    - Status reporting
 * 
 * 5. **GPIO Commands (gpio_commands.h/.cpp)**:
 *    - Physical pin handling
 *    - WiFi ESP32 interface
 *    - Command debouncing
 * 
 * TESTING WORKFLOW:
 * 1. Upload this code to ESP32
 * 2. Use serial commands (T, P, ?, N) for basic testing
 * 3. Connect WiFi ESP32 to pins 4, 2, 15 for GPIO testing
 * 4. Monitor serial output for state transitions
 * 5. Test NAVCON by reaching MAZE state with SNC IST=3
 * 
 * KEY FEATURES:
 * - Modular architecture for easy debugging
 * - Complete NAVCON integration
 * - Proper SCS protocol compliance
 * - GPIO command support
 * - Comprehensive status reporting
 * - Clean separation of concerns
 * - Efficient SPI communication to WiFi ESP32
 * - Optimized update rates to prevent system overload
 */

// ==================== SPI COMMUNICATION FUNCTIONS ====================
// Called every 10ms from main loop - sends fast periodic updates (100Hz)
void sendSPIUpdates() {
    unsigned long currentTime = millis();
    spiTiming.updateCounter++;

    // Send system state (every call = 100Hz)
    spi_comm.sendSystemState(
        (SystemState)systemStatus.currentSystemState,
        (Subsystem)SUB_SNC,
        systemStatus.nextExpectedIST
    );
    delayMicroseconds(500); // Small delay between SPI packets

    // Send touch/tone status if detected
    if (systemStatus.touchDetected) {
        spi_comm.sendTouchDetected(true, (SystemState)systemStatus.currentSystemState, 140);
        delayMicroseconds(500);
    }
    if (systemStatus.pureToneDetected) {
        spi_comm.sendPureTone(true, 1000, 80);
        delayMicroseconds(500);
    }

    // Update cache from NAVCON's current_colors (ONLY in MAZE state - NAVCON is active)
    // In CAL state, colors come directly from SS packets and should NOT be overwritten
    extern uint8_t current_colors[3]; // From navcon_core.h
    extern uint8_t received_incidence_angle; // From navcon_core.h
    extern uint16_t current_rotation; // From navcon_core.h
    extern uint8_t current_rotation_dir; // From navcon_core.h
    extern uint16_t current_distance; // From navcon_core.h

    // Only use NAVCON colors in MAZE state (when NAVCON is running)
    if (systemStatus.currentSystemState == SYS_MAZE) {
        spiDataCache.sensor1_color = current_colors[0];
        spiDataCache.sensor2_color = current_colors[1];
        spiDataCache.sensor3_color = current_colors[2];
        spiDataCache.incidence_angle = received_incidence_angle;
    }
    // In other states (CAL, IDLE, SOS), colors come from SS packets and are already in cache

    // Update rotation data from NAVCON if available
    if (current_rotation > 0) {
        spiDataCache.rotation_angle = current_rotation;
        spiDataCache.rotation_direction = current_rotation_dir;
    }

    // Update distance from NAVCON (smoother than packet-only updates)
    if (current_distance > 0) {
        spiDataCache.distance_mm = current_distance;
    }

    // Send cached sensor colors (every call = 100Hz)
    // static unsigned long lastColorDebug = 0;
    // if (millis() - lastColorDebug > 2000) { // Every 2 seconds
    //     Serial.printf("[SPI-TX] State=%d Sending colors: S1=%d S2=%d S3=%d\n",
    //                  systemStatus.currentSystemState,
    //                  spiDataCache.sensor1_color,
    //                  spiDataCache.sensor2_color,
    //                  spiDataCache.sensor3_color);
    //     lastColorDebug = millis();
    // }
    spi_comm.sendSensorColors(
        (Color)spiDataCache.sensor1_color,
        (Color)spiDataCache.sensor2_color,
        (Color)spiDataCache.sensor3_color
    );
    delayMicroseconds(500); // Small delay between SPI packets

    // Send cached wheel speeds (every call = 100Hz)
    spi_comm.sendWheelSpeeds(
        spiDataCache.wheelSpeedR,
        spiDataCache.wheelSpeedL,
        spiDataCache.wheelSetpoint
    );
    delayMicroseconds(500); // Small delay between SPI packets

    // Send incidence angle periodically (every call = 100Hz)
    spi_comm.sendIncidenceAngle(spiDataCache.incidence_angle, 0, 0, 0);
    delayMicroseconds(500); // Small delay between SPI packets

    // Send rotation data (every call = 100Hz for smooth updates)
    if (spiDataCache.hasNewRotation || spiDataCache.rotation_angle > 0) {
        spi_comm.sendRotationAngle(spiDataCache.rotation_angle, spiDataCache.rotation_direction);
        delayMicroseconds(500);
        // Clear flag after sending once
        if (spiDataCache.hasNewRotation) {
            spiDataCache.hasNewRotation = false;
        }
    }

    // Send distance data (every call = 100Hz for smooth updates)
    spi_comm.sendDistance(spiDataCache.distance_mm);
    delayMicroseconds(500);

    // Sync end-of-maze flag from system status (in case cache wasn't set)
    if (systemStatus.eomLatched && !spiDataCache.endOfMazeDetected) {
        spiDataCache.endOfMazeDetected = true;
        Serial.println("[EOM-SYNC] Syncing eomLatched -> spiDataCache.endOfMazeDetected");
    }

    // Send end-of-maze status (every call = 100Hz when detected for reliability)
    if (spiDataCache.endOfMazeDetected) {
        // Serial.println("[EOM-SPI] >>> SENDING PKT_END_OF_MAZE NOW <<<");
        spi_comm.sendEndOfMaze();
        delayMicroseconds(500);
        // Also send all RED sensors to emphasize completion
        spi_comm.sendSensorColors(COLOR_RED, COLOR_RED, COLOR_RED);
        delayMicroseconds(500);
    }

    // Debug: Print flag status every second to verify it's set
    // static unsigned long lastFlagCheck = 0;
    // if (millis() - lastFlagCheck > 1000) {
    //     if (spiDataCache.endOfMazeDetected) {
    //         Serial.printf("[EOM-DEBUG] Flag is SET: eomCache=%d, eomSys=%d, updateCounter=%d\n",
    //                      spiDataCache.endOfMazeDetected,
    //                      systemStatus.eomLatched,
    //                      spiTiming.updateCounter);
    //     }
    //     lastFlagCheck = millis();
    // }

    // Heartbeat every 100th call (1 second at 100Hz)
    if (spiTiming.updateCounter % 100 == 0) {
        spi_comm.sendHeartbeat();

        // Performance monitoring every 10 seconds
        // if (spiTiming.updateCounter % 1000 == 0) {
        //     Serial.printf("[SPI] Sent %d updates, Uptime: %lu ms\n",
        //                  spiTiming.updateCounter, currentTime);
        // }
    }

    // Debug messages every 10 seconds (1000 calls at 100Hz)
    if (spiTiming.updateCounter % 1000 == 0) {
        char debugMsg[100];
        if (spiDataCache.endOfMazeDetected) {
            snprintf(debugMsg, sizeof(debugMsg),
                    "🎉 MAZE COMPLETE! Uptime=%lus",
                    currentTime / 1000);
        } else {
            snprintf(debugMsg, sizeof(debugMsg),
                    "State=%d IST=%d Uptime=%lus",
                    systemStatus.currentSystemState,
                    systemStatus.nextExpectedIST,
                    currentTime / 1000);
        }
        spi_comm.sendDebug(0, debugMsg);
    }
}