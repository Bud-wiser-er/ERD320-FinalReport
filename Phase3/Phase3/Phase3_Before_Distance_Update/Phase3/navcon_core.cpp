#include "navcon_core.h"
#include "edge_case_matrix.h"

// ==================== CONSTANT DEFINITIONS ====================
// Define the constants that were declared as extern in the header
// Reverse distances based on angle (removed single constant, now calculated based on angle)
const uint16_t SENSOR_SPACING = 61;        // mm between S2 and S1/S3 (updated to 6.1cm)
const uint8_t STEERING_CORRECTION = 5;     // degrees for steering corrections
const uint8_t VOP_FORWARD = 10;            // forward speed mm/s (adjust as needed)

// Color constant definitions
const uint8_t WHITE = 0;
const uint8_t RED = 1;
const uint8_t GREEN = 2;
const uint8_t BLUE = 3;
const uint8_t BLACK = 4;

// ==================== GLOBAL VARIABLE DEFINITIONS ====================
// Define the global variables that were declared as extern in the header
uint8_t current_colors[3] = {WHITE, WHITE, WHITE};         // S1, S2, S3 current colors
uint8_t previous_colors[3] = {WHITE, WHITE, WHITE};        // Previous sensor readings
uint8_t received_incidence_angle = 0;                      // Raw angle from SS
uint8_t current_speed_left = 0;                            // Left wheel speed
uint8_t current_speed_right = 0;                           // Right wheel speed
uint16_t current_distance = 0;                             // Distance since last stop
uint16_t current_rotation = 0;                             // Last rotation executed
uint8_t current_rotation_dir = 0;                          // 2=left(CCW), 3=right(CW)
bool stop_confirmation_received = false;                    // indiactes if we have infact stopped proerply aftrer reversring
bool waiting_for_stop_confirmation = false;  

// Main NAVCON status instance
NavconStatus navcon_status;

// ==================== DATA STRUCTURE IMPLEMENTATIONS ====================
void LineDetectionData::reset() {
    detected_color = WHITE;
    detecting_sensor = 0;
    detection_start_distance = 0;
    initial_angle = 0;
    current_target_angle = 0;
    angle_valid = false;
    detection_active = false;
    line_type = LINE_NONE;
}

void CorrectionTracker::reset() {
    correction_direction = 0;
    attempts_made = 0;
    in_correction_sequence = false;
    last_rotation_commanded = 0;
    last_rotation_actual = 0;
    rotation_feedback_processed = false;
}

void BlackBlueNavigation::reset() {
    expecting_180_turn = false;
    first_black_blue_angle = 0;
    first_turn_completed = false;
}

void NavconStatus::reset() {
    current_state = NAVCON_FORWARD_SCAN;
    line_detection.reset();
    correction.reset();
    black_blue_nav.reset();
    stop_confirmed = false;
    reverse_confirmed = false;
    reverse_start_distance = 0;
}

// ==================== UTILITY FUNCTIONS ====================
bool isColorNavigable(uint8_t color) {
    return (color == RED || color == GREEN);
}

bool isColorWall(uint8_t color) {
    return (color == BLACK || color == BLUE);
}

bool sensorsAllWhite() {
    return (current_colors[0] == WHITE && 
            current_colors[1] == WHITE && 
            current_colors[2] == WHITE);
}

bool isMDPSStopped() {
    return (current_speed_left == 0 && current_speed_right == 0);
}

void printNavconState(const char* message) {
    const char* state_names[] = {
    "FORWARD_SCAN", "STOP", "REVERSE", "STOP_BEFORE_ROTATE", 
    "ROTATE", "EVALUATE_CORRECTION", "CROSSING_LINE"
    };
    Serial.printf("NAVCON [%s]: %s\n", state_names[navcon_status.current_state], message);
}

// ==================== FIXED LINE DETECTION IMPLEMENTATION ====================
void updateLineDetection() {
    LineDetectionData& detection = navcon_status.line_detection;

     // Serial.printf("DEBUG: updateLineDetection - active:%d, sensor:%d, angle_valid:%d\n",
     //            detection.detection_active, detection.detecting_sensor, detection.angle_valid);
    
    // Skip detection if we're already processing a line
    if (detection.detection_active) {
        return;
    }
    
    // Priority 1: S2 (center) detection - IMMEDIATE RESPONSE
    if (current_colors[1] != WHITE && current_colors[1] != previous_colors[1]) {
        detection.detected_color = current_colors[1];
        detection.detecting_sensor = 2;
        detection.initial_angle = received_incidence_angle;
        detection.current_target_angle = received_incidence_angle;
        detection.angle_valid = true;
        detection.detection_active = true;
        
        if (isColorNavigable(detection.detected_color)) {
            detection.line_type = LINE_RED_GREEN;
        } else if (isColorWall(detection.detected_color)) {
            detection.line_type = LINE_BLACK_BLUE;
        }
        
        // Serial.printf("S2 DETECTION: Color=%d, Angle=%d°, Type=%d\n",
        //              detection.detected_color, detection.initial_angle, detection.line_type);
        return;
    }
    
    // Priority 2: Check if ANY sensor has a non-white color for edge detection
    bool s1_detected = (current_colors[0] != WHITE);
    bool s2_detected = (current_colors[1] != WHITE);
    bool s3_detected = (current_colors[2] != WHITE);
    
    // If S1 and S2 both have color (like GREEN-GREEN-WHITE scenario)
    if (s1_detected && s2_detected && !detection.detection_active) {
        // This is likely a line at an angle - use S2's position as reference
        detection.detected_color = current_colors[1]; // Use S2 color
        detection.detecting_sensor = 1; // But mark as coming from left side
        detection.initial_angle = received_incidence_angle;
        detection.current_target_angle = received_incidence_angle;
        detection.angle_valid = true;
        detection.detection_active = true;
        
        if (isColorNavigable(detection.detected_color)) {
            detection.line_type = LINE_RED_GREEN;
        } else if (isColorWall(detection.detected_color)) {
            detection.line_type = LINE_BLACK_BLUE;
        }
        
        // Serial.printf("S1+S2 DETECTION: Color=%d, Angle=%d°, Type=%d\n",
        //              detection.detected_color, detection.initial_angle, detection.line_type);
        return;
    }
    
    // If S2 and S3 both have color
    if (s2_detected && s3_detected && !detection.detection_active) {
        detection.detected_color = current_colors[1]; // Use S2 color
        detection.detecting_sensor = 3; // Mark as coming from right side
        detection.initial_angle = received_incidence_angle;
        detection.current_target_angle = received_incidence_angle;
        detection.angle_valid = true;
        detection.detection_active = true;
        
        if (isColorNavigable(detection.detected_color)) {
            detection.line_type = LINE_RED_GREEN;
        } else if (isColorWall(detection.detected_color)) {
            detection.line_type = LINE_BLACK_BLUE;
        }
        
        // Serial.printf("S2+S3 DETECTION: Color=%d, Angle=%d°, Type=%d\n",
        //              detection.detected_color, detection.initial_angle, detection.line_type);
        return;
    }
    
    // Single edge sensor detection (original logic for edge cases)
    // Single edge sensor detection - start distance tracking
        if (s1_detected && !s2_detected && !detection.detection_active && detection.detecting_sensor == 0) {
            // Serial.printf("S1 EDGE DETECTION: Color=%d, starting distance tracking\n", current_colors[0]);
            detection.detected_color = current_colors[0];
            detection.detecting_sensor = 1;
            detection.detection_start_distance = current_distance;
            // Don't set detection_active yet - wait for confirmation or distance threshold
        }
        else if (s3_detected && !s2_detected && !detection.detection_active && detection.detecting_sensor == 0) {
            // Serial.printf("S3 EDGE DETECTION: Color=%d, starting distance tracking\n", current_colors[2]);
            detection.detected_color = current_colors[2];
            detection.detecting_sensor = 3;
            detection.detection_start_distance = current_distance;
            // Don't set detection_active yet - wait for confirmation or distance threshold
        }
    
    // Check for S2 confirmation after edge-only detection
    if (detection.detecting_sensor != 0 && !detection.detection_active && !detection.angle_valid) {
        if (current_colors[1] != WHITE) {
            detection.initial_angle = received_incidence_angle;
            detection.current_target_angle = received_incidence_angle;
            detection.angle_valid = true;
            detection.detection_active = true;
            
            if (isColorNavigable(detection.detected_color)) {
                detection.line_type = LINE_RED_GREEN;
            } else if (isColorWall(detection.detected_color)) {
                detection.line_type = LINE_BLACK_BLUE;
            }
            
            // Serial.printf("S2 CONFIRMED after edge: Color=%d, Angle=%d°, Type=%d\n",
            //              detection.detected_color, detection.initial_angle, detection.line_type);
        }
        else {
            // Check if we've traveled far enough to infer steep angle
            uint16_t travel_distance = current_distance - detection.detection_start_distance;
            // Serial.printf("DISTANCE CHECK: current=%d, start=%d, travel=%d, threshold=%d\n",
            //             current_distance, detection.detection_start_distance,
            //             travel_distance, SENSOR_SPACING);

            if (travel_distance >= SENSOR_SPACING) {
                // Serial.printf("STEEP ANGLE INFERRED: Sensor %d, Color=%d, Distance=%d\n",
                //             detection.detecting_sensor, detection.detected_color, travel_distance);
                
                detection.initial_angle = 46;  // Inferred > 45°
                detection.current_target_angle = 46;
                detection.angle_valid = false;  // Inferred, not measured
                detection.detection_active = true;  // NOW activate detection
                
                if (isColorNavigable(detection.detected_color)) {
                    detection.line_type = LINE_RED_GREEN;
                } else if (isColorWall(detection.detected_color)) {
                    detection.line_type = LINE_BLACK_BLUE;
                }

                // Serial.printf("STEEP ANGLE SETUP COMPLETE: Type=%d, Active=%d\n",
                //             detection.line_type, detection.detection_active);
            }
        }
    }
}


// ==================== CORRECTION PLANNING IMPLEMENTATION ====================
void planCorrectionForRedGreen() {
    LineDetectionData& detection = navcon_status.line_detection;
    CorrectionTracker& correction = navcon_status.correction;
    
    if (detection.current_target_angle <= 5) {
        // Safe to cross - no correction needed
        // Serial.printf("RED/GREEN: Safe to cross at %d°\n", detection.current_target_angle);
        navcon_status.current_state = NAVCON_CROSSING_LINE;
        navcon_status.black_blue_nav.expecting_180_turn = false;
        return;
    }
    
    // Calculate exact rotation needed to get to ≤5°
    uint16_t rotation_needed = 0;
    
    if (detection.current_target_angle <= 45) {
        // Need correction TOWARD the line - rotate to bring angle down to ~5°
        rotation_needed = detection.current_target_angle;
        
        // Determine correction direction based on which sensor detected
        if (detection.detecting_sensor == 1) {
            correction.correction_direction = 2;  // S1 detected -> turn LEFT toward line
        } else if (detection.detecting_sensor == 3) {
            correction.correction_direction = 3;  // S3 detected -> turn RIGHT toward line  
        } else {
            // S2 detection - use a default based on angle
            correction.correction_direction = 2;  // Default LEFT
        }
        
        // Serial.printf("RED/GREEN: ≤45° - correcting %d° %s toward line (angle: %d°)\n",
        //              rotation_needed,
        //              correction.correction_direction == 2 ? "LEFT" : "RIGHT",
        //              detection.current_target_angle);
    }
    else {
        // Angle > 45° - steer AWAY from line edge to reduce angle to ~40°
        rotation_needed =5;  // Bring it down to manageable angle
        
        // Steer AWAY from the line
        if (detection.detecting_sensor == 1) {
            correction.correction_direction = 2;  // S1 detected -> turn RIGHT AWAY from line
        } else if (detection.detecting_sensor == 3) {
            correction.correction_direction = 3;  // S3 detected -> turn LEFT AWAY from line
        } else {
            correction.correction_direction = 3;  // Default RIGHT
        }
        
        // Serial.printf("RED/GREEN: >45° - correcting %d° %s AWAY from line (angle: %d°)\n",
        //              rotation_needed,
        //              correction.correction_direction == 2 ? "LEFT" : "RIGHT",
        //              detection.current_target_angle);
    }
    
    // Store the planned rotation
    correction.in_correction_sequence = true;
    correction.last_rotation_commanded = rotation_needed;
    
    // Start correction sequence
    navcon_status.current_state = NAVCON_STOP;
}

void planCorrectionForBlackBlue() {
    LineDetectionData& detection = navcon_status.line_detection;
    CorrectionTracker& correction = navcon_status.correction;
    BlackBlueNavigation& bb_nav = navcon_status.black_blue_nav;
    
    // Safety check to prevent double-planning
    if (correction.in_correction_sequence && 
        (navcon_status.current_state == NAVCON_STOP || 
         navcon_status.current_state == NAVCON_REVERSE ||
         navcon_status.current_state == NAVCON_STOP_BEFORE_ROTATE)) {
        Serial.println("BLACK/BLUE: Already in correction sequence - skipping planning");
        return;
    }
    
    // Handle 180° turn case (second BLACK/BLUE detection)
    if (bb_nav.expecting_180_turn && detection.current_target_angle <= 45) {
        uint16_t rotation = 180;
        
        if (bb_nav.first_black_blue_angle > 0) {
            rotation = 180 - bb_nav.first_black_blue_angle;
        }
        
        correction.in_correction_sequence = true;
        correction.correction_direction = 2;  // LEFT for 180° turn
        correction.last_rotation_commanded = rotation;
        correction.rotation_feedback_processed = false;
        
        bb_nav.expecting_180_turn = false;
        bb_nav.first_turn_completed = true;
        
        // Serial.printf("BLACK/BLUE: 180° turn (%d°) - first angle was %d°\n",
        //              rotation, bb_nav.first_black_blue_angle);
        navcon_status.current_state = NAVCON_STOP;
        return;
    }
    
    // Handle normal angle cases (≤45°) - includes after steep angle corrections
    if (detection.current_target_angle <= 45) {
        uint16_t rotation = 90;
        
        if (detection.detecting_sensor == 1) {
            rotation = 90 - detection.current_target_angle;
        } else if (detection.detecting_sensor == 3) {
            rotation = 90 + detection.current_target_angle;
        }
        
        correction.in_correction_sequence = true;
        correction.correction_direction = 3;  // Always RIGHT turn for BLACK/BLUE
        correction.last_rotation_commanded = rotation;
        correction.rotation_feedback_processed = false;
        
        bb_nav.expecting_180_turn = true;
        bb_nav.first_black_blue_angle = 0;  // Reset for next sequence
        
        // Serial.printf("BLACK/BLUE: Normal angle - %d° RIGHT turn (sensor %d, angle %d°)\n",
        //              rotation, detection.detecting_sensor, detection.current_target_angle);
        navcon_status.current_state = NAVCON_STOP;
    }
    else {
        // Steep angle (>45°) handling - apply 5° corrections
        // Serial.printf("BLACK/BLUE: Steep angle detected (%d°) - applying 5° correction AWAY from wall\n",
        //              detection.current_target_angle);
        
        uint16_t rotation = STEERING_CORRECTION;  // 5°
        
        // Determine direction AWAY from the wall
        if (detection.detecting_sensor == 1) {
            correction.correction_direction = 3;  // S1 detected -> turn RIGHT AWAY from left wall
        } else if (detection.detecting_sensor == 3) {
            correction.correction_direction = 2;  // S3 detected -> turn LEFT AWAY from right wall
        } else {
            correction.correction_direction = 3;  // Default RIGHT
        }
        
        correction.in_correction_sequence = true;
        correction.last_rotation_commanded = rotation;
        correction.rotation_feedback_processed = false;
        
        // CRITICAL: Update the angle for next iteration
        detection.current_target_angle = detection.current_target_angle - 5;
        if (detection.current_target_angle <= 45) {
            // Ensure we trigger the 90° turn on the next call
            detection.current_target_angle = 45;
        }
        
        // Serial.printf("BLACK/BLUE: Steep angle - %d° %s away from wall (new target angle: %d°)\n",
        //              rotation,
        //              correction.correction_direction == 2 ? "LEFT" : "RIGHT",
        //              detection.current_target_angle);
        
        navcon_status.current_state = NAVCON_STOP;
    }
}


// ==================== PACKET CREATION IMPLEMENTATION ====================
SCSPacket createStopPacket() {
    
    SCSPacket packet;
    packet.control = createControlByte((SystemState)2, (SubsystemID)1, 3); // MAZE, SNC, IST=3
    packet.dat1 = 0;
    packet.dat0 = 0;
    packet.dec = 0;
    return packet;
}

SCSPacket createForwardPacket() {

    if (navcon_status.line_detection.line_type == LINE_BLACK_BLUE &&
        // either the original measured incidence or what we're still tracking is >45°
        (navcon_status.line_detection.initial_angle > 45 ||
         navcon_status.line_detection.current_target_angle > 45) &&
        !navcon_status.reverse_confirmed) {
        Serial.println("GUARD: FORWARD blocked (BLACK/BLUE >45° not yet reversed) → STOP");
        navcon_status.current_state = NAVCON_STOP;   // ensure the state machine stays in the STOP→REVERSE cadence
        return createStopPacket();
    }

    SCSPacket packet;
    packet.control = createControlByte((SystemState)2, (SubsystemID)1, 3); // MAZE, SNC, IST=3
    packet.dat1 = VOP_FORWARD;
    packet.dat0 = VOP_FORWARD;
    packet.dec = 0;
    return packet;
}

SCSPacket createReversePacket() {
    SCSPacket packet;
    packet.control = createControlByte((SystemState)2, (SubsystemID)1, 3); // MAZE, SNC, IST=3
    packet.dat1 = VOP_FORWARD;
    packet.dat0 = VOP_FORWARD;
    packet.dec = 1;  // Reverse direction
    return packet;
}

SCSPacket createRotatePacket(uint16_t angle, uint8_t direction) {
    SCSPacket packet;
    packet.control = createControlByte((SystemState)2, (SubsystemID)1, 3); // MAZE, SNC, IST=3
    packet.dat1 = (angle >> 8) & 0xFF;
    packet.dat0 = angle & 0xFF;
    packet.dec = direction;  // 2=LEFT, 3=RIGHT
    return packet;
}

// ==================== CORRECTED STATE MACHINE - NO STOP AFTER ROTATION ====================
SCSPacket executeNavconStateMachine() {
    printNavconState("Executing");
    
    switch (navcon_status.current_state) {
        case NAVCON_FORWARD_SCAN: {
            // Update line detection with edge case handling
            updateLineDetectionWithEdgeCases();
            
            // Check if we detected a line that needs processing
            if (navcon_status.line_detection.detection_active) {

                // Serial.printf("DEBUG: Line detected! Color=%d, Sensor=%d, Angle=%d°\n",
                //              navcon_status.line_detection.detected_color,
                //              navcon_status.line_detection.detecting_sensor,
                //              navcon_status.line_detection.current_target_angle);
                
                // Plan correction based on line type
                switch (navcon_status.line_detection.line_type) {
                    case LINE_RED_GREEN:
                        planCorrectionForRedGreen();
                        break;
                    case LINE_BLACK_BLUE:
                        planCorrectionForBlackBlue();
                        break;
                    default:
                        break;
                }
            }
            
            // If state changed to STOP, send stop packet
            if (navcon_status.current_state == NAVCON_STOP) {
                return createStopPacket();
            }
            
            // Otherwise continue forward
            return createForwardPacket();
        }
        
        case NAVCON_STOP: {
            // Wait for MDPS to confirm stop (speeds = 0)
            if (current_speed_left == 0 && current_speed_right == 0) {
                navcon_status.stop_confirmed = true;
                navcon_status.current_state = NAVCON_REVERSE;
                navcon_status.reverse_start_distance = current_distance;
                stop_confirmation_received = false;
                Serial.println("Stop confirmed - starting reverse");
                return createReversePacket();
            }
            
            // Keep sending stop until confirmed
            return createStopPacket();
        }
        
        case NAVCON_REVERSE: {
            // Calculate reverse distance based on angle
            uint16_t reverse_distance;
            if (navcon_status.line_detection.initial_angle > 45) {
                reverse_distance = 75;  // 75mm for steep angles (>45°)
            } else {
                reverse_distance = 60;  // 52mm for normal angles (≤45°)
            }

            // MDPS resets distance after stop, so current_distance IS the reverse distance
            if (current_distance >= reverse_distance) {
                navcon_status.reverse_confirmed = true;
                navcon_status.current_state = NAVCON_STOP_BEFORE_ROTATE;

                // Clear flag and set waiting status
                stop_confirmation_received = false;
                waiting_for_stop_confirmation = true;

                Serial.printf("Reverse complete (distance: %d mm, target: %d mm for angle %d°) - stopping before rotate\n",
                            current_distance, reverse_distance, navcon_status.line_detection.initial_angle);

                return createStopPacket();
            }

            // Continue reversing
            Serial.printf("Reversing... distance: %d mm (target: %d mm for angle %d°)\n",
                        current_distance, reverse_distance, navcon_status.line_detection.initial_angle);
            return createReversePacket();
        }

        case NAVCON_STOP_BEFORE_ROTATE: {
            // Check if we have confirmation
            if (stop_confirmation_received) {
                // Reset flag
                stop_confirmation_received = false;
                navcon_status.current_state = NAVCON_ROTATE;
                
                uint16_t rotation_amount = navcon_status.correction.last_rotation_commanded;
                uint8_t rotation_direction = navcon_status.correction.correction_direction;
                
                // Serial.printf("Stop confirmed - rotating %d° %s\n",
                //             rotation_amount,
                //             rotation_direction == 2 ? "LEFT" : "RIGHT");
                
                return createRotatePacket(rotation_amount, rotation_direction);
            }
            
            // Keep sending stop until confirmed
            Serial.println("Waiting for stop confirmation...");
            return createStopPacket();
        }

        case NAVCON_ROTATE: {
            // Safety check: Don't rotate if all sensors are white or if we have invalid rotation data
            if (sensorsAllWhite() || navcon_status.correction.last_rotation_commanded == 0 ||
                navcon_status.correction.last_rotation_commanded > 360) {
                Serial.printf("ERROR: Invalid rotation attempt (angle=%d, sensors WWW) - aborting\n",
                            navcon_status.correction.last_rotation_commanded);
                navcon_status.line_detection.reset();
                navcon_status.correction.reset();
                navcon_status.current_state = NAVCON_FORWARD_SCAN;
                return createForwardPacket();
            }
            
            navcon_status.current_state = NAVCON_EVALUATE_CORRECTION;
            // Fall through
        }
        
        // ==================== FIXED EVALUATION STATE ====================
        
        
        case NAVCON_CROSSING_LINE: {
            // Continue forward while crossing RED/GREEN
            if (sensorsAllWhite()) {
                Serial.println("Line crossing complete - resuming forward scan");
                navcon_status.line_detection.reset();
                navcon_status.correction.reset();
                navcon_status.black_blue_nav.reset();

                navcon_status.current_state = NAVCON_FORWARD_SCAN;
            }
            
            return createForwardPacket();
        }
        
        case NAVCON_EVALUATE_CORRECTION: {
            // Serial.printf("EVALUATE: Processing rotation feedback - commanded=%d°, actual=%d°\n",
            //             navcon_status.correction.last_rotation_commanded, current_rotation);
            
            // Check if this was a steep angle correction (5° incremental)
            if (navcon_status.correction.last_rotation_commanded == STEERING_CORRECTION) {
                int16_t rotation_difference = abs(navcon_status.correction.last_rotation_commanded - current_rotation);

                if (rotation_difference <= 5) {
                    // Serial.printf("EVALUATE: 5° steering correction verified (commanded=%d°, actual=%d°)\n",
                    //              navcon_status.correction.last_rotation_commanded, current_rotation);

                    // COMPLETE reset to ensure clean state for next detection
                    navcon_status.resetForNewDetection();

                    // Force return to forward scan
                    navcon_status.current_state = NAVCON_FORWARD_SCAN;
                    Serial.println("EVALUATE: 5° correction verified - resuming forward scan");
                    return createForwardPacket();
                } else {
                    // Rotation insufficient - need to correct
                    int16_t additional_rotation = navcon_status.correction.last_rotation_commanded - current_rotation;
                    Serial.printf("EVALUATE: 5° correction insufficient - need %d° more\n", additional_rotation);
                    navcon_status.correction.last_rotation_commanded = abs(additional_rotation);
                    navcon_status.current_state = NAVCON_STOP;
                    return createStopPacket();
                }
            }
            
            // Regular correction evaluation (RED/GREEN full angle corrections, BLACK/BLUE 90°/180°)
            if (navcon_status.line_detection.line_type == LINE_RED_GREEN) {
                int16_t rotation_difference = abs(navcon_status.correction.last_rotation_commanded - current_rotation);
                
                if (rotation_difference <= 5) {
                    Serial.println("RED/GREEN: Rotation sufficient - starting line crossing");
                    navcon_status.current_state = NAVCON_CROSSING_LINE;
                    return createForwardPacket();
                } else {
                    int16_t additional_rotation = navcon_status.correction.last_rotation_commanded - current_rotation;
                    Serial.printf("RED/GREEN: Rotation insufficient - need %d° more\n", additional_rotation);
                    navcon_status.correction.last_rotation_commanded = abs(additional_rotation);
                    navcon_status.current_state = NAVCON_STOP;
                    return createStopPacket();
                }
            }
            else if (navcon_status.line_detection.line_type == LINE_BLACK_BLUE) {
                // Serial.printf("BLACK/BLUE EVALUATE: %d° turn completed\n", current_rotation);
                
                // For 90°/180° turns, completely reset and continue forward
                navcon_status.resetForNewDetection();
                navcon_status.current_state = NAVCON_FORWARD_SCAN;
                Serial.println("BLACK/BLUE: Major turn complete - clean slate forward scan");
                return createForwardPacket();
            }
            
            // Fallback - complete reset
            Serial.println("EVALUATE: Unknown condition - performing complete reset");
            navcon_status.resetForNewDetection();
            navcon_status.current_state = NAVCON_FORWARD_SCAN;
            return createForwardPacket();
        }

    }
}

// ==================== PUBLIC INTERFACE FUNCTIONS ====================
void initializeNavcon() {
    navcon_status.reset();
    Serial.println("NAVCON System Initialized");
}

SCSPacket runEnhancedNavcon() {
    // This is called when it's NAVCON's turn (MAZE state, SNC IST=3)
    return executeNavconStateMachine();
}

void handleNavconIncomingData(const SCSPacket& packet) {
    // Extract subsystem and internal state from packet
    SystemState packetSysState = getSystemState(packet.control);
    SubsystemID packetSubsystem = getSubsystemID(packet.control);
    uint8_t packetIST = getInternalState(packet.control);
    
    // Only process MAZE state packets
    if (packetSysState != 2) { // MAZE = 2
        return;
    }
    
    // Store previous values before updating
    previous_colors[0] = current_colors[0];
    previous_colors[1] = current_colors[1];
    previous_colors[2] = current_colors[2];
    
    // Process based on which subsystem sent the packet
    switch (packetSubsystem) {
        case 3: // SUB_SS = 3
            if (packetIST == 1) {
                // SS Colors packet (MAZE state)
                uint16_t colorData = (packet.dat1 << 8) | packet.dat0;
                
                current_colors[0] = (colorData >> 6) & 0x07;  // Sensor 1
                current_colors[1] = (colorData >> 3) & 0x07;  // Sensor 2
                current_colors[2] = colorData & 0x07;          // Sensor 3

                // Serial.printf("NAVCON: Colors updated - S1:%d, S2:%d, S3:%d\n",
                //              current_colors[0], current_colors[1], current_colors[2]);
            }
            else if (packetIST == 2) {
                // SS Incidence Angle packet
                received_incidence_angle = packet.dat1;
                // Serial.printf("NAVCON: Received angle = %d°\n", received_incidence_angle);
            }
            else if (packetIST == 3) {
                // SS End-of-Maze detected
                Serial.println("NAVCON: End-of-Maze detected by SS!");
            }
            break;
            
        case 2: // SUB_MDPS = 2
            if (packetIST == 1) {
                // MDPS Battery/Level (we ignore this - just zeros)
            }
            else if (packetIST == 2) {
                // MDPS Rotation feedback
                current_rotation = (packet.dat1 << 8) | packet.dat0;
                current_rotation_dir = packet.dec;  // 2=left, 3=right
                // Serial.printf("NAVCON: Rotation completed - %d° %s\n",
                //              current_rotation,
                //              current_rotation_dir == 2 ? "LEFT" : "RIGHT");
            }
            else if (packetIST == 3) {
                // MDPS Speed feedback
                current_speed_right = packet.dat1;
                current_speed_left = packet.dat0;

                // Serial.printf("NAVCON: Speeds - L:%d mm/s, R:%d mm/s (State: %d)\n",
                //             current_speed_left, current_speed_right, navcon_status.current_state);
                
                // Only set confirmation if we're waiting for a stop
                if (navcon_status.current_state == NAVCON_STOP_BEFORE_ROTATE) {
                    if (current_speed_left == 0 && current_speed_right == 0) {
                        stop_confirmation_received = true;
                        Serial.println("NAVCON: Stop confirmed by MDPS - ready to rotate");
                    }
                }
            }
            else if (packetIST == 4) {
                // MDPS Distance feedback
                current_distance = (packet.dat1 << 8) | packet.dat0;
                // Serial.printf("NAVCON: Distance = %d mm\n", current_distance);
            }
            break;
            
        case 1: // SUB_SNC = 1
            // We don't need to process our own packets
            break;
    }
}

void printNavconDebugInfo() {
    Serial.println("\n=== NAVCON DEBUG STATUS ===");
    Serial.printf("State: %d | Colors: [%d,%d,%d]\n",
                 navcon_status.current_state,
                 current_colors[0], current_colors[1], current_colors[2]);
    Serial.printf("Line: Color=%d, InitialAngle=%d°, CurrentTargetAngle=%d°\n",
                 navcon_status.line_detection.detected_color,
                 navcon_status.line_detection.initial_angle,
                 navcon_status.line_detection.current_target_angle);
    Serial.printf("Correction: Direction=%s, Attempts=%d\n",
                 navcon_status.correction.correction_direction == 2 ? "LEFT" : "RIGHT",
                 navcon_status.correction.attempts_made);
    Serial.printf("Last Rotation: Commanded=%d°, Actual=%d°\n",
                 navcon_status.correction.last_rotation_commanded,
                 navcon_status.correction.last_rotation_actual);
    Serial.printf("Speeds: L=%d R=%d | Distance: %d | Angle: %d°\n",
                 current_speed_left, current_speed_right, current_distance, received_incidence_angle);
    Serial.println("============================\n");
}

void NavconStatus::resetForNewDetection() {
    // Complete reset of line detection
    line_detection.detected_color = WHITE;
    line_detection.detecting_sensor = 0;
    line_detection.detection_start_distance = 0;
    line_detection.initial_angle = 0;
    line_detection.current_target_angle = 0;
    line_detection.angle_valid = false;
    line_detection.detection_active = false;  // This is critical
    line_detection.line_type = LINE_NONE;
    
    // Complete reset of correction tracking
    correction.correction_direction = 0;
    correction.attempts_made = 0;
    correction.in_correction_sequence = false;
    correction.last_rotation_commanded = 0;
    correction.last_rotation_actual = 0;
    correction.rotation_feedback_processed = false;
    
    // Reset black/blue navigation state
    black_blue_nav.reset();

    // Reset confirmation flags
    stop_confirmed = false;
    reverse_confirmed = false;
    stop_confirmation_received = false;

    Serial.println("NAVCON: Complete state reset for new detection");
}