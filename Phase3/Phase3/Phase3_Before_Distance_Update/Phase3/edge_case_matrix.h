#ifndef EDGE_CASE_MATRIX_H
#define EDGE_CASE_MATRIX_H

#include <Arduino.h>

// Forward declarations to avoid circular includes
struct SCSPacket;
struct LineDetectionData;
struct NavconStatus;

// Color constants for edge case matrix (use different names to avoid conflicts)
#define EDGE_WHITE 0
#define EDGE_RED 1
#define EDGE_GREEN 2
#define EDGE_BLUE 3
#define EDGE_BLACK 4

// ==================== EDGE CASE DEFINITIONS ====================

// Edge case priority levels
enum EdgeCasePriority {
    PRIORITY_EMERGENCY = 0,    // Immediate action required
    PRIORITY_HIGH = 1,         // Important navigation decision
    PRIORITY_MEDIUM = 2,       // Standard line following
    PRIORITY_LOW = 3,          // Minor adjustments
    PRIORITY_IGNORE = 4        // No action needed
};

// Edge case action types
enum EdgeCaseAction {
    ACTION_FOLLOW_S1 = 1,      // Follow S1 detection
    ACTION_FOLLOW_S2 = 2,      // Follow S2 detection (center priority)
    ACTION_FOLLOW_S3 = 3,      // Follow S3 detection
    ACTION_FOLLOW_STRONGEST = 4, // Follow most significant color
    ACTION_AVERAGE_ANGLE = 5,   // Use average of multiple sensors
    ACTION_EMERGENCY_STOP = 6,  // Immediate stop required
    ACTION_IGNORE_ALL = 7,      // Continue forward, ignore sensors
    ACTION_BACKUP_FIRST = 8     // Reverse before taking action
};

// Edge case sensor combination structure
struct EdgeCaseRule {
    uint8_t s1_color;          // Sensor 1 color (0-4, 255=ANY)
    uint8_t s2_color;          // Sensor 2 color (0-4, 255=ANY)
    uint8_t s3_color;          // Sensor 3 color (0-4, 255=ANY)
    EdgeCasePriority priority; // Priority level
    EdgeCaseAction action;     // Action to take
    uint8_t primary_sensor;    // Which sensor to use for angle (1-3)
    const char* description;   // Human-readable description
};

// Special color codes
#define ANY_COLOR 255
#define SAME_AS_S2 254

// ==================== COMPREHENSIVE EDGE CASE MATRIX ====================

const EdgeCaseRule EDGE_CASE_MATRIX[] = {

    // ==================== EMERGENCY CASES (Priority 0) ====================
    // Multiple conflicting navigation lines - EMERGENCY STOP
    {EDGE_RED,   EDGE_GREEN, EDGE_BLACK, PRIORITY_EMERGENCY, ACTION_EMERGENCY_STOP, 2, "RED-GREEN-BLACK conflict"},
    {EDGE_GREEN, EDGE_RED,   EDGE_BLUE,  PRIORITY_EMERGENCY, ACTION_EMERGENCY_STOP, 2, "GREEN-RED-BLUE conflict"},
    {EDGE_RED,   EDGE_BLUE,  EDGE_GREEN, PRIORITY_EMERGENCY, ACTION_EMERGENCY_STOP, 2, "RED-BLUE-GREEN conflict"},
    {EDGE_BLACK, EDGE_RED,   EDGE_GREEN, PRIORITY_EMERGENCY, ACTION_EMERGENCY_STOP, 2, "BLACK-RED-GREEN conflict"},

    // All sensors same non-white color (potential error state)
    {EDGE_RED,   EDGE_RED,   EDGE_RED,   PRIORITY_EMERGENCY, ACTION_EMERGENCY_STOP, 2, "All RED detected"},
    {EDGE_GREEN, EDGE_GREEN, EDGE_GREEN, PRIORITY_EMERGENCY, ACTION_EMERGENCY_STOP, 2, "All GREEN detected"},
    {EDGE_BLUE,  EDGE_BLUE,  EDGE_BLUE,  PRIORITY_EMERGENCY, ACTION_EMERGENCY_STOP, 2, "All BLUE detected"},
    {EDGE_BLACK, EDGE_BLACK, EDGE_BLACK, PRIORITY_EMERGENCY, ACTION_EMERGENCY_STOP, 2, "All BLACK detected"},

    // ==================== HIGH PRIORITY CASES (Priority 1) ====================
    // Center sensor (S2) always takes precedence when active
    {ANY_COLOR, EDGE_RED,   ANY_COLOR, PRIORITY_HIGH, ACTION_FOLLOW_S2, 2, "S2 RED priority"},
    {ANY_COLOR, EDGE_GREEN, ANY_COLOR, PRIORITY_HIGH, ACTION_FOLLOW_S2, 2, "S2 GREEN priority"},
    {ANY_COLOR, EDGE_BLACK, ANY_COLOR, PRIORITY_HIGH, ACTION_FOLLOW_S2, 2, "S2 BLACK priority"},
    {ANY_COLOR, EDGE_BLUE,  ANY_COLOR, PRIORITY_HIGH, ACTION_FOLLOW_S2, 2, "S2 BLUE priority"},

    // Your specific example: S1=BLACK, S2=WHITE, S3=GREEN
    {EDGE_BLACK, EDGE_WHITE, EDGE_GREEN, PRIORITY_HIGH, ACTION_FOLLOW_S3, 3, "BLACK-WHITE-GREEN: follow S3 GREEN"},
    {EDGE_GREEN, EDGE_WHITE, EDGE_BLACK, PRIORITY_HIGH, ACTION_FOLLOW_S1, 1, "GREEN-WHITE-BLACK: follow S1 GREEN"},

    // Wall avoidance with navigation line visible
    {EDGE_BLACK, EDGE_WHITE, EDGE_GREEN, PRIORITY_HIGH, ACTION_FOLLOW_S3, 3, "Avoid BLACK wall, follow GREEN"},
    {EDGE_GREEN, EDGE_WHITE, EDGE_BLACK, PRIORITY_HIGH, ACTION_FOLLOW_S1, 1, "Follow GREEN, avoid BLACK wall"},
    {EDGE_BLUE,  EDGE_WHITE, EDGE_GREEN, PRIORITY_HIGH, ACTION_FOLLOW_S3, 3, "Avoid BLUE wall, follow GREEN"},
    {EDGE_GREEN, EDGE_WHITE, EDGE_BLUE,  PRIORITY_HIGH, ACTION_FOLLOW_S1, 1, "Follow GREEN, avoid BLUE wall"},
    {EDGE_BLACK, EDGE_WHITE, EDGE_RED,   PRIORITY_HIGH, ACTION_FOLLOW_S3, 3, "Avoid BLACK wall, follow RED"},
    {EDGE_RED,   EDGE_WHITE, EDGE_BLACK, PRIORITY_HIGH, ACTION_FOLLOW_S1, 1, "Follow RED, avoid BLACK wall"},
    {EDGE_BLUE,  EDGE_WHITE, EDGE_RED,   PRIORITY_HIGH, ACTION_FOLLOW_S3, 3, "Avoid BLUE wall, follow RED"},
    {EDGE_RED,   EDGE_WHITE, EDGE_BLUE,  PRIORITY_HIGH, ACTION_FOLLOW_S1, 1, "Follow RED, avoid BLUE wall"},

    // ==================== MEDIUM PRIORITY CASES (Priority 2) ====================
    // Two adjacent sensors with same navigable color
    {EDGE_RED,   EDGE_RED,   EDGE_WHITE, PRIORITY_MEDIUM, ACTION_AVERAGE_ANGLE, 1, "S1-S2 RED line"},
    {EDGE_GREEN, EDGE_GREEN, EDGE_WHITE, PRIORITY_MEDIUM, ACTION_AVERAGE_ANGLE, 1, "S1-S2 GREEN line"},
    {EDGE_WHITE, EDGE_RED,   EDGE_RED,   PRIORITY_MEDIUM, ACTION_AVERAGE_ANGLE, 3, "S2-S3 RED line"},
    {EDGE_WHITE, EDGE_GREEN, EDGE_GREEN, PRIORITY_MEDIUM, ACTION_AVERAGE_ANGLE, 3, "S2-S3 GREEN line"},

    // Two adjacent sensors with same wall color
    {EDGE_BLACK, EDGE_BLACK, EDGE_WHITE, PRIORITY_MEDIUM, ACTION_FOLLOW_STRONGEST, 1, "S1-S2 BLACK wall"},
    {EDGE_BLUE,  EDGE_BLUE,  EDGE_WHITE, PRIORITY_MEDIUM, ACTION_FOLLOW_STRONGEST, 1, "S1-S2 BLUE wall"},
    {EDGE_WHITE, EDGE_BLACK, EDGE_BLACK, PRIORITY_MEDIUM, ACTION_FOLLOW_STRONGEST, 3, "S2-S3 BLACK wall"},
    {EDGE_WHITE, EDGE_BLUE,  EDGE_BLUE,  PRIORITY_MEDIUM, ACTION_FOLLOW_STRONGEST, 3, "S2-S3 BLUE wall"},

    // Single sensor active cases (edge detection)
    {EDGE_RED,   EDGE_WHITE, EDGE_WHITE, PRIORITY_MEDIUM, ACTION_FOLLOW_S1, 1, "S1 RED edge detection"},
    {EDGE_GREEN, EDGE_WHITE, EDGE_WHITE, PRIORITY_MEDIUM, ACTION_FOLLOW_S1, 1, "S1 GREEN edge detection"},
    {EDGE_BLACK, EDGE_WHITE, EDGE_WHITE, PRIORITY_MEDIUM, ACTION_FOLLOW_S1, 1, "S1 BLACK edge detection"},
    {EDGE_BLUE,  EDGE_WHITE, EDGE_WHITE, PRIORITY_MEDIUM, ACTION_FOLLOW_S1, 1, "S1 BLUE edge detection"},

    {EDGE_WHITE, EDGE_WHITE, EDGE_RED,   PRIORITY_MEDIUM, ACTION_FOLLOW_S3, 3, "S3 RED edge detection"},
    {EDGE_WHITE, EDGE_WHITE, EDGE_GREEN, PRIORITY_MEDIUM, ACTION_FOLLOW_S3, 3, "S3 GREEN edge detection"},
    {EDGE_WHITE, EDGE_WHITE, EDGE_BLACK, PRIORITY_MEDIUM, ACTION_FOLLOW_S3, 3, "S3 BLACK edge detection"},
    {EDGE_WHITE, EDGE_WHITE, EDGE_BLUE,  PRIORITY_MEDIUM, ACTION_FOLLOW_S3, 3, "S3 BLUE edge detection"},

    // Mixed wall colors (choose safer path)
    {EDGE_BLACK, EDGE_WHITE, EDGE_BLUE,  PRIORITY_MEDIUM, ACTION_IGNORE_ALL, 2, "Between walls - continue straight"},
    {EDGE_BLUE,  EDGE_WHITE, EDGE_BLACK, PRIORITY_MEDIUM, ACTION_IGNORE_ALL, 2, "Between walls - continue straight"},

    // ==================== LOW PRIORITY CASES (Priority 3) ====================
    // All white (normal forward operation)
    {EDGE_WHITE, EDGE_WHITE, EDGE_WHITE, PRIORITY_LOW, ACTION_IGNORE_ALL, 2, "All white - normal forward"},

    // Non-critical single sensor noise
    {EDGE_WHITE, EDGE_RED,   EDGE_WHITE, PRIORITY_LOW, ACTION_FOLLOW_S2, 2, "S2 RED single sensor"},
    {EDGE_WHITE, EDGE_GREEN, EDGE_WHITE, PRIORITY_LOW, ACTION_FOLLOW_S2, 2, "S2 GREEN single sensor"},
    {EDGE_WHITE, EDGE_BLACK, EDGE_WHITE, PRIORITY_LOW, ACTION_FOLLOW_S2, 2, "S2 BLACK single sensor"},
    {EDGE_WHITE, EDGE_BLUE,  EDGE_WHITE, PRIORITY_LOW, ACTION_FOLLOW_S2, 2, "S2 BLUE single sensor"},

    // ==================== END MARKER ====================
    {255, 255, 255, PRIORITY_IGNORE, ACTION_IGNORE_ALL, 0, "END_OF_MATRIX"}
};

// ==================== FUNCTION DECLARATIONS ====================

/**
 * Find the best matching edge case rule for current sensor readings
 */
const EdgeCaseRule* findEdgeCaseRule(uint8_t s1_color, uint8_t s2_color, uint8_t s3_color);

/**
 * Apply the edge case rule to current NAVCON state
 */
bool applyEdgeCaseRule(const EdgeCaseRule* rule, uint8_t current_angle);

/**
 * Get priority level name for debugging
 */
const char* getPriorityName(EdgeCasePriority priority);

/**
 * Get action name for debugging
 */
const char* getActionName(EdgeCaseAction action);

/**
 * Print edge case analysis for debugging
 */
void printEdgeCaseAnalysis(uint8_t s1_color, uint8_t s2_color, uint8_t s3_color);

/**
 * Enhanced line detection with comprehensive edge case handling
 */
void updateLineDetectionWithEdgeCases();

#endif // EDGE_CASE_MATRIX_H