#ifndef SYSTEM_STATE_H
#define SYSTEM_STATE_H

#include "scs_protocol.h"

// ==================== SYSTEM STATUS STRUCTURE ====================
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
    bool justSentPureToneDetection; // Track if we just sent pure tone detection
    
    // Error tracking
    int unexpectedPacketCount;
    int gpioCommandCount;
    
    // Last packets for display
    String lastSSPacket;
    String lastMDPSPacket;
    String lastSNCPacket;

    //End of maze flag
    bool eomLatched;
};

// Global system status
extern SystemStatus systemStatus;

// ==================== STATE MANAGEMENT FUNCTIONS ====================
/**
 * Initialize system state management
 */
void initializeSystemState();

/**
 * Update next expected state based on current system state
 */
void updateNextExpectedState();

/**
 * Update next expected state based on last received packet
 * @param lastPacket: The packet that was just processed
 */
void updateNextExpectedBasedOnLastPacket(const SCSPacket& lastPacket);

/**
 * Process state transitions based on received packet
 * @param packet: Incoming packet to process
 */
void processStateTransition(const SCSPacket& packet);

/**
 * Check if we should send an SNC packet now
 * @return: true if it's time to send SNC packet
 */
bool shouldSendSNCPacket();

/**
 * Check if we should send an SNC packet now (with timing logic)
 * @return: true if it's time to send SNC packet now
 */
bool shouldSendSNCPacketNow();

/**
 * Update auto-send timing state after sending packet
 */
void updateAutoSendState();

/**
 * Generate appropriate SNC packet based on current state
 * @return: SCS packet to send
 */
SCSPacket generateSNCPacket();

// ==================== STATUS REPORTING FUNCTIONS ====================
/**
 * Print comprehensive system status
 */
void printSystemStatus();

/**
 * Print compact status summary
 */
void printCompactStatus();

/**
 * Update periodic status displays
 */
void updateStatusDisplay();

// ==================== MANUAL CONTROL FUNCTIONS ====================
/**
 * Simulate touch detection via serial command
 */
void simulateTouch();

/**
 * Simulate pure tone detection via serial command
 */
void simulatePureTone();

/**
 * Trigger manual packet send via serial command
 */
void manualSendTrigger();

/**
 * Handle serial commands (T, P, S, ?, N)
 */
void handleSerialCommands();

#endif // SYSTEM_STATE_H