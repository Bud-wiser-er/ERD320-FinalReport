#ifndef SCS_PROTOCOL_H
#define SCS_PROTOCOL_H

#include <Arduino.h>
#include <HardwareSerial.h>

// ==================== SCS SYSTEM DEFINITIONS ====================
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

// ==================== SCS PACKET STRUCTURE ====================
struct SCSPacket {
    uint8_t control;    // CONTROL<31:24>: SYS<1:0> | SUB<1:0> | IST<3:0>
    uint8_t dat1;       // DAT1<23:16>: Upper data byte
    uint8_t dat0;       // DAT0<15:8>: Lower data byte  
    uint8_t dec;        // DEC<7:0>: Decimal/general purpose byte
    
    SCSPacket();
    SCSPacket(uint8_t ctrl, uint8_t d1, uint8_t d0, uint8_t d);
};

// ==================== PACKET PARSING FUNCTIONS ====================
/**
 * Extract system state from control byte
 */
SystemState getSystemState(uint8_t control);

/**
 * Extract subsystem ID from control byte
 */
SubsystemID getSubsystemID(uint8_t control);

/**
 * Extract internal state from control byte
 */
uint8_t getInternalState(uint8_t control);

/**
 * Create control byte from components
 */
uint8_t createControlByte(SystemState sys, SubsystemID sub, uint8_t ist);

// ==================== DEBUG FUNCTIONS ====================
/**
 * Convert system state to string for debugging
 */
const char* systemStateToString(SystemState state);

/**
 * Convert subsystem ID to string for debugging
 */
const char* subsystemToString(SubsystemID sub);

/**
 * Print packet details for debugging
 */
void printPacket(const SCSPacket& packet, const char* direction = "");

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
    /**
     * Constructor
     * @param ser: Pointer to HardwareSerial instance
     * @param rx: RX pin number
     * @param tx: TX pin number
     */
    SerialPacketHandler(HardwareSerial* ser, int rx, int tx);
    
    /**
     * Initialize serial communication
     * @param baud: Baud rate (typically 19200)
     */
    void begin(unsigned long baud);
    
    /**
     * Read incoming packet from serial buffer
     * @param packet: Reference to packet structure to fill
     * @return: true if valid packet received
     */
    bool readPacket(SCSPacket& packet);
    
    /**
     * Send packet over serial
     * @param packet: Packet to send
     */
    void sendPacket(const SCSPacket& packet);
    
    /**
     * Check if handler is synchronized
     * @return: true if synced
     */
    bool isSynced() const;
    
    /**
     * Get current buffer level for debugging
     * @return: Number of bytes in buffer
     */
    int getBufferLevel() const;
};

#endif // SCS_PROTOCOL_H