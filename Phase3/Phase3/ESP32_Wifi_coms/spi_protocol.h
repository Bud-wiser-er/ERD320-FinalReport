/*
 * spi_protocol.h
 * Complete SPI Communication Protocol for MARV
 * WiFi ESP32 Version - Simplified for receiving only
 */

#ifndef SPI_PROTOCOL_H
#define SPI_PROTOCOL_H

#include <Arduino.h>
#include <SPI.h>

// ============================================================================
// SYSTEM ENUMS (From Your Project)
// ============================================================================

enum SystemState : uint8_t {
    SYS_IDLE = 0,
    SYS_CAL = 1,
    SYS_MAZE = 2,
    SYS_SOS = 3
};

enum Subsystem : uint8_t {
    SUB_HUB = 0,
    SUB_SNC = 1,
    SUB_MDPS = 2,
    SUB_SS = 3
};

enum NavconState : uint8_t {
    NAVCON_FORWARD_SCAN = 0,
    NAVCON_STOP = 1,
    NAVCON_REVERSE = 2,
    NAVCON_STOP_BEFORE_ROTATE = 3,
    NAVCON_ROTATE = 4,
    NAVCON_EVALUATE_CORRECTION = 5,
    NAVCON_CROSSING_LINE = 6
};

enum Color : uint8_t {
    COLOR_WHITE = 0,
    COLOR_RED = 1,
    COLOR_GREEN = 2,
    COLOR_BLUE = 3,
    COLOR_BLACK = 4
};

enum LineType : uint8_t {
    LINE_NAVIGABLE = 0,
    LINE_WALL = 1
};

// ============================================================================
// PACKET TYPES
// ============================================================================

enum PacketType : uint8_t {
    PKT_SYSTEM_STATE = 0x01,
    PKT_TOUCH_DETECTED = 0x02,
    PKT_PURE_TONE = 0x03,
    PKT_SENSOR_COLORS = 0x10,
    PKT_INCIDENCE_ANGLE = 0x11,
    PKT_END_OF_MAZE = 0x12,
    PKT_WHEEL_SPEEDS = 0x20,
    PKT_DISTANCE = 0x21,
    PKT_ROTATION_ANGLE = 0x22,
    PKT_LINE_DETECTION = 0x30,
    PKT_NAVCON_STATE = 0x31,
    PKT_ROTATION_COMMAND = 0x32,
    PKT_ROTATION_FEEDBACK = 0x33,
    PKT_ANGLE_EVALUATION = 0x34,
    PKT_DEBUG_MESSAGE = 0x40,
    PKT_HEARTBEAT = 0x42
};

// ============================================================================
// PACKET STRUCTURES
// ============================================================================

struct SPIPacketHeader {
    uint8_t sync1;              // 0xAA
    uint8_t sync2;              // 0x55
    uint8_t packet_type;
    uint8_t data_length;
    uint16_t sequence;
    uint8_t flags;
    uint8_t checksum_header;
} __attribute__((packed));

#define MAX_PAYLOAD_SIZE 248

struct SPIPacket {
    SPIPacketHeader header;
    uint8_t payload[MAX_PAYLOAD_SIZE];
    uint8_t checksum_payload;
} __attribute__((packed));

// Payload Structures
struct SystemStatePayload {
    uint32_t timestamp;
    uint8_t system_state;
    uint8_t subsystem;
    uint8_t internal_state;
    uint8_t reserved;
} __attribute__((packed));

struct TouchPayload {
    uint32_t timestamp;
    uint8_t touch_detected;
    uint8_t system_state;
    uint16_t vop_designed;
} __attribute__((packed));

struct PureTonePayload {
    uint32_t timestamp;
    uint8_t tone_detected;
    uint16_t frequency;
    uint8_t dB_level;
} __attribute__((packed));

struct SensorColorsPayload {
    uint32_t timestamp;
    uint8_t sensor1_color;
    uint8_t sensor2_color;
    uint8_t sensor3_color;
    uint8_t reserved;
} __attribute__((packed));

struct IncidenceAnglePayload {
    uint32_t timestamp;
    uint16_t angle;
    uint8_t first_sensor;
    uint8_t second_sensor;
    uint8_t sensors_used;
    uint8_t reserved[3];
} __attribute__((packed));

struct WheelSpeedsPayload {
    uint32_t timestamp;
    uint8_t vR;
    uint8_t vL;
    uint8_t vop_setpoint;
    uint8_t reserved;
} __attribute__((packed));

struct DistancePayload {
    uint32_t timestamp;
    uint16_t distance_mm;
    uint8_t reserved[2];
} __attribute__((packed));

struct RotationAnglePayload {
    uint32_t timestamp;
    uint16_t angle;
    uint8_t direction;
    uint8_t reserved;
} __attribute__((packed));

struct LineDetectionPayload {
    uint32_t timestamp;
    uint8_t color;
    uint8_t first_sensor;
    uint16_t angle;
    uint8_t line_type;
    uint8_t reserved[3];
} __attribute__((packed));

struct NavconStatePayload {
    uint32_t timestamp;
    uint8_t old_state;
    uint8_t new_state;
    uint16_t reason_code;
    char reason_text[32];
} __attribute__((packed));

struct RotationCommandPayload {
    uint32_t timestamp;
    uint16_t target_angle;
    uint8_t direction;
    uint8_t command_reason;
    uint16_t original_angle;
    uint16_t corrections_done;
} __attribute__((packed));

struct RotationFeedbackPayload {
    uint32_t timestamp;
    uint16_t actual_angle;
    uint16_t target_angle;
    int16_t error;
    uint8_t reserved[2];
} __attribute__((packed));

struct AngleEvaluationPayload {
    uint32_t timestamp;
    uint16_t original_angle;
    uint16_t remaining_angle;
    uint8_t decision;
    uint8_t correction_count;
    uint16_t threshold_used;
} __attribute__((packed));

struct DebugMessagePayload {
    uint32_t timestamp;
    uint8_t severity;
    char message[115];
} __attribute__((packed));

#endif // SPI_PROTOCOL_H