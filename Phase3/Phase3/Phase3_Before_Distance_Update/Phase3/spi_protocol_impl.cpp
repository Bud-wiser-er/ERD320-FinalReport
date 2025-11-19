/*
 * spi_protocol_impl.cpp
 * Optimized SPI Communication Implementation for MARV Main ESP32
 * Designed to prevent system overload while maintaining responsiveness
 */

#include "spi_protocol.h"
#include <Arduino.h>

// ============================================================================
// MARV SPI COMMUNICATION CLASS IMPLEMENTATION
// ============================================================================

MarvSPIComm::MarvSPIComm(SPIClass* spi_instance, uint8_t chip_select)
    : spi(spi_instance), cs_pin(chip_select), sequence_counter(0), packets_sent(0) {
    memset(&tx_packet, 0, sizeof(SPIPacket));
}

void MarvSPIComm::begin() {
    pinMode(cs_pin, OUTPUT);
    digitalWrite(cs_pin, HIGH);

    // Use VSPI (GPIO 5=CS, 18=SCK, 23=MOSI, 19=MISO)
    spi->begin();

    Serial.println("SPI Master initialized for WiFi communication");
    Serial.println("  CS: GPIO 15, SCK: GPIO 18, MOSI: GPIO 23");
    Serial.println("  Speed: 2MHz, Mode: 0");
}

uint8_t MarvSPIComm::calculateChecksum(const uint8_t* data, size_t length) {
    uint8_t checksum = 0;
    for (size_t i = 0; i < length; i++) {
        checksum ^= data[i];
    }
    return checksum;
}

void MarvSPIComm::buildHeader(PacketType type, uint8_t payload_length) {
    tx_packet.header.sync1 = 0xAA;
    tx_packet.header.sync2 = 0x55;
    tx_packet.header.packet_type = (uint8_t)type;
    tx_packet.header.data_length = payload_length;
    tx_packet.header.sequence = sequence_counter++;
    tx_packet.header.flags = 0;

    // Calculate header checksum (excluding checksum field itself)
    tx_packet.header.checksum_header = calculateChecksum(
        (uint8_t*)&tx_packet.header,
        sizeof(SPIPacketHeader) - 1
    );
}

bool MarvSPIComm::sendPacket() {
    // Calculate payload checksum
    tx_packet.checksum_payload = calculateChecksum(
        tx_packet.payload,
        tx_packet.header.data_length
    );

    size_t packet_size = sizeof(SPIPacketHeader) + MAX_PAYLOAD_SIZE + 1;

    // Efficient SPI transmission with minimal delays
    digitalWrite(cs_pin, LOW);
    delayMicroseconds(5);  // Reduced from 10 to 5 microseconds

    spi->beginTransaction(SPISettings(2000000, MSBFIRST, SPI_MODE0));
    spi->transfer((uint8_t*)&tx_packet, packet_size);
    spi->endTransaction();

    delayMicroseconds(5);  // Reduced from 10 to 5 microseconds
    digitalWrite(cs_pin, HIGH);

    packets_sent++;
    return true;
}

// ============================================================================
// SYSTEM STATE FUNCTIONS
// ============================================================================

bool MarvSPIComm::sendSystemState(SystemState state, Subsystem sub, uint8_t ist) {
    SystemStatePayload* payload = (SystemStatePayload*)tx_packet.payload;
    buildHeader(PKT_SYSTEM_STATE, sizeof(SystemStatePayload));

    payload->timestamp = millis();
    payload->system_state = (uint8_t)state;
    payload->subsystem = (uint8_t)sub;
    payload->internal_state = ist;
    payload->reserved = 0;

    return sendPacket();
}

bool MarvSPIComm::sendTouchDetected(bool detected, SystemState state, uint16_t vop) {
    TouchPayload* payload = (TouchPayload*)tx_packet.payload;
    buildHeader(PKT_TOUCH_DETECTED, sizeof(TouchPayload));

    payload->timestamp = millis();
    payload->touch_detected = detected ? 1 : 0;
    payload->system_state = (uint8_t)state;
    payload->vop_designed = vop;

    return sendPacket();
}

bool MarvSPIComm::sendPureTone(bool detected, uint16_t freq, uint8_t dB) {
    PureTonePayload* payload = (PureTonePayload*)tx_packet.payload;
    buildHeader(PKT_PURE_TONE, sizeof(PureTonePayload));

    payload->timestamp = millis();
    payload->tone_detected = detected ? 1 : 0;
    payload->frequency = freq;
    payload->dB_level = dB;

    return sendPacket();
}

// ============================================================================
// SENSOR DATA FUNCTIONS
// ============================================================================

bool MarvSPIComm::sendSensorColors(Color s1, Color s2, Color s3) {
    // Debug removed to reduce serial spam

    SensorColorsPayload* payload = (SensorColorsPayload*)tx_packet.payload;
    buildHeader(PKT_SENSOR_COLORS, sizeof(SensorColorsPayload));

    payload->timestamp = millis();
    payload->sensor1_color = (uint8_t)s1;
    payload->sensor2_color = (uint8_t)s2;
    payload->sensor3_color = (uint8_t)s3;
    payload->reserved = 0;

    return sendPacket();
}

bool MarvSPIComm::sendIncidenceAngle(uint16_t angle, uint8_t first_sensor,
                                   uint8_t second_sensor, uint8_t sensors_mask) {
    IncidenceAnglePayload* payload = (IncidenceAnglePayload*)tx_packet.payload;
    buildHeader(PKT_INCIDENCE_ANGLE, sizeof(IncidenceAnglePayload));

    payload->timestamp = millis();
    payload->angle = angle;
    payload->first_sensor = first_sensor;
    payload->second_sensor = second_sensor;
    payload->sensors_used = sensors_mask;
    memset(payload->reserved, 0, 3);

    return sendPacket();
}

bool MarvSPIComm::sendEndOfMaze() {
    buildHeader(PKT_END_OF_MAZE, 4);
    uint32_t* timestamp = (uint32_t*)tx_packet.payload;
    *timestamp = millis();

    return sendPacket();
}

// ============================================================================
// MDPS DATA FUNCTIONS
// ============================================================================

bool MarvSPIComm::sendWheelSpeeds(uint8_t vR, uint8_t vL, uint8_t setpoint) {
    WheelSpeedsPayload* payload = (WheelSpeedsPayload*)tx_packet.payload;
    buildHeader(PKT_WHEEL_SPEEDS, sizeof(WheelSpeedsPayload));

    payload->timestamp = millis();
    payload->vR = vR;
    payload->vL = vL;
    payload->vop_setpoint = setpoint;
    payload->reserved = 0;

    return sendPacket();
}

bool MarvSPIComm::sendDistance(uint16_t distance_mm) {
    DistancePayload* payload = (DistancePayload*)tx_packet.payload;
    buildHeader(PKT_DISTANCE, sizeof(DistancePayload));

    payload->timestamp = millis();
    payload->distance_mm = distance_mm;
    memset(payload->reserved, 0, 2);

    return sendPacket();
}

bool MarvSPIComm::sendRotationAngle(uint16_t angle, uint8_t direction) {
    RotationAnglePayload* payload = (RotationAnglePayload*)tx_packet.payload;
    buildHeader(PKT_ROTATION_ANGLE, sizeof(RotationAnglePayload));

    payload->timestamp = millis();
    payload->angle = angle;
    payload->direction = direction;
    payload->reserved = 0;

    return sendPacket();
}

// ============================================================================
// NAVCON FUNCTIONS
// ============================================================================

bool MarvSPIComm::sendLineDetection(Color color, uint8_t sensor, uint16_t angle,
                                   LineType line_type) {
    LineDetectionPayload* payload = (LineDetectionPayload*)tx_packet.payload;
    buildHeader(PKT_LINE_DETECTION, sizeof(LineDetectionPayload));

    payload->timestamp = millis();
    payload->color = (uint8_t)color;
    payload->first_sensor = sensor;
    payload->angle = angle;
    payload->line_type = (uint8_t)line_type;
    memset(payload->reserved, 0, 3);

    return sendPacket();
}

bool MarvSPIComm::sendNavconState(NavconState old_state, NavconState new_state,
                                 const char* reason) {
    NavconStatePayload* payload = (NavconStatePayload*)tx_packet.payload;
    buildHeader(PKT_NAVCON_STATE, sizeof(NavconStatePayload));

    payload->timestamp = millis();
    payload->old_state = (uint8_t)old_state;
    payload->new_state = (uint8_t)new_state;
    payload->reason_code = 0;

    strncpy(payload->reason_text, reason, 31);
    payload->reason_text[31] = '\0';

    return sendPacket();
}

bool MarvSPIComm::sendRotationCommand(uint16_t target_angle, uint8_t direction,
                                    uint16_t original_angle, uint16_t corrections) {
    RotationCommandPayload* payload = (RotationCommandPayload*)tx_packet.payload;
    buildHeader(PKT_ROTATION_COMMAND, sizeof(RotationCommandPayload));

    payload->timestamp = millis();
    payload->target_angle = target_angle;
    payload->direction = direction;
    payload->command_reason = 0;
    payload->original_angle = original_angle;
    payload->corrections_done = corrections;

    return sendPacket();
}

bool MarvSPIComm::sendRotationFeedback(uint16_t actual, uint16_t target) {
    RotationFeedbackPayload* payload = (RotationFeedbackPayload*)tx_packet.payload;
    buildHeader(PKT_ROTATION_FEEDBACK, sizeof(RotationFeedbackPayload));

    payload->timestamp = millis();
    payload->actual_angle = actual;
    payload->target_angle = target;
    payload->error = (int16_t)(actual - target);
    memset(payload->reserved, 0, 2);

    return sendPacket();
}

bool MarvSPIComm::sendAngleEvaluation(uint16_t original, uint16_t remaining,
                                     bool will_cross, uint8_t corrections, uint16_t threshold) {
    AngleEvaluationPayload* payload = (AngleEvaluationPayload*)tx_packet.payload;
    buildHeader(PKT_ANGLE_EVALUATION, sizeof(AngleEvaluationPayload));

    payload->timestamp = millis();
    payload->original_angle = original;
    payload->remaining_angle = remaining;
    payload->decision = will_cross ? 1 : 0;
    payload->correction_count = corrections;
    payload->threshold_used = threshold;

    return sendPacket();
}

// ============================================================================
// DEBUG FUNCTIONS
// ============================================================================

bool MarvSPIComm::sendDebug(uint8_t severity, const char* message) {
    DebugMessagePayload* payload = (DebugMessagePayload*)tx_packet.payload;
    buildHeader(PKT_DEBUG_MESSAGE, sizeof(DebugMessagePayload));

    payload->timestamp = millis();
    payload->severity = severity;

    strncpy(payload->message, message, 114);
    payload->message[114] = '\0';

    return sendPacket();
}

bool MarvSPIComm::sendHeartbeat() {
    buildHeader(PKT_HEARTBEAT, 4);
    uint32_t* uptime = (uint32_t*)tx_packet.payload;
    *uptime = millis();

    return sendPacket();
}

// ============================================================================
// PERFORMANCE MONITORING
// ============================================================================

void MarvSPIComm::printPerformanceStats() {
    Serial.println("\n--- SPI Communication Performance ---");
    Serial.printf("Total packets sent: %lu\n", packets_sent);
    Serial.printf("Current sequence: %d\n", sequence_counter);
    Serial.printf("SPI Speed: 2MHz\n");
    Serial.println("--------------------------------------\n");
}