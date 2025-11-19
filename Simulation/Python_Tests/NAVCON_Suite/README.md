# NAVCON Test Suite

## Overview

This directory contains the comprehensive NAVCON (Navigation Control) testing application for validating the complete navigation algorithm implementation in the SNC subsystem.

## Test Application

### navcon_tester.py

**Purpose**: Comprehensive NAVCON algorithm validation and testing

**Description**: This is the primary NAVCON testing application that provides extensive validation of the navigation control algorithm. The application tests the complete decision-making process that determines robot behavior based on sensor inputs (color detection) and angle measurements.

**Key Features**:
- Complete NAVCON algorithm implementation testing
- Angle-dependent navigation validation
- Color-based decision tree testing
- Three navigation categories:
  - Straight navigation (θ ≤ 5°)
  - Alignment maneuvers (5° < θ ≤ 45°)
  - Steep angle handling (θ > 45°)
- Multi-sensor color detection (3-sensor array)
- Real-time decision visualization
- Comprehensive test scenarios
- Expected behavior validation

**Algorithm Overview**:

The NAVCON algorithm processes sensor data to make navigation decisions:

1. **Color Detection**: Three sensors (S1, S2, S3) detect line colors (WHITE, RED, GREEN, BLUE, BLACK)
2. **Angle Measurement**: Incidence angle relative to detected line (0-90°)
3. **Decision Logic**: Combined color and angle analysis determines robot action

**Decision Categories**:

- **GREEN Line Following**: Primary navigation path
  - Straight: Continue forward (θ ≤ 5°)
  - Alignment: Minor corrections (5° < θ ≤ 45°)
  - Steep: Rotation required (θ > 45°)

- **RED Line Handling**: Special markers or decision points
  - Behavior depends on configuration and angle

- **BLUE/BLACK Detection**: Wall or obstacle avoidance
  - Triggers avoidance maneuvers

- **WHITE Detection**: Open area navigation
  - Continues forward motion or initiates search pattern

**Test Coverage**:

The NAVCON tester validates:
- All angle ranges (0-90° in discrete steps)
- All color combinations across 3-sensor array
- Edge cases (single sensor detection, multiple colors)
- Transition behaviors between decision categories
- Response timing and consistency

**Usage**:

1. Launch the application:
   ```bash
   python3 navcon_tester.py
   ```

2. Connect to the SNC serial port

3. Select test scenarios:
   - Individual angle/color combinations
   - Complete angle range sweep
   - Specific color pattern testing
   - Full matrix validation

4. Monitor decision outputs and validate against expected behaviors

5. Export test results for documentation

**Test Methodology**:

1. **Scenario Selection**: Choose angle and color configuration
2. **Packet Transmission**: Send MAZE:SS:1 packet with encoded sensor data
3. **Response Monitoring**: Observe NAVCON decision output
4. **Validation**: Compare actual behavior to expected decision
5. **Logging**: Record test results with pass/fail status

**Integration with Command Tests**:

This NAVCON suite provides comprehensive testing beyond the `test_navcon_decisions.py` application in the Command_Tests directory. While the command tester focuses on individual command validation, this suite provides:
- Extended test scenarios
- More detailed decision analysis
- Historical test result tracking
- Advanced visualization options

**Dependencies**:

- Python 3.7+
- tkinter (GUI framework)
- pyserial (serial communication)
- Core modules: `../../Core/scs_protocol.py`, `../../Core/gui_framework.py`

**Related Documentation**:

- Command Test Suite: `../Command_Tests/README.md`
- Main Simulation README: `../../README.md`
- NAVCON Algorithm Specification: Project documentation
- MATLAB NAVCON Analysis: `../../MATLAB_Simulations/NAVCON_Analysis/README.md`

## NAVCON Algorithm Background

The NAVCON algorithm is the core navigation intelligence of the AMazeEng MARV robot. It processes real-time sensor data to make autonomous navigation decisions in maze environments. The algorithm must:

1. Detect and classify line colors from three independent sensors
2. Calculate incidence angles relative to detected lines
3. Determine appropriate motor commands based on combined color and angle data
4. Handle special cases (crossings, dead ends, obstacles)
5. Maintain consistent behavior across all sensor configurations

This test suite ensures the algorithm meets all requirements and handles edge cases correctly.

## Notes

- This application provides the most comprehensive NAVCON testing capability in the project.
- All emojis have been removed from GUI elements per project requirements.
- Formal language is used throughout all documentation and interface elements.
- Test results can be exported for inclusion in project reports and QTP documentation.
