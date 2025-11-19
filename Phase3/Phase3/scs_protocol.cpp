#include "scs_protocol.h"

// ==================== SCS PACKET IMPLEMENTATION ====================
SCSPacket::SCSPacket() : control(0), dat1(0), dat0(0), dec(0) {}

SCSPacket::SCSPacket(uint8_t ctrl, uint8_t d1, uint8_t d0, uint8_t d) 
    : control(ctrl), dat1(d1), dat0(d0), dec(d) {}

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

void printPacket(const SCSPacket& packet, const char* direction) {
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

// ==================== SERIAL PACKET HANDLER IMPLEMENTATION ====================
SerialPacketHandler::SerialPacketHandler(HardwareSerial* ser, int rx, int tx) 
    : serial(ser), bufferIndex(0), lastByteTime(0), synced(false), rxPin(rx), txPin(tx) {}

void SerialPacketHandler::begin(unsigned long baud) {
    serial->begin(baud, SERIAL_8N1, rxPin, txPin);
    bufferIndex = 0;
    synced = false;
}

bool SerialPacketHandler::readPacket(SCSPacket& packet) {
    while (serial->available()) {
        uint8_t incomingByte = serial->read();
        unsigned long currentTime = millis();
        
        // Reset buffer if timeout occurred (3ms for optimal recovery)
        if (currentTime - lastByteTime > 3 && bufferIndex > 0) {
            bufferIndex = 0;
            synced = false;
        }
        
        lastByteTime = currentTime;
        buffer[bufferIndex] = incomingByte;
        bufferIndex++;
        
        // Try to find valid packet when we have enough bytes
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
            
            // Prevent buffer overflow
            if (bufferIndex >= BUFFER_SIZE) {
                memmove(buffer, buffer + 1, BUFFER_SIZE - 1);
                bufferIndex = BUFFER_SIZE - 1;
            }
        }
    }
    
    return false;
}

void SerialPacketHandler::sendPacket(const SCSPacket& packet) {
    serial->write(packet.control);
    serial->write(packet.dat1);
    serial->write(packet.dat0);
    serial->write(packet.dec);
    serial->flush();
}

bool SerialPacketHandler::isSynced() const {
    return synced;
}

int SerialPacketHandler::getBufferLevel() const {
    return bufferIndex;
}