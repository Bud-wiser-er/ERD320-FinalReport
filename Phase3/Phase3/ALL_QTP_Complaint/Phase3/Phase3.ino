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
    
    SCSPacket packet;
    
    // ==================== HANDLE SS PACKETS ====================
    if (ssHandler.readPacket(packet)) {
        printPacket(packet, "RX SS:");
        systemStatus.lastSSPacket = "Received SS packet";
        
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
        printPacket(packet, "RX MDPS:");
        systemStatus.lastMDPSPacket = "Received MDPS packet";
        
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
 */