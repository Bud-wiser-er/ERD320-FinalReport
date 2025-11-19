/*
 * edge_case_matrix.cpp
 * Updated Edge Case Handling for MARV Navigation System
 *
 * This implementation matches the CURRENT NAVCON logic:
 * 1. S2 (center) has ABSOLUTE priority - immediate response
 * 2. S1+S2 or S2+S3 = line at angle, uses S2's color
 * 3. S1 or S3 alone = distance tracking mode
 * 4. After 50mm without S2 confirmation = infer steep angle (>45°)
 *
 * The EDGE_CASE_MATRIX rules are available but not actively used.
 * The updateLineDetectionWithEdgeCases() function implements the
 * proven NAVCON algorithm with enhanced debug output.
 */

#include "edge_case_matrix.h"
#include "navcon_core.h"

// External variable declarations (defined in navcon_core.cpp)
extern uint8_t current_colors[3];
extern uint8_t previous_colors[3];
extern uint8_t received_incidence_angle;
extern NavconStatus navcon_status;
extern uint16_t current_distance;
extern const uint16_t SENSOR_SPACING;

// External color constants (defined in navcon_core.cpp)
extern const uint8_t WHITE;
extern const uint8_t RED;
extern const uint8_t GREEN;
extern const uint8_t BLUE;
extern const uint8_t BLACK;

// External function declarations (defined in navcon_core.cpp)
extern bool isColorNavigable(uint8_t color);
extern bool isColorWall(uint8_t color);

// ==================== EDGE CASE RULE MATCHING ====================

const EdgeCaseRule* findEdgeCaseRule(uint8_t s1_color, uint8_t s2_color, uint8_t s3_color) {
    Serial.printf("EDGE_CASE: Searching for rule - S1:%d, S2:%d, S3:%d\n", s1_color, s2_color, s3_color);

    // Iterate through the edge case matrix
    for (int i = 0; EDGE_CASE_MATRIX[i].s1_color != 255; i++) {
        const EdgeCaseRule* rule = &EDGE_CASE_MATRIX[i];

        // Check if this rule matches current sensor readings
        bool s1_match = (rule->s1_color == ANY_COLOR) || (rule->s1_color == s1_color);
        bool s2_match = (rule->s2_color == ANY_COLOR) || (rule->s2_color == s2_color);
        bool s3_match = (rule->s3_color == ANY_COLOR) || (rule->s3_color == s3_color);

        // Special case: SAME_AS_S2 handling
        if (rule->s1_color == SAME_AS_S2) {
            s1_match = (s1_color == s2_color);
        }
        if (rule->s3_color == SAME_AS_S2) {
            s3_match = (s3_color == s2_color);
        }

        if (s1_match && s2_match && s3_match) {
            Serial.printf("EDGE_CASE: Rule found [%d] - %s (Priority: %s, Action: %s)\n",
                         i, rule->description,
                         getPriorityName(rule->priority),
                         getActionName(rule->action));
            return rule;
        }
    }

    Serial.println("EDGE_CASE: No specific rule found - using default behavior");
    return nullptr;
}

// ==================== EDGE CASE RULE APPLICATION ====================

bool applyEdgeCaseRule(const EdgeCaseRule* rule, uint8_t current_angle) {
    if (!rule) {
        Serial.println("EDGE_CASE: No rule to apply");
        return false;
    }

    LineDetectionData& detection = navcon_status.line_detection;

    Serial.printf("EDGE_CASE: Applying rule - %s\n", rule->description);

    switch (rule->action) {
        case ACTION_FOLLOW_S1: {
            detection.detected_color = current_colors[0];
            detection.detecting_sensor = 1;
            detection.initial_angle = current_angle;
            detection.current_target_angle = current_angle;
            detection.angle_valid = true;
            detection.detection_active = true;

            if (isColorNavigable(detection.detected_color)) {
                detection.line_type = LINE_RED_GREEN;
            } else if (isColorWall(detection.detected_color)) {
                detection.line_type = LINE_BLACK_BLUE;
            }

            Serial.printf("EDGE_CASE: Following S1 - Color:%d, Angle:%d°\n",
                         detection.detected_color, current_angle);
            return true;
        }

        case ACTION_FOLLOW_S2: {
            detection.detected_color = current_colors[1];
            detection.detecting_sensor = 2;
            detection.initial_angle = current_angle;
            detection.current_target_angle = current_angle;
            detection.angle_valid = true;
            detection.detection_active = true;

            if (isColorNavigable(detection.detected_color)) {
                detection.line_type = LINE_RED_GREEN;
            } else if (isColorWall(detection.detected_color)) {
                detection.line_type = LINE_BLACK_BLUE;
            }

            Serial.printf("EDGE_CASE: Following S2 - Color:%d, Angle:%d°\n",
                         detection.detected_color, current_angle);
            return true;
        }

        case ACTION_FOLLOW_S3: {
            detection.detected_color = current_colors[2];
            detection.detecting_sensor = 3;
            detection.initial_angle = current_angle;
            detection.current_target_angle = current_angle;
            detection.angle_valid = true;
            detection.detection_active = true;

            if (isColorNavigable(detection.detected_color)) {
                detection.line_type = LINE_RED_GREEN;
            } else if (isColorWall(detection.detected_color)) {
                detection.line_type = LINE_BLACK_BLUE;
            }

            Serial.printf("EDGE_CASE: Following S3 - Color:%d, Angle:%d°\n",
                         detection.detected_color, current_angle);
            return true;
        }

        case ACTION_FOLLOW_STRONGEST: {
            // Determine which sensor has the "strongest" signal
            // Priority: RED > GREEN > BLACK > BLUE > WHITE
            uint8_t strongest_color = WHITE;
            uint8_t strongest_sensor = 2; // Default to center

            for (int i = 0; i < 3; i++) {
                uint8_t color = current_colors[i];
                if (color == RED ||
                    (color == GREEN && strongest_color != RED) ||
                    (color == BLACK && strongest_color != RED && strongest_color != GREEN) ||
                    (color == BLUE && strongest_color == WHITE)) {
                    strongest_color = color;
                    strongest_sensor = i + 1;
                }
            }

            detection.detected_color = strongest_color;
            detection.detecting_sensor = strongest_sensor;
            detection.initial_angle = current_angle;
            detection.current_target_angle = current_angle;
            detection.angle_valid = true;
            detection.detection_active = true;

            if (isColorNavigable(detection.detected_color)) {
                detection.line_type = LINE_RED_GREEN;
            } else if (isColorWall(detection.detected_color)) {
                detection.line_type = LINE_BLACK_BLUE;
            }

            Serial.printf("EDGE_CASE: Following strongest - S%d Color:%d, Angle:%d°\n",
                         strongest_sensor, strongest_color, current_angle);
            return true;
        }

        case ACTION_AVERAGE_ANGLE: {
            // Use the primary sensor from the rule but average angles if multiple sensors detect
            detection.detected_color = current_colors[rule->primary_sensor - 1];
            detection.detecting_sensor = rule->primary_sensor;

            // For averaging, assume sensors are 50mm apart and calculate effective angle
            // This is a simplified approach - in reality you'd need more complex geometry
            uint8_t effective_angle = current_angle;

            // Count how many sensors detect the same color
            uint8_t target_color = detection.detected_color;
            uint8_t sensor_count = 0;

            for (int i = 0; i < 3; i++) {
                if (current_colors[i] == target_color && target_color != WHITE) {
                    sensor_count++;
                }
            }

            // Adjust angle based on multiple sensor detection (line is wider/more angled)
            if (sensor_count > 1) {
                effective_angle = current_angle / 2; // Reduce angle for wider detection
            }

            detection.initial_angle = effective_angle;
            detection.current_target_angle = effective_angle;
            detection.angle_valid = true;
            detection.detection_active = true;

            if (isColorNavigable(detection.detected_color)) {
                detection.line_type = LINE_RED_GREEN;
            } else if (isColorWall(detection.detected_color)) {
                detection.line_type = LINE_BLACK_BLUE;
            }

            Serial.printf("EDGE_CASE: Averaging angle - S%d Color:%d, EffectiveAngle:%d° (sensors:%d)\n",
                         rule->primary_sensor, detection.detected_color, effective_angle, sensor_count);
            return true;
        }

        case ACTION_EMERGENCY_STOP: {
            Serial.printf("EDGE_CASE: EMERGENCY STOP triggered - %s\n", rule->description);

            // Force immediate stop state
            navcon_status.current_state = NAVCON_STOP;

            // Set emergency detection to prevent further processing
            detection.detected_color = RED; // Use RED to trigger stop behavior
            detection.detecting_sensor = 2;
            detection.initial_angle = 0;
            detection.current_target_angle = 0;
            detection.angle_valid = false;
            detection.detection_active = true;
            detection.line_type = LINE_RED_GREEN; // Force stop handling

            // Log emergency for analysis
            Serial.println("EDGE_CASE: EMERGENCY - Multiple conflicting lines detected");
            Serial.printf("EDGE_CASE: EMERGENCY - S1:%d, S2:%d, S3:%d\n",
                         current_colors[0], current_colors[1], current_colors[2]);

            return true;
        }

        case ACTION_IGNORE_ALL: {
            Serial.printf("EDGE_CASE: Ignoring all sensors - %s\n", rule->description);
            // Don't set detection_active, continue normal forward operation
            return false; // Return false to continue normal forward movement
        }

        case ACTION_BACKUP_FIRST: {
            Serial.printf("EDGE_CASE: Backup first required - %s\n", rule->description);

            // Force immediate stop and reverse
            navcon_status.current_state = NAVCON_STOP;

            // Set up for backup operation
            detection.detected_color = current_colors[rule->primary_sensor - 1];
            detection.detecting_sensor = rule->primary_sensor;
            detection.initial_angle = current_angle;
            detection.current_target_angle = current_angle;
            detection.angle_valid = true;
            detection.detection_active = true;
            detection.line_type = LINE_BLACK_BLUE; // Force backup behavior

            Serial.println("EDGE_CASE: Backup sequence initiated");
            return true;
        }

        default: {
            Serial.printf("EDGE_CASE: Unknown action type: %d\n", rule->action);
            return false;
        }
    }
}

// ==================== ENHANCED LINE DETECTION WITH EDGE CASES ====================

void updateLineDetectionWithEdgeCases() {
    LineDetectionData& detection = navcon_status.line_detection;

    // Serial.printf("EDGE_CASE: updateLineDetectionWithEdgeCases - active:%d, sensor:%d, angle_valid:%d\n",
    //              detection.detection_active, detection.detecting_sensor, detection.angle_valid);

    // Skip detection if we're already processing a line
    if (detection.detection_active) {
        return;
    }

    // Check if S2 has changed and is non-white (PRIORITY 1: Center sensor)
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

        // Serial.printf("EDGE_CASE: S2 PRIORITY DETECTION - Color=%d, Angle=%d°, Type=%d\n",
        //              detection.detected_color, detection.initial_angle, detection.line_type);
        // printEdgeCaseAnalysis(current_colors[0], current_colors[1], current_colors[2]);
        return;
    }

    // Check for multi-sensor detections (PRIORITY 2)
    bool s1_detected = (current_colors[0] != WHITE);
    bool s2_detected = (current_colors[1] != WHITE);
    bool s3_detected = (current_colors[2] != WHITE);

    // S1+S2 detection (line from left)
    if (s1_detected && s2_detected && !detection.detection_active) {
        detection.detected_color = current_colors[1]; // Use S2 color
        detection.detecting_sensor = 1; // Mark as coming from left
        detection.initial_angle = received_incidence_angle;
        detection.current_target_angle = received_incidence_angle;
        detection.angle_valid = true;
        detection.detection_active = true;

        if (isColorNavigable(detection.detected_color)) {
            detection.line_type = LINE_RED_GREEN;
        } else if (isColorWall(detection.detected_color)) {
            detection.line_type = LINE_BLACK_BLUE;
        }

        // Serial.printf("EDGE_CASE: S1+S2 DETECTION - Color=%d, Angle=%d°, Type=%d\n",
        //              detection.detected_color, detection.initial_angle, detection.line_type);
        // printEdgeCaseAnalysis(current_colors[0], current_colors[1], current_colors[2]);
        return;
    }

    // S2+S3 detection (line from right)
    if (s2_detected && s3_detected && !detection.detection_active) {
        detection.detected_color = current_colors[1]; // Use S2 color
        detection.detecting_sensor = 3; // Mark as coming from right
        detection.initial_angle = received_incidence_angle;
        detection.current_target_angle = received_incidence_angle;
        detection.angle_valid = true;
        detection.detection_active = true;

        if (isColorNavigable(detection.detected_color)) {
            detection.line_type = LINE_RED_GREEN;
        } else if (isColorWall(detection.detected_color)) {
            detection.line_type = LINE_BLACK_BLUE;
        }

        // Serial.printf("EDGE_CASE: S2+S3 DETECTION - Color=%d, Angle=%d°, Type=%d\n",
        //              detection.detected_color, detection.initial_angle, detection.line_type);
        // printEdgeCaseAnalysis(current_colors[0], current_colors[1], current_colors[2]);
        return;
    }

    // Single edge sensor detection (S1 or S3 alone) - START DISTANCE TRACKING
    if (s1_detected && !s2_detected && !detection.detection_active && detection.detecting_sensor == 0) {
        // Serial.printf("EDGE_CASE: S1 EDGE START - Color=%d, starting distance tracking\n", current_colors[0]);
        detection.detected_color = current_colors[0];
        detection.detecting_sensor = 1;
        detection.detection_start_distance = current_distance;
        // Don't set detection_active yet - wait for S2 confirmation or distance threshold
        // printEdgeCaseAnalysis(current_colors[0], current_colors[1], current_colors[2]);
    }
    else if (s3_detected && !s2_detected && !detection.detection_active && detection.detecting_sensor == 0) {
        // Serial.printf("EDGE_CASE: S3 EDGE START - Color=%d, starting distance tracking\n", current_colors[2]);
        detection.detected_color = current_colors[2];
        detection.detecting_sensor = 3;
        detection.detection_start_distance = current_distance;
        // Don't set detection_active yet - wait for S2 confirmation or distance threshold
        // printEdgeCaseAnalysis(current_colors[0], current_colors[1], current_colors[2]);
    }

    // Check for S2 confirmation after edge detection started
    if (detection.detecting_sensor != 0 && !detection.detection_active && !detection.angle_valid) {
        if (current_colors[1] != WHITE) {
            // S2 confirmed the line!
            detection.initial_angle = received_incidence_angle;
            detection.current_target_angle = received_incidence_angle;
            detection.angle_valid = true;
            detection.detection_active = true;

            if (isColorNavigable(detection.detected_color)) {
                detection.line_type = LINE_RED_GREEN;
            } else if (isColorWall(detection.detected_color)) {
                detection.line_type = LINE_BLACK_BLUE;
            }

            // Serial.printf("EDGE_CASE: S2 CONFIRMED edge S%d - Color=%d, Angle=%d°, Type=%d\n",
            //              detection.detecting_sensor, detection.detected_color,
            //              detection.initial_angle, detection.line_type);
            // printEdgeCaseAnalysis(current_colors[0], current_colors[1], current_colors[2]);
        }
        else {
            // Check if we've traveled far enough (50mm) to infer steep angle >45°
            uint16_t travel_distance = current_distance - detection.detection_start_distance;

            // Serial.printf("EDGE_CASE: Distance check - current=%d, start=%d, travel=%d, threshold=%d\n",
            //              current_distance, detection.detection_start_distance,
            //              travel_distance, SENSOR_SPACING);

            if (travel_distance >= SENSOR_SPACING) {
                // Steep angle inferred (>45°)
                // Serial.printf("EDGE_CASE: STEEP ANGLE INFERRED - Sensor=%d, Color=%d, Distance=%dmm\n",
                //              detection.detecting_sensor, detection.detected_color, travel_distance);

                detection.initial_angle = 46;  // >45° inferred
                detection.current_target_angle = 46;
                detection.angle_valid = false;  // Inferred, not measured
                detection.detection_active = true;  // NOW activate

                if (isColorNavigable(detection.detected_color)) {
                    detection.line_type = LINE_RED_GREEN;
                } else if (isColorWall(detection.detected_color)) {
                    detection.line_type = LINE_BLACK_BLUE;
                }

                // Serial.printf("EDGE_CASE: Steep angle setup complete - Type=%d, Active=%d\n",
                //              detection.line_type, detection.detection_active);
                // printEdgeCaseAnalysis(current_colors[0], current_colors[1], current_colors[2]);
            }
        }
    }
}

// ==================== DEBUGGING AND UTILITY FUNCTIONS ====================

const char* getPriorityName(EdgeCasePriority priority) {
    switch (priority) {
        case PRIORITY_EMERGENCY: return "EMERGENCY";
        case PRIORITY_HIGH:      return "HIGH";
        case PRIORITY_MEDIUM:    return "MEDIUM";
        case PRIORITY_LOW:       return "LOW";
        case PRIORITY_IGNORE:    return "IGNORE";
        default:                 return "UNKNOWN";
    }
}

const char* getActionName(EdgeCaseAction action) {
    switch (action) {
        case ACTION_FOLLOW_S1:       return "FOLLOW_S1";
        case ACTION_FOLLOW_S2:       return "FOLLOW_S2";
        case ACTION_FOLLOW_S3:       return "FOLLOW_S3";
        case ACTION_FOLLOW_STRONGEST: return "FOLLOW_STRONGEST";
        case ACTION_AVERAGE_ANGLE:   return "AVERAGE_ANGLE";
        case ACTION_EMERGENCY_STOP:  return "EMERGENCY_STOP";
        case ACTION_IGNORE_ALL:      return "IGNORE_ALL";
        case ACTION_BACKUP_FIRST:    return "BACKUP_FIRST";
        default:                     return "UNKNOWN";
    }
}

void printEdgeCaseAnalysis(uint8_t s1_color, uint8_t s2_color, uint8_t s3_color) {
    Serial.println("\n=== EDGE CASE ANALYSIS ===");

    const char* color_names[] = {"WHITE", "RED", "GREEN", "BLUE", "BLACK"};
    Serial.printf("Sensor Reading: S1=%s, S2=%s, S3=%s\n",
                 s1_color < 5 ? color_names[s1_color] : "UNKNOWN",
                 s2_color < 5 ? color_names[s2_color] : "UNKNOWN",
                 s3_color < 5 ? color_names[s3_color] : "UNKNOWN");

    const EdgeCaseRule* rule = findEdgeCaseRule(s1_color, s2_color, s3_color);
    if (rule) {
        Serial.printf("Rule: %s\n", rule->description);
        Serial.printf("Priority: %s\n", getPriorityName(rule->priority));
        Serial.printf("Action: %s\n", getActionName(rule->action));
        Serial.printf("Primary Sensor: S%d\n", rule->primary_sensor);
    } else {
        Serial.println("No specific rule found");
    }

    Serial.println("=========================\n");
}

// ==================== COLOR NAME UTILITY ====================

// getColorName function removed to avoid conflicts
// Debug output will use numeric color values instead