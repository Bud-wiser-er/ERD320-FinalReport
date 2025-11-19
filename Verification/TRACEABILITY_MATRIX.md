# Requirements Traceability Matrix

**Complete Requirement-to-Evidence Mapping**
**SNC Subsystem - ERD320 AMazeEng MARV**
**Last Updated:** 2025-01-18

---

## Purpose

This traceability matrix provides complete mapping between:
- Requirements (QTPs, design specifications)
- Implementation (source code, hardware)
- Verification evidence (tests, measurements, analysis)

This ensures every requirement has been implemented and verified with documented evidence.

---

## QTP Requirements Traceability

### QTP-SNC-01: IDLE -> CAL State Transition

**Requirement:** System shall transition from IDLE to CAL state when touch sensor is activated.

**Implementation:**
- Source Code: `Phase3/Phase3/Phase3.ino:setup()`, `system_state.cpp:handleIdleState()`
- Hardware: Capacitive touch sensor on ESP32 GPIO pin

**Verification Evidence:**

| Evidence Type | Location | Status |
|---------------|----------|--------|
| Python Test | `Python_Test_Suite/Command_Test_Results/test_idle_results.txt` | PASS |
| HUB Log | `QTP_Test_Results/HUB_Test_Logs/All_QTP_Complete_Logs.txt` (search: QTP-SNC-01) | PASS |
| QTP Documentation | `QTP_Test_Results/README.md#qtp-snc-01` | Complete |
| Integration Test | `Phase3_Compliance/Integration_Test_Logs/` | PASS |

**Traceability:** COMPLETE - Requirement -> Implementation -> Verification

---

### QTP-SNC-02: CAL -> MAZE State Transition

**Requirement:** System shall transition from CAL to MAZE state after successful calibration sequence.

**Implementation:**
- Source Code: `Phase3/Phase3/system_state.cpp:handleCalState()`
- Calibration Sequence: SS calibration -> MDPS calibration -> MAZE transition

**Verification Evidence:**

| Evidence Type | Location | Status |
|---------------|----------|--------|
| Python Test | `Python_Test_Suite/Command_Test_Results/test_cal_results.txt` | PASS |
| HUB Log | `QTP_Test_Results/HUB_Test_Logs/All_QTP_Complete_Logs.txt` (search: QTP-SNC-02) | PASS |
| QTP Documentation | `QTP_Test_Results/README.md#qtp-snc-02` | Complete |
| State Machine Verification | `Phase3_Compliance/NAVCON_TEST_COMPLIANCE_REVIEW.md` | PASS |

**Traceability:** COMPLETE - Requirement -> Implementation -> Verification

---

### QTP-SNC-03: NAVCON Forward Navigation Logic

**Requirement:** NAVCON algorithm shall correctly determine forward motion commands based on sensor input.

**Implementation:**
- Source Code: `Phase3/Phase3/navcon_core.cpp:determineNavconAction()`
- Algorithm: Angle and color-based decision logic
- Motion Primitives: FORWARD command generation

**Verification Evidence:**

| Evidence Type | Location | Status |
|---------------|----------|--------|
| Python Test | `Python_Test_Suite/NAVCON_Test_Results/navcon_decision_matrix.txt` | PASS (125 scenarios) |
| MATLAB Analysis | `MATLAB_Simulations/NAVCON_Geometric_Analysis/` | Validated |
| HUB Log | `QTP_Test_Results/HUB_Test_Logs/All_QTP_Complete_Logs.txt` (search: QTP-SNC-03) | PASS |
| Compliance Review | `Phase3_Compliance/NAVCON_TEST_COMPLIANCE_REVIEW.md` | Complete |
| QTP Documentation | `QTP_Test_Results/README.md#qtp-snc-03` | Complete |

**Traceability:** COMPLETE - Requirement -> Implementation -> Verification -> Analysis

---

### QTP-SNC-04: NAVCON Rotation Decision Logic

**Requirement:** NAVCON algorithm shall correctly determine rotation commands when forward motion is not valid.

**Implementation:**
- Source Code: `Phase3/Phase3/navcon_core.cpp:determineNavconAction()`
- Algorithm: RED/GREEN/BLACK color priority, rotation selection
- Motion Primitives: ROTATE_LEFT, ROTATE_RIGHT, TURN_180

**Verification Evidence:**

| Evidence Type | Location | Status |
|---------------|----------|--------|
| Python Test | `Python_Test_Suite/NAVCON_Test_Results/navcon_decision_matrix.txt` | PASS (125 scenarios) |
| MATLAB Analysis | `MATLAB_Simulations/NAVCON_Geometric_Analysis/` | Validated |
| HUB Log | `QTP_Test_Results/HUB_Test_Logs/All_QTP_Complete_Logs.txt` (search: QTP-SNC-04) | PASS |
| Compliance Review | `Phase3_Compliance/NAVCON_TEST_COMPLIANCE_REVIEW.md` | Complete |
| Edge Case Testing | `Phase3_Compliance/EDGE_CASE_DOCUMENTATION.md` | Complete |

**Traceability:** COMPLETE - Requirement -> Implementation -> Verification -> Analysis

---

### QTP-SNC-05: SCS Protocol Compliance

**Requirement:** All SCS protocol packets shall be properly formatted and follow state machine rules.

**Implementation:**
- Source Code: `Phase3/Phase3/scs_protocol.cpp`, `spi_protocol_impl.cpp`
- Protocol: SYS:SUB:IST packet format
- State Machine: IDLE, CAL, MAZE, SOS states

**Verification Evidence:**

| Evidence Type | Location | Status |
|---------------|----------|--------|
| Python Test (All Commands) | `Python_Test_Suite/Command_Test_Results/` | PASS (47 commands) |
| Protocol Specification | `Evidence_Archive/SCS_Protocol_Spec.md` | Complete |
| HUB Log | `QTP_Test_Results/HUB_Test_Logs/All_QTP_Complete_Logs.txt` | PASS (1000+ packets) |
| Packet Validation | `Python_Test_Suite/README.md` | ✅ 100% coverage |

**Traceability:** COMPLETE - Requirement -> Implementation -> Verification

---

### QTP-SNC-06: Pure Tone Detection (2800 Hz, 4000 Hz)

**Requirement:** System shall detect pure tones at 2800 Hz and 4000 Hz with proper timing.

**Implementation:**
- Hardware: Cascaded MFB bandpass filter, microphone input
- Source Code: `Phase3/Phase3/Phase3.ino:detectPureTone()` (Goertzel algorithm)
- Timing: 500-1000ms duration validation

**Verification Evidence:**

| Evidence Type | Location | Status |
|---------------|----------|--------|
| MATLAB Gain Analysis | `MATLAB_Simulations/Pure_Tone_Gain_Analysis/` (6 PDFs) | Validated |
| Laboratory Testing | `Phase2_Lab_Results/Pure_Tone_Oscilloscope/` (33 files) | PASS |
| Circuit Schematics | `Simulation/LTspice_Simulations/Pure_Tone_Detection/` | Complete |
| Python Test | `Python_Test_Suite/Command_Test_Results/test_pure_tone_results.txt` | PASS |
| QTP Documentation | `QTP_Test_Results/README.md#qtp-snc-06` | Complete |

**Traceability:** COMPLETE - Requirement -> Design -> Hardware -> Software -> Verification

---

### QTP-SNC-07: MAZE <-> SOS State Toggle

**Requirement:** Dual-tone sequence shall toggle between MAZE and SOS states.

**Implementation:**
- Source Code: `Phase3/Phase3/system_state.cpp:handlePureToneToggle()`
- Logic: Detect 2800 Hz followed by 4000 Hz within 2s window
- State: Bidirectional toggle (MAZE <-> SOS)

**Verification Evidence:**

| Evidence Type | Location | Status |
|---------------|----------|--------|
| Python Test | `Python_Test_Suite/Command_Test_Results/test_pure_tone_results.txt` | PASS |
| Laboratory Testing | `Phase2_Lab_Results/Pure_Tone_Oscilloscope/` | PASS |
| HUB Log | `QTP_Test_Results/HUB_Test_Logs/All_QTP_Complete_Logs.txt` (search: QTP-SNC-07) | PASS |
| State Machine Verification | `Phase3_Compliance/NAVCON_TEST_COMPLIANCE_REVIEW.md` | Complete |

**Traceability:** COMPLETE - Requirement -> Implementation -> Verification

---

### QTP-SNC-08: WiFi Telemetry Transmission

**Requirement:** SNC shall transmit telemetry data via WiFi to monitoring system.

**Implementation:**
- Source Code: `Phase3/Phase3/ESP32_Wifi_coms/` WiFi communication modules
- Protocol: TCP/IP telemetry packets
- Hardware: ESP32 WiFi radio

**Verification Evidence:**

| Evidence Type | Location | Status |
|---------------|----------|--------|
| HUB Log | `QTP_Test_Results/HUB_Test_Logs/All_QTP_Complete_Logs.txt` (search: QTP-SNC-08) | PASS |
| Integration Test | `Phase3_Compliance/Integration_Test_Logs/` | PASS |
| QTP Documentation | `QTP_Test_Results/README.md#qtp-snc-08` | Complete |

**Traceability:** COMPLETE - Requirement -> Implementation -> Verification

---

### QTP-SNC-09: Main Loop Timing Requirements

**Requirement:** Main control loop shall execute within specified timing constraints.

**Implementation:**
- Source Code: `Phase3/Phase3/Phase3.ino:loop()` main control loop
- Timing: Real-time constraints enforced
- Performance: Loop execution monitored

**Verification Evidence:**

| Evidence Type | Location | Status |
|---------------|----------|--------|
| HUB Log | `QTP_Test_Results/HUB_Test_Logs/All_QTP_Complete_Logs.txt` (search: QTP-SNC-09) | PASS |
| Integration Test | `Phase3_Compliance/Integration_Test_Logs/` | PASS (avg: 45ms, max: 78ms) |
| QTP Documentation | `QTP_Test_Results/README.md#qtp-snc-09` | Complete |

**Traceability:** COMPLETE - Requirement -> Implementation -> Verification

---

### QTP-SNC-10: End-of-Maze Detection and Handling

**Requirement:** System shall detect end-of-maze signal and handle appropriately.

**Implementation:**
- Source Code: `Phase3/Phase3/system_state.cpp:handleMazeState()`
- Protocol: MAZE:SS:3 packet detection
- Action: Stop motion commands, enter safe state

**Verification Evidence:**

| Evidence Type | Location | Status |
|---------------|----------|--------|
| Python Test | `Python_Test_Suite/Command_Test_Results/test_maze_ss_results.txt` | PASS |
| HUB Log | `QTP_Test_Results/HUB_Test_Logs/All_QTP_Complete_Logs.txt` (search: QTP-SNC-10) | PASS |
| QTP Documentation | `QTP_Test_Results/README.md#qtp-snc-10` | Complete |

**Traceability:** COMPLETE - Requirement -> Implementation -> Verification

---

## Design Requirements Traceability

### Pure Tone Detection Circuit

**Requirement:** Detect 2800 Hz and 4000 Hz tones with >20 dB gain

**Design Analysis:**

| Design Phase | Evidence | Location | Status |
|--------------|----------|----------|--------|
| Gain Calculation | MATLAB Analysis | `MATLAB_Simulations/Pure_Tone_Gain_Analysis/Feasibility_Map.pdf` | Complete |
| Op-Amp Selection | MATLAB Analysis | `MATLAB_Simulations/Pure_Tone_Gain_Analysis/MCP6024_Design_Decision.pdf` | Validated |
| Frequency Limitations | MATLAB Analysis | `MATLAB_Simulations/Pure_Tone_Gain_Analysis/MCP6024_Frequency_Limitations.pdf` | ✅ No issues |
| Output Levels | MATLAB Analysis | `MATLAB_Simulations/Pure_Tone_Gain_Analysis/MCP6024_Output_Signal_Level.pdf` | ✅ Optimal |
| Circuit Simulation | LTspice | `Simulation/LTspice_Simulations/Pure_Tone_Detection/` | Complete |

**Hardware Validation:**

| Test | Evidence | Location | Result |
|------|----------|----------|--------|
| Frequency Response | Oscilloscope | `Phase2_Lab_Results/Pure_Tone_Oscilloscope/Cascaded MFB/` | ✅ 24 dB @ 2800 Hz, 4000 Hz |
| Time Domain @ 2800 Hz | Oscilloscope | `Phase2_Lab_Results/Pure_Tone_Oscilloscope/Time Domain 2800/` | ✅ Clean signal |
| Time Domain @ 4000 Hz | Oscilloscope | `Phase2_Lab_Results/Pure_Tone_Oscilloscope/Time Domain 4000/` | ✅ Clean signal |
| Microphone Integration | Oscilloscope | `Phase2_Lab_Results/Pure_Tone_Oscilloscope/New MFB second order combined with MIC/` | ✅ Functional |

**Software Validation:**

| Test | Evidence | Location | Result |
|------|----------|----------|--------|
| Goertzel Algorithm | Python Test | `Python_Test_Suite/Command_Test_Results/test_pure_tone_results.txt` | PASS |
| Dual-Tone Detection | Python Test | `Python_Test_Suite/Command_Test_Results/test_pure_tone_results.txt` | PASS |

**Traceability:** COMPLETE - Requirement -> MATLAB -> LTspice -> Lab -> Software -> QTP

---

### NAVCON Algorithm

**Requirement:** Autonomous navigation using sensor incidence angles and color detection

**Design Analysis:**

| Design Phase | Evidence | Location | Status |
|--------------|----------|----------|--------|
| Geometric Analysis | MATLAB | `MATLAB_Simulations/NAVCON_Geometric_Analysis/` | Complete |
| Angle Thresholds | MATLAB | Incidence angle calculations (5 degrees, 45 degrees justified) | Validated |
| Decision Matrix | Documentation | `Phase3_Compliance/NAVCON_TEST_COMPLIANCE_REVIEW.md` | Complete |

**Software Implementation:**

| Component | Evidence | Location | Status |
|-----------|----------|----------|--------|
| Core Algorithm | Source Code | `Phase3/Phase3/navcon_core.cpp` | ✅ Implemented |
| Color Processing | Source Code | `Phase3/Phase3/navcon_core.cpp:processColorData()` | ✅ Implemented |
| Angle Processing | Source Code | `Phase3/Phase3/navcon_core.cpp:processAngleData()` | ✅ Implemented |
| Decision Logic | Source Code | `Phase3/Phase3/navcon_core.cpp:determineNavconAction()` | ✅ Implemented |

**Verification:**

| Test Type | Evidence | Location | Result |
|-----------|----------|----------|--------|
| All Angles (0-90 degrees) | Python Test | `Python_Test_Suite/NAVCON_Test_Results/` | ✅ 100% (91 scenarios) |
| All Colors (5³) | Python Test | `Python_Test_Suite/NAVCON_Test_Results/` | ✅ 100% (125 scenarios) |
| Forward Navigation | Python Test | QTP-SNC-03 validation | PASS |
| Rotation Logic | Python Test | QTP-SNC-04 validation | PASS |
| Edge Cases | Documentation | `Phase3_Compliance/EDGE_CASE_DOCUMENTATION.md` | ✅ All handled |

**Traceability:** COMPLETE - Requirement -> MATLAB -> Implementation -> Testing -> QTP

---

### SCS Protocol Implementation

**Requirement:** Implement SCS (Shared Communication Standard) protocol for subsystem communication

**Protocol Specification:**

| Component | Evidence | Location | Status |
|-----------|----------|----------|--------|
| Protocol Spec | Documentation | `Evidence_Archive/SCS_Protocol_Spec.md` | Complete |
| Packet Format | Documentation | SYS:SUB:IST format defined | Complete |
| State Machine | Documentation | IDLE, CAL, MAZE, SOS states | Complete |

**Software Implementation:**

| Component | Evidence | Location | Status |
|-----------|----------|----------|--------|
| Protocol Core | Source Code | `Phase3/Phase3/scs_protocol.cpp` | ✅ Implemented |
| SPI Communication | Source Code | `Phase3/Phase3/spi_protocol_impl.cpp` | ✅ Implemented |
| State Management | Source Code | `Phase3/Phase3/system_state.cpp` | ✅ Implemented |

**Verification:**

| Test | Evidence | Location | Result |
|------|----------|----------|--------|
| All 47 Commands | Python Test | `Python_Test_Suite/` | ✅ 100% coverage |
| Packet Format | Python Test | Control byte, data bytes validated | PASS |
| State Transitions | Python Test | All valid transitions tested | PASS |
| Protocol Compliance | QTP | QTP-SNC-05 | PASS |

**Traceability:** COMPLETE - Specification -> Implementation -> Testing -> QTP

---

## Cross-Verification Matrix

### Evidence Cross-References

| Requirement | MATLAB | LTspice | Lab | Python | QTP | Status |
|-------------|--------|---------|-----|--------|-----|--------|
| Pure Tone Detection | ✅ 6 PDFs | ✅ Circuit | ✅ 33 files | ✅ Tests | ✅ QTP-06,07 | Complete |
| NAVCON Algorithm | ✅ Geometry | - | - | ✅ 125 tests | ✅ QTP-03,04 | Complete |
| SCS Protocol | - | - | - | ✅ 47 cmds | ✅ QTP-05 | Complete |
| State Machine | - | - | - | ✅ Tests | ✅ QTP-01,02 | Complete |
| WiFi Telemetry | - | - | - | - | ✅ QTP-08 | Complete |
| Main Loop Timing | - | - | - | - | ✅ QTP-09 | Complete |
| End-of-Maze | - | - | - | ✅ Tests | ✅ QTP-10 | Complete |

**All Requirements: 100% Cross-Verified ✅**

---

## Component Traceability

### Hardware Components

| Component | Specification | Design | Implementation | Verification | Status |
|-----------|--------------|--------|----------------|--------------|--------|
| MCP6024 Op-Amp | Datasheet | MATLAB analysis | Veroboard circuit | Lab testing | Validated |
| MFB Bandpass Filter | Theory | MATLAB + LTspice | Veroboard circuit | Oscilloscope | Validated |
| Electret Microphone | Datasheet | Signal level analysis | PCB integration | Lab testing | Validated |
| ESP32-WROOM-32 | Datasheet | System design | Development board | Full testing | Validated |
| Touch Sensor | ESP32 feature | Software design | GPIO config | Python tests | Validated |

---

### Software Modules

| Module | Specification | Implementation | Unit Test | Integration Test | Status |
|--------|--------------|----------------|-----------|------------------|--------|
| `navcon_core.cpp` | NAVCON spec | ✅ Code | ✅ Python | ✅ Dual-port | Complete |
| `system_state.cpp` | State machine | ✅ Code | ✅ Python | ✅ HUB logs | Complete |
| `scs_protocol.cpp` | SCS spec | ✅ Code | ✅ Python | ✅ HUB logs | Complete |
| `spi_protocol_impl.cpp` | SPI spec | ✅ Code | ✅ Python | ✅ HUB logs | Complete |
| Pure tone detection | Algorithm spec | ✅ Code | ✅ Python | ✅ Lab + QTP | Complete |

---

## Verification Method Traceability

### Test Method Coverage

| Requirement | Analysis | Simulation | Lab | Software Test | Integration | QTP | Status |
|-------------|----------|------------|-----|---------------|-------------|-----|--------|
| QTP-SNC-01 | - | - | - | ✅ | ✅ | ✅ | Complete |
| QTP-SNC-02 | - | - | - | ✅ | ✅ | ✅ | Complete |
| QTP-SNC-03 | ✅ | - | - | ✅ | ✅ | ✅ | Complete |
| QTP-SNC-04 | ✅ | - | - | ✅ | ✅ | ✅ | Complete |
| QTP-SNC-05 | - | - | - | ✅ | ✅ | ✅ | Complete |
| QTP-SNC-06 | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ | Complete |
| QTP-SNC-07 | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ | Complete |
| QTP-SNC-08 | - | - | - | - | ✅ | ✅ | Complete |
| QTP-SNC-09 | - | - | - | - | ✅ | ✅ | Complete |
| QTP-SNC-10 | - | - | - | ✅ | ✅ | ✅ | Complete |

**All Requirements: Multiple Verification Methods ✅**

---

## Document Traceability

### Evidence Documentation

| Document | Purpose | Location | Status |
|----------|---------|----------|--------|
| Master Verification Index | Overall structure | `Verification/README.md` | Complete |
| Verification Summary | Executive summary | `Verification/VERIFICATION_SUMMARY.md` | Complete |
| Traceability Matrix | This document | `Verification/TRACEABILITY_MATRIX.md` | Complete |
| QTP Test Results | QTP evidence | `Verification/QTP_Test_Results/README.md` | Complete |
| Phase 2 Lab Results | Hardware validation | `Verification/Phase2_Lab_Results/README.md` | Complete |
| Python Test Suite | Software testing | `Verification/Python_Test_Suite/README.md` | Complete |
| MATLAB Simulations | Design analysis | `Verification/MATLAB_Simulations/README.md` | Complete |
| Phase 3 Compliance | Final verification | `Verification/Phase3_Compliance/README.md` | Complete |

**All Documentation: Complete and Cross-Referenced ✅**

---

## Gap Analysis

### Verification Coverage

**Analysis:** All requirements have been verified through multiple methods.
**Gaps:** None identified
**Status:** ✅ 100% Coverage

### Evidence Gaps

**Analysis:** All evidence documented and archived.
**Gaps:** None identified
**Status:** Complete Evidence Package

### Traceability Gaps

**Analysis:** All requirements traced to implementation and verification.
**Gaps:** None identified
**Status:** Complete Traceability

---

## Summary

### Traceability Status

- **QTP Requirements:** 10/10 traced (100%)
- **Design Requirements:** All traced (100%)
- **Hardware Components:** All traced (100%)
- **Software Modules:** All traced (100%)
- **Verification Methods:** All traced (100%)
- **Documentation:** All complete (100%)

### Overall Traceability

**✅ 100% COMPLETE TRACEABILITY**

Every requirement has been:
1. ✅ Analyzed (MATLAB, specifications)
2. ✅ Designed (circuit schematics, algorithms)
3. ✅ Implemented (hardware, software)
4. ✅ Verified (tests, measurements)
5. ✅ Documented (evidence, reports)

---

## Contact

For questions regarding traceability:
- Review this matrix for requirement-to-evidence mapping
- Consult individual README files for detailed evidence
- Check cross-references for related documentation

---

**Last Updated:** 2025-01-18
**Status:** Complete
**Traceability Coverage:** 100%
