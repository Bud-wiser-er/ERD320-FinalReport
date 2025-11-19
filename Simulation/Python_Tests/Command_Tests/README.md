# SNC Command Test Suite

## Overview

This directory contains comprehensive Python-based test applications for validating all SNC (Sensor Navigation Control) subsystem commands across different system states. Each test application provides a graphical user interface for manual command testing and automated test sequence execution.

## Test Applications

### 1. test_idle_commands.py

**Purpose**: IDLE state command validation

**Description**: Tests IDLE:HUB:0 and IDLE:SNC:0 command packets and validates transition from IDLE to CAL state. This test verifies the system initialization sequence and confirms proper command acknowledgment.

**Key Features**:
- IDLE state command transmission
- Response monitoring and validation
- State transition testing (IDLE to CAL)

**Usage**: Execute the script to launch the GUI. Connect to the SNC serial port and send IDLE commands to verify proper system initialization.

---

### 2. test_cal_commands.py

**Purpose**: Calibration state command testing

**Description**: Comprehensive testing of CAL:SS and CAL:MDPS calibration commands (IST 0 and 1). Includes automated full calibration sequence, individual subsystem calibration, and visual calibration status indicators.

**Key Features**:
- Full calibration sequence automation
- Individual SS calibration testing
- Individual MDPS calibration testing
- Visual calibration status indicators
- Calibration flowchart reference

**Usage**: Launch the application, connect to the serial port, and execute calibration sequences. Visual indicators display SS and MDPS calibration status in real-time.

---

### 3. test_maze_ss_commands.py

**Purpose**: MAZE state Sensor Subsystem (SS) command validation

**Description**: Tests MAZE:SS:1 color data packets with comprehensive coverage of all 8 color combinations (WHITE, RED, GREEN, BLUE, BLACK) across a 3-sensor array. Includes angle testing from 0 to 90 degrees and complete color matrix validation.

**Key Features**:
- Complete color matrix testing (all 8 combinations)
- Full angle range testing (0-90 degrees)
- 3-sensor array visualization
- Color encoding reference documentation
- Quick color palette tests
- Automated complete matrix execution

**Usage**: Connect to the SNC serial port and use the color palette for quick tests or execute the complete color matrix to validate all sensor combinations.

---

### 4. test_maze_mdps_commands.py

**Purpose**: MAZE state Motor Drive & Power Supply (MDPS) command testing

**Description**: Validates all MDPS motor commands (IST 1-4) including stop/rotate, confirmation, forward motion, and distance update packets. Includes speed variation testing, rotation testing, and virtual robot visualization.

**Key Features**:
- All MDPS commands (IST 1, 2, 3, 4)
- Speed variation testing (0-50 mm/s)
- Rotation testing (0-180 degrees)
- Distance tracking with virtual robot
- Forward motion test sequences
- Complete MDPS command suite automation

**Usage**: Launch the application to test motor commands. The virtual robot visualization displays movement based on sent commands. Execute automated test suites to validate all MDPS functionality.

---

### 5. test_navcon_decisions.py

**Purpose**: NAVCON algorithm decision matrix validation

**Description**: Comprehensive testing of NAVCON decision-making across all angle categories (straight, alignment, steep) and color combinations. Tests the complete decision matrix to validate navigation logic for all possible sensor inputs.

**Key Features**:
- Complete angle/color matrix testing
- Angle categories: straight (≤5°), alignment (5-45°), steep (>45°)
- All color combinations across 3 sensors
- Expected behavior analysis for each scenario
- Decision tree visualization
- Automated complete matrix execution

**Usage**: Select angle and color scenarios to test individual NAVCON decisions, or execute the complete test matrix to validate all combinations. Expected behaviors are displayed for verification.

---

### 6. test_pure_tone.py

**Purpose**: Pure tone detection validation (2800 Hz dual-tone)

**Description**: Tests the pure tone detection system with 8 comprehensive test cases including valid dual-tone detection, single tone rejection, timeout validation, duration checking, and MAZE-SOS state toggling.

**Key Features**:
- Valid dual-tone sequence testing (500-1000ms duration, <2s gap)
- Single tone rejection validation
- Timeout testing (>2s gap rejection)
- Short duration rejection (<500ms)
- Long duration rejection (>1000ms)
- MAZE to SOS state transition testing
- SOS to MAZE restoration testing
- Visual tone timeline
- Manual tone simulation

**Usage**: Execute individual pure tone tests or run the complete test suite. The timeline display shows tone sequences and validation results. Manual simulation allows custom tone duration testing.

---

### 7. hub_testing_suite.py

**Purpose**: Comprehensive QTP (Quality Test Procedure) validation

**Description**: Implements all 10 QTP tests (QTP-SNC-01 through QTP-SNC-10) for complete subsystem validation. Provides automated test execution, pass/fail validation, and comprehensive test reporting.

**Key Features**:
- All 10 QTP tests implemented
- Automated test execution
- Pass/fail validation criteria
- Test report generation
- Comprehensive packet monitoring
- QTP test results summary

**QTP Tests Covered**:
- QTP-SNC-01: IDLE state commands
- QTP-SNC-02: CAL state transitions
- QTP-SNC-03: SS calibration validation
- QTP-SNC-04: MDPS calibration validation
- QTP-SNC-05: MAZE:SS color detection
- QTP-SNC-06: MAZE:MDPS motor commands
- QTP-SNC-07: Pure tone detection
- QTP-SNC-08: NAVCON decision validation
- QTP-SNC-09: State machine validation
- QTP-SNC-10: Complete system integration

**Usage**: Launch the suite and execute individual QTP tests or run all tests sequentially. Results are logged with pass/fail status and can be exported for documentation.

---

### 8. dual_port_maze_tester.py

**Purpose**: Dual-port maze simulation with independent SS and MDPS emulation

**Description**: Advanced testing application using two independent serial ports to separately emulate the Sensor Subsystem (SS) and Motor Drive & Power Supply (MDPS). Provides virtual maze state simulation with real-time visualization and comprehensive logging.

**Key Features**:
- Independent SS port emulation (Port 1)
- Independent MDPS port emulation (Port 2)
- Virtual maze state simulation
- Color event injection to SS port
- Angle event injection to SS port
- Motor command monitoring on MDPS port
- Real-time maze visualization
- Comprehensive dual-port logging
- Test statistics and export

**Usage**: Connect two serial ports - one for SS emulation and one for MDPS emulation. The virtual maze allows injection of color and angle events to SS while monitoring MDPS motor responses independently. This configuration enables comprehensive integration testing without physical hardware.

---

## Dependencies

All test applications require:
- Python 3.7+
- tkinter (GUI framework)
- pyserial (serial communication)
- Core modules: `scs_protocol.py`, `gui_framework.py`

## Core Modules

Test applications depend on two core modules located in `../Core/`:

1. **scs_protocol.py**: Complete SCS protocol implementation with packet structures, encoding/decoding functions, and command helpers.

2. **gui_framework.py**: Base GUI framework providing consistent interface across all test applications, including serial port management, packet logging, and statistics tracking.

## Execution

To run any test application:

```bash
cd /home/user/ERD320-SNC/Simulation/Python_Tests/Command_Tests
python3 <test_name>.py
```

Example:
```bash
python3 test_pure_tone.py
```

## Serial Port Configuration

All test applications support:
- Serial port selection from dropdown
- Baud rate configuration (default: 115200)
- Auto-refresh port detection
- Connection status monitoring
- Packet transmission/reception logging

## Test Methodology

Each test application follows a consistent methodology:
1. Serial port connection and configuration
2. Command transmission using SCS protocol
3. Response monitoring and validation
4. Results logging with color-coded status
5. Statistics tracking and export capability

## Documentation References

For detailed protocol information, see:
- Main Simulation README: `../../README.md`
- SCS Protocol: `../../Core/scs_protocol.py`
- Project Report: `../../ERD320_Report.pdf`
- QTP Documentation: Project documentation folder

## Notes

- All emojis have been removed from GUI elements per project requirements.
- Formal language is used throughout all documentation and interface elements.
- Test applications are designed for manual testing and automated sequence execution.
- Comprehensive logging enables detailed test result analysis and debugging.
