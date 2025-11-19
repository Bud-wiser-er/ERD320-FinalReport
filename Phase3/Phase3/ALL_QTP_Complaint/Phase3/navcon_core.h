#ifndef NAVCON_CORE_H
#define NAVCON_CORE_H

#include <Arduino.h>
#include "scs_protocol.h"  // Include SCS protocol instead of redefining

// ==================== NAVCON CONSTANTS ====================
extern const uint16_t REVERSE_DISTANCE;      // mm to reverse
extern const uint16_t SENSOR_SPACING;        // mm between S2 and S1/S3  
extern const uint8_t STEERING_CORRECTION;    // degrees for corrections
extern const uint8_t VOP_FORWARD;            // forward speed mm/s

// Color Constants
extern const uint8_t WHITE;
extern const uint8_t RED;
extern const uint8_t GREEN;
extern const uint8_t BLUE;
extern const uint8_t BLACK;

// ==================== NAVCON ENUMS ====================
enum NavconState {
    NAVCON_FORWARD_SCAN = 0,      // Scanning for lines while moving forward
    NAVCON_STOP,                  // Execute stop command
    NAVCON_REVERSE,               // Execute reverse command
    NAVCON_STOP_BEFORE_ROTATE,
    NAVCON_ROTATE,                // Execute rotation command
    NAVCON_EVALUATE_CORRECTION,   // Check if more corrections needed
    NAVCON_CROSSING_LINE         // Currently crossing a safe line (≤ 5°)
};

enum LineType {
    LINE_NONE = 0,
    LINE_RED_GREEN,    // Navigable lines
    LINE_BLACK_BLUE    // Wall lines
};

// ==================== NAVCON DATA STRUCTURES ====================
struct LineDetectionData {
    uint8_t detected_color;              // Color of detected line
    uint8_t detecting_sensor;            // Which sensor detected first (1, 2, or 3)
    uint16_t detection_start_distance;   // Distance when first detected
    uint8_t initial_angle;              // First measured/inferred angle
    uint8_t current_target_angle;       // Target angle we're trying to achieve
    bool angle_valid;                   // True if angle is measured (not inferred)
    bool detection_active;              // True if we're processing a detection
    LineType line_type;                 // What type of line we're dealing with
    
    void reset();
};

struct CorrectionTracker {
    uint8_t correction_direction;       // 2=LEFT, 3=RIGHT
    uint8_t attempts_made;              // Number of correction attempts
    bool in_correction_sequence;       // Are we in the middle of corrections?
    uint16_t last_rotation_commanded;   // What we asked MDPS to rotate
    uint16_t last_rotation_actual;      // What MDPS actually rotated
    bool rotation_feedback_processed;
    void reset();
};

struct BlackBlueNavigation {
    bool expecting_180_turn;           // For BLACK/BLUE navigation
    uint8_t first_black_blue_angle;    // Store angle for 180° calculation
    bool first_turn_completed;         // Track if we did the first 90° turn
    
    void reset();
};

struct NavconStatus {
    NavconState current_state;
    LineDetectionData line_detection;
    CorrectionTracker correction;
    BlackBlueNavigation black_blue_nav;
    
    // Motion confirmation tracking
    bool stop_confirmed;
    bool reverse_confirmed;
    uint16_t reverse_start_distance;
    
    void reset();
    void resetForNewDetection();
};

// ==================== GLOBAL NAVCON VARIABLES ====================
// These are updated by incoming SS/MDPS packets
extern uint8_t current_colors[3];          // S1, S2, S3 current colors
extern uint8_t previous_colors[3];         // Previous sensor readings  
extern uint8_t received_incidence_angle;   // Raw angle from SS
extern uint8_t current_speed_left;         // Left wheel speed
extern uint8_t current_speed_right;        // Right wheel speed
extern uint16_t current_distance;          // Distance since last stop
extern uint16_t current_rotation;          // Last rotation executed
extern uint8_t current_rotation_dir;       // 2=left(CCW), 3=right(CW)

// Main NAVCON status
extern NavconStatus navcon_status;

// ==================== MAIN NAVCON FUNCTIONS ====================
/**
 * Initialize the NAVCON system
 */
void initializeNavcon();

/**
 * Main NAVCON function - called when it's NAVCON's turn in SCS protocol
 * Returns the SCS packet to send to MDPS/SS
 */
SCSPacket runEnhancedNavcon();

/**
 * Process incoming packets from SS and MDPS to update NAVCON variables
 * Called for every received packet in MAZE state
 */
void handleNavconIncomingData(const SCSPacket& packet);

/**
 * Print detailed NAVCON debug information
 */
void printNavconDebugInfo();

// ==================== NAVCON UTILITY FUNCTIONS ====================
/**
 * Check if color is navigable (RED/GREEN)
 */
bool isColorNavigable(uint8_t color);

/**
 * Check if color is a wall (BLACK/BLUE)
 */
bool isColorWall(uint8_t color);

/**
 * Check if all sensors are reading WHITE
 */
bool sensorsAllWhite();

/**
 * Check if MDPS has stopped (both wheels at 0 speed)
 */
bool isMDPSStopped();

/**
 * Print current NAVCON state with message
 */
void printNavconState(const char* message);

// ==================== LINE DETECTION FUNCTIONS ====================
/**
 * Update line detection based on current sensor readings
 * Handles S1/S3 -> S2 confirmation and steep angle inference
 */
void updateLineDetection();

// ==================== CORRECTION PLANNING FUNCTIONS ====================
/**
 * Plan correction strategy for RED/GREEN lines
 * Handles ≤5° (cross), ≤45° (toward), >45° (away) cases
 */
void planCorrectionForRedGreen();

/**
 * Plan correction strategy for BLACK/BLUE lines  
 * Handles 90° turns and 180° turn scenarios
 */
void planCorrectionForBlackBlue();

// ==================== PACKET CREATION FUNCTIONS ====================
/**
 * Create STOP packet (speed = 0)
 */
SCSPacket createStopPacket();

/**
 * Create FORWARD packet (normal forward speed)
 */
SCSPacket createForwardPacket();

/**
 * Create REVERSE packet (reverse direction)
 */
SCSPacket createReversePacket();

/**
 * Create ROTATE packet with specified angle and direction
 * @param angle: Rotation angle in degrees
 * @param direction: 2=LEFT, 3=RIGHT
 */
SCSPacket createRotatePacket(uint16_t angle, uint8_t direction);

#endif // NAVCON_CORE_H