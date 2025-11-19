# QTP Test Results Documentation

**Quality Test Procedure Verification Evidence**
**SNC Subsystem - ERD320 AMazeEng MARV**
**Last Updated:** 2025-01-18

---

## Overview

This directory contains comprehensive evidence of SNC subsystem compliance with all Quality Test Procedures (QTPs) as specified in "AMazeEng MARV QTPs 2025.pdf".

### Test Execution Summary

- **Total QTPs:** 10
- **Passed:** 10
- **Failed:** 0
- **Success Rate:** 100%
- **Test Environment:** Real hardware + Python simulation suite
- **Test Date Range:** Phase 2 - Phase 3 (2024-2025)

---

## QTP Requirements

### QTP-SNC-01: IDLE → CAL State Transition

**Requirement:** System shall transition from IDLE to CAL state when touch sensor is activated.

**Test Evidence:**
- **Location:** `HUB_Test_Logs/All_QTP_Complete_Logs.txt` (search for "QTP-SNC-01")
- **Python Test:** `Python_Suite_Results/test_idle_commands_results.txt`
- **Status:** ✅ PASS

**Test Procedure:**
1. System powered in IDLE state
2. HUB sends IDLE:HUB:0 packet
3. SNC responds with IDLE:SNC:0
4. Touch sensor activated
5. SNC transitions to CAL state
6. SNC sends CAL:SNC:0 acknowledgment

**Success Criteria:**
- ✅ Touch sensor detection within 100ms
- ✅ State transition to CAL confirmed
- ✅ Proper CAL:SNC:0 packet transmitted
- ✅ No spurious transitions

**Evidence Files:**
- HUB execution logs with timestamps
- Python test automation results
- State machine verification

---

### QTP-SNC-02: CAL → MAZE State Transition

**Requirement:** System shall transition from CAL to MAZE state after successful calibration sequence.

**Test Evidence:**
- **Location:** `HUB_Test_Logs/All_QTP_Complete_Logs.txt` (search for "QTP-SNC-02")
- **Python Test:** `Python_Suite_Results/test_cal_commands_results.txt`
- **Status:** ✅ PASS

**Test Procedure:**
1. System in CAL state
2. HUB sends CAL:SS:0 (start sensor calibration)
3. SNC responds appropriately
4. HUB sends CAL:SS:1 (sensor calibration complete)
5. HUB sends CAL:MDPS:0 (start motor calibration)
6. SNC responds appropriately
7. HUB sends CAL:MDPS:1 (motor calibration complete)
8. SNC transitions to MAZE state
9. SNC sends MAZE:SNC:0 acknowledgment

**Success Criteria:**
- ✅ All calibration commands acknowledged
- ✅ State transition to MAZE confirmed
- ✅ Proper MAZE:SNC:0 packet transmitted
- ✅ Calibration sequence order enforced

**Evidence Files:**
- Complete calibration sequence logs
- State transition verification
- Timing analysis

---

### QTP-SNC-03: NAVCON Forward Navigation Logic

**Requirement:** NAVCON algorithm shall correctly determine forward motion commands based on sensor input.

**Test Evidence:**
- **Location:** `Python_Suite_Results/test_navcon_decisions_results.txt`
- **MATLAB Analysis:** `../MATLAB_Simulations/NAVCON_Geometric_Analysis/`
- **Status:** ✅ PASS

**Test Coverage:**
- ✅ Small angles (≤5°): All white sensors → FORWARD
- ✅ Medium angles (5-45°): White center → FORWARD
- ✅ Large angles (>45°): Any forward-valid configuration → FORWARD
- ✅ All color combinations tested

**Test Procedure:**
1. Python test suite sends MAZE:SS:1 (color) and MAZE:SS:2 (angle) packets
2. SNC processes sensor data through NAVCON algorithm
3. SNC determines motion primitive
4. SNC sends MAZE:MDPS:X command
5. Verify command matches expected navigation rule

**Success Criteria:**
- ✅ 100% accuracy on forward navigation scenarios
- ✅ Correct angle threshold application
- ✅ Proper color priority handling
- ✅ Consistent decision-making

**Evidence Files:**
- NAVCON decision matrix validation (125+ scenarios)
- MATLAB geometric analysis
- Python automated test results

---

### QTP-SNC-04: NAVCON Rotation Decision Logic

**Requirement:** NAVCON algorithm shall correctly determine rotation commands when forward motion is not valid.

**Test Evidence:**
- **Location:** `Python_Suite_Results/test_navcon_decisions_results.txt`
- **Status:** ✅ PASS

**Test Coverage:**
- ✅ All rotation scenarios (LEFT, RIGHT, TURN_180)
- ✅ RED sensor handling (priority rotation)
- ✅ GREEN sensor handling (preferred direction)
- ✅ BLACK sensor avoidance
- ✅ No-white-sensor scenarios

**Test Procedure:**
1. Configure sensors for rotation scenario
2. Send color and angle data via SCS protocol
3. NAVCON determines rotation primitive
4. Verify rotation command correctness
5. Validate rotation priority rules

**Success Criteria:**
- ✅ RED sensor triggers immediate rotation
- ✅ GREEN sensor influences rotation direction
- ✅ BLACK sensors avoided correctly
- ✅ TURN_180 only when no other option

**Evidence Files:**
- Complete rotation decision matrix
- Priority rule verification
- Edge case validation

---

### QTP-SNC-05: SCS Protocol Compliance

**Requirement:** All SCS protocol packets shall be properly formatted and follow state machine rules.

**Test Evidence:**
- **Location:** `Python_Suite_Results/` (all test files)
- **Protocol Spec:** `../Evidence_Archive/SCS_Protocol_Spec.md`
- **Status:** ✅ PASS

**Test Coverage:**
- ✅ All 4 system states (IDLE, CAL, MAZE, SOS)
- ✅ All 4 subsystems (HUB, SNC, MDPS, SS)
- ✅ 47 distinct SCS commands
- ✅ Control byte calculation
- ✅ Data byte formatting
- ✅ Checksum validation

**Test Procedure:**
1. Send packets for each command type
2. Verify packet structure (SYS:SUB:IST format)
3. Validate control byte encoding
4. Check data byte values
5. Verify checksum calculation

**Success Criteria:**
- ✅ 100% packet format compliance
- ✅ No protocol violations detected
- ✅ Proper state machine adherence
- ✅ Correct subsystem addressing

**Evidence Files:**
- Packet capture logs (thousands of packets)
- Protocol validation reports
- State machine verification

---

### QTP-SNC-06: Pure Tone Detection (2800 Hz, 4000 Hz)

**Requirement:** System shall detect pure tones at 2800 Hz and 4000 Hz with proper timing.

**Test Evidence:**
- **Location:** `Python_Suite_Results/test_pure_tone_results.txt`
- **Lab Results:** `../Phase2_Lab_Results/Pure_Tone_Oscilloscope/`
- **MATLAB Analysis:** `../MATLAB_Simulations/Pure_Tone_Gain_Analysis/`
- **Status:** ✅ PASS

**Test Coverage:**
- ✅ 2800 Hz detection (500-1000ms duration)
- ✅ 4000 Hz detection (500-1000ms duration)
- ✅ Dual-tone sequence detection (within 2s window)
- ✅ False alarm rejection

**Test Procedure:**
1. Apply pure tone to microphone input
2. Verify analog circuit amplification
3. Monitor digital detection via Goertzel algorithm
4. Confirm timing requirements met
5. Validate state transition trigger

**Success Criteria:**
- ✅ Tone detection within specified duration
- ✅ Both tones required for state change
- ✅ Proper timing window enforcement
- ✅ No false positives

**Evidence Files:**
- Oscilloscope captures at 2800 Hz and 4000 Hz
- Goertzel algorithm validation
- Timing analysis logs
- MATLAB gain calculations

---

### QTP-SNC-07: MAZE ↔ SOS State Toggle

**Requirement:** Dual-tone sequence shall toggle between MAZE and SOS states.

**Test Evidence:**
- **Location:** `Python_Suite_Results/test_pure_tone_results.txt`
- **Status:** ✅ PASS

**Test Coverage:**
- ✅ MAZE → SOS transition on dual-tone
- ✅ SOS → MAZE transition on dual-tone
- ✅ Multiple toggle cycles
- ✅ State persistence verification

**Test Procedure:**
1. System in MAZE state
2. Apply 2800 Hz tone (500-1000ms)
3. Apply 4000 Hz tone (within 2s window)
4. Verify transition to SOS state
5. Repeat sequence to return to MAZE
6. Confirm state toggle works bidirectionally

**Success Criteria:**
- ✅ MAZE → SOS transition confirmed
- ✅ SOS → MAZE transition confirmed
- ✅ State change only on valid dual-tone sequence
- ✅ No state change on incomplete sequence

**Evidence Files:**
- State transition logs
- Dual-tone sequence validation
- Toggle cycle verification

---

### QTP-SNC-08: WiFi Telemetry Transmission

**Requirement:** SNC shall transmit telemetry data via WiFi to monitoring system.

**Test Evidence:**
- **Location:** `HUB_Test_Logs/All_QTP_Complete_Logs.txt`
- **WiFi Logs:** ESP32 WiFi communication logs
- **Status:** ✅ PASS

**Test Coverage:**
- ✅ WiFi connection establishment
- ✅ Telemetry packet transmission
- ✅ Data integrity verification
- ✅ Connection stability

**Test Procedure:**
1. Configure WiFi credentials
2. Establish connection to monitoring network
3. Transmit telemetry packets
4. Verify packet reception
5. Validate data accuracy

**Success Criteria:**
- ✅ WiFi connection successful
- ✅ Telemetry packets transmitted
- ✅ Data received correctly
- ✅ Stable communication maintained

**Evidence Files:**
- WiFi connection logs
- Telemetry packet captures
- Data validation results

---

### QTP-SNC-09: Main Loop Timing Requirements

**Requirement:** Main control loop shall execute within specified timing constraints.

**Test Evidence:**
- **Location:** `HUB_Test_Logs/All_QTP_Complete_Logs.txt`
- **Timing Analysis:** Phase3 timing measurements
- **Status:** ✅ PASS

**Test Coverage:**
- ✅ Loop execution time measurement
- ✅ Worst-case timing analysis
- ✅ Interrupt response time
- ✅ Real-time constraint verification

**Test Procedure:**
1. Instrument main loop with timing markers
2. Measure loop execution time
3. Identify worst-case scenarios
4. Verify all timing deadlines met
5. Confirm real-time performance

**Success Criteria:**
- ✅ Main loop executes within time budget
- ✅ No deadline misses detected
- ✅ Interrupt latency acceptable
- ✅ Real-time constraints satisfied

**Evidence Files:**
- Timing measurement logs
- Worst-case analysis
- Performance validation

---

### QTP-SNC-10: End-of-Maze Detection and Handling

**Requirement:** System shall detect end-of-maze signal and handle appropriately.

**Test Evidence:**
- **Location:** `Python_Suite_Results/test_maze_ss_commands_results.txt`
- **Status:** ✅ PASS

**Test Coverage:**
- ✅ MAZE:SS:3 packet detection
- ✅ End-of-maze state handling
- ✅ Motion command cessation
- ✅ Proper system shutdown sequence

**Test Procedure:**
1. Simulate maze navigation
2. Send MAZE:SS:3 (end-of-maze signal)
3. Verify SNC detects end condition
4. Confirm motion commands stop
5. Validate proper system state

**Success Criteria:**
- ✅ End-of-maze signal detected
- ✅ Motion commands cease
- ✅ System enters safe state
- ✅ Proper acknowledgment sent

**Evidence Files:**
- End-of-maze detection logs
- Motion command verification
- State handling validation

---

## Test Execution Environment

### Hardware Configuration

- **ESP32-WROOM-32 Development Board**
- **Pure Tone Detection Circuit:** Cascaded MFB bandpass filters
- **Microphone:** Electret condenser microphone
- **Touch Sensor:** Capacitive touch pad
- **Serial Interfaces:** UART1 (SS), UART2 (MDPS), UART0 (HUB)

### Software Configuration

- **Arduino IDE:** 1.8.19+
- **ESP32 Board Package:** 2.0.11+
- **Python Test Suite:** 3.8+
- **Serial Communication:** 19200 baud (HUB), 115200 baud (SS/MDPS)

### Test Tools

1. **Python Test Suite:**
   - Individual command testers
   - NAVCON comprehensive validator
   - HUB testing suite
   - Dual-port maze simulator

2. **Laboratory Equipment:**
   - Oscilloscope (Tektronix)
   - Function generator
   - Power supply
   - Multimeter

3. **Analysis Tools:**
   - MATLAB R2023a+
   - LTspice XVII
   - Serial terminal utilities

---

## Evidence Files

### HUB Test Logs

**Location:** `HUB_Test_Logs/`

- `All_QTP_Complete_Logs.txt` - Complete HUB execution logs (724 KB)
  - Contains timestamped logs for all QTP executions
  - Format: `DD/MM/YYYY HH:MM:SS || SEQ || DIRECTION || (SYS-SUB-IST) || ...`
  - Includes packet captures, state transitions, and validation results

### Python Suite Results

**Location:** `Python_Suite_Results/`

Each test script generates detailed results:
- `test_idle_commands_results.txt` - IDLE state testing
- `test_cal_commands_results.txt` - Calibration testing
- `test_maze_mdps_commands_results.txt` - MAZE MDPS testing
- `test_maze_ss_commands_results.txt` - MAZE SS testing
- `test_navcon_decisions_results.txt` - NAVCON validation
- `test_pure_tone_results.txt` - Pure tone detection
- `hub_testing_suite_results.txt` - Comprehensive QTP automation
- `dual_port_maze_logs.txt` - Full maze simulation

---

## Supporting Documentation

### Referenced Documents

- **QTP Specification:** `QTP_Specification_2025.pdf`
- **SNC Reference Guide:** `../../SNC_REFERENCE_GUIDE.md`
- **SCS Protocol Spec:** `../Evidence_Archive/SCS_Protocol_Spec.md`
- **Python Test Suite:** `../../Simulation/Python_Tests/Command_Tests/`

### Related Evidence

- **Phase 2 Lab Results:** `../Phase2_Lab_Results/`
- **MATLAB Simulations:** `../MATLAB_Simulations/`
- **Phase 3 Compliance:** `../Phase3_Compliance/`

---

## Verification Checklist

- ✅ QTP-SNC-01: IDLE → CAL transition - PASSED
- ✅ QTP-SNC-02: CAL → MAZE transition - PASSED
- ✅ QTP-SNC-03: NAVCON forward logic - PASSED
- ✅ QTP-SNC-04: NAVCON rotation logic - PASSED
- ✅ QTP-SNC-05: SCS protocol compliance - PASSED
- ✅ QTP-SNC-06: Pure tone detection - PASSED
- ✅ QTP-SNC-07: MAZE ↔ SOS toggle - PASSED
- ✅ QTP-SNC-08: WiFi telemetry - PASSED
- ✅ QTP-SNC-09: Main loop timing - PASSED
- ✅ QTP-SNC-10: End-of-maze handling - PASSED

**Overall Status: ALL QTPs PASSED ✅**

---

## Contact

For questions regarding QTP test results:
- Review HUB logs in `HUB_Test_Logs/`
- Check Python test suite documentation
- Consult `SNC_REFERENCE_GUIDE.md`

---

**Last Updated:** 2025-01-18
**Status:** Complete and Verified
