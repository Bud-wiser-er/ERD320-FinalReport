# LTspice Pure Tone Detection Simulations

## Overview

This directory contains LTspice circuit simulations for the pure tone detection analog signal chain. These simulations model the complete signal path from microphone input through amplification, filtering, and detection stages required to identify 2800 Hz dual-tone signals in the AMazeEng MARV robot.

## Simulation Files

### 1. FullSim.asc

**Purpose**: Complete pure tone detection circuit simulation (Phase 1 design)

**Description**: This simulation models the full analog signal processing chain for detecting 2800 Hz pure tone signals. The circuit implements the original design specification developed during Phase 1.

**Circuit Stages**:

1. **Microphone Preamplifier**:
   - Electret microphone interface
   - Initial gain stage for weak audio signals
   - Bias network for microphone power

2. **Bandpass Filter**:
   - Center frequency: 2800 Hz
   - Tuned for pure tone detection
   - Rejects ambient noise and other frequencies
   - See Filter/ subdirectory for detailed filter analysis

3. **Variable Gain Amplifier**:
   - Adjustable gain for signal conditioning
   - Compensates for varying tone amplitudes
   - Optimized for MCP6024 op-amp characteristics

4. **Peak Detector**:
   - Envelope detection for tone presence
   - Fast attack, controlled decay
   - Provides DC level proportional to tone amplitude

5. **Comparator Stage**:
   - Threshold detection for digital output
   - Hysteresis to prevent false triggering
   - Logic-level output to ESP32 GPIO

**Key Parameters**:
- Operating frequency: 2800 Hz
- Detection threshold: Configurable (typically -20 dBV to 0 dBV)
- Supply voltage: 3.3V (ESP32 compatible)
- Tone duration validation: 500-1000 ms
- Dual-tone gap requirement: < 2000 ms

**Analysis Types**:
- AC analysis: Frequency response verification
- Transient analysis: Tone detection timing
- Operating point: DC bias validation

---

### 2. FullSim_connected.asc

**Purpose**: Updated pure tone detection circuit for veroboard implementation (Phase 3 design)

**Description**: This is the refined simulation reflecting the actual circuit built on veroboard. It incorporates practical component values, parasitic effects, and design modifications made during physical implementation.

**Design Updates from FullSim.asc**:
- Component values adjusted for standard E12/E24 series
- Op-amp model updated to match actual MCP6024 used
- Additional decoupling capacitors for stability
- Connector parasitics modeled
- Layout-induced capacitance included

**Veroboard Implementation Reference**:
- See `Veroboard_Stripboard-Layout-1.pdf` for physical layout
- Stripboard tracks modeled in simulation
- Grounding scheme validated
- Component placement optimized for minimal noise coupling

**Validation**:
This simulation was validated against physical prototype measurements to ensure accuracy. Key validation points:
- Frequency response matches measured data
- Detection threshold consistent with hardware
- Timing characteristics verified
- Noise immunity confirmed

---

### 3. Draft1.asc

**Purpose**: Early prototype circuit (exploratory design)

**Description**: Initial exploratory design for pure tone detection. This draft represents early design iterations before finalization of the FullSim circuit. Preserved for design evolution reference.

**Status**: Superseded by FullSim.asc and FullSim_connected.asc

---

## Filter Subdirectory

### Filter/MFB.asc

**Purpose**: Multiple Feedback (MFB) bandpass filter detailed analysis

**Description**: Isolated simulation of the MFB bandpass filter stage tuned to 2800 Hz. This simulation provides detailed analysis of the filter characteristics including:

- Frequency response (magnitude and phase)
- Q factor verification
- Stopband rejection analysis
- Component sensitivity analysis

**MFB Filter Characteristics**:
- Topology: Multiple Feedback (MFB) bandpass
- Center frequency (f0): 2800 Hz
- Bandwidth: Optimized for pure tone selectivity
- Passband gain: Configurable
- Stopband rejection: > 40 dB at ±500 Hz

**Design Advantages**:
- Single op-amp implementation (low power)
- High Q factor achievable
- Low component count
- Excellent stability

### Filter Analysis Plots

The Filter/ subdirectory includes simulation output plots:

- **MFB_Filter1.emf**: Magnitude response
- **MFB_Filter2.emf**: Phase response
- **MFB_Response.emf**: Combined frequency response

These plots demonstrate the filter meets the 2800 Hz detection requirements with adequate selectivity and stopband rejection.

---

## Veroboard Physical Implementation

### Veroboard_Stripboard-Layout-1.pdf

**Purpose**: Physical circuit layout documentation

**Description**: This document shows the actual stripboard layout used for prototype construction. It maps the schematic (FullSim_connected.asc) to physical component placement and track routing.

**Contents**:
- Component placement diagram
- Track cutting locations
- Wire jumper routing
- Power supply connections
- Signal input/output connections

**Usage**: Reference this layout when constructing the physical pure tone detector circuit or when correlating simulation results with hardware measurements.

---

## Pure Tone Detection Requirements

The pure tone detection system must identify valid dual-tone sequences according to these specifications:

### Timing Requirements

1. **Single Tone Duration**: 500 ms to 1000 ms
2. **Inter-tone Gap**: < 2000 ms (between first and second tone)
3. **Frequency Tolerance**: ±50 Hz from 2800 Hz center frequency

### Validation Logic

- **Valid Sequence**: Two consecutive tones, each 500-1000 ms, separated by <2s
- **Rejection Cases**:
  - Single tone only (no second tone detected)
  - Tone duration < 500 ms (too short)
  - Tone duration > 1000 ms (too long)
  - Inter-tone gap > 2000 ms (timeout)

### System Integration

The analog detector output connects to ESP32 GPIO for digital timing validation. The ESP32 firmware implements the duration and gap checking logic, triggering the MAZE to SOS state transition upon valid dual-tone detection.

---

## Simulation Methodology

### Running Simulations

1. **Open LTspice XVII** (or compatible version)

2. **Load simulation file**: Open .asc file

3. **Run analysis**:
   - AC Sweep: Analyze frequency response
   - Transient: Validate tone detection timing
   - Operating Point: Check DC bias levels

4. **Probe signals**: Click on nets to display waveforms

5. **Verify performance**: Compare results against specifications

### Key Nodes to Probe

- **IN**: Microphone input signal
- **FILT_OUT**: Bandpass filter output
- **AMP_OUT**: Variable gain amplifier output
- **DET_OUT**: Peak detector output
- **COMP_OUT**: Digital comparator output (to ESP32)

### Expected Results

- **AC Analysis**: Peak response at 2800 Hz with >40 dB stopband rejection
- **Transient Analysis**: Clean detection of 700 ms tone with proper envelope
- **Detection Latency**: < 10 ms from tone start to digital output assertion

---

## Integration with Python Test Suite

The LTspice simulations model the analog hardware that interfaces with the SNC subsystem. For software-level pure tone testing, see:

**Python Test Application**: `../../Python_Tests/Command_Tests/test_pure_tone.py`

The Python tester validates the complete pure tone detection logic including:
- Dual-tone sequence validation
- Duration checking (500-1000 ms)
- Gap timeout detection (>2s)
- MAZE to SOS state transitions

**Combined Validation Approach**:
1. LTspice: Validate analog detection circuit meets frequency and amplitude specs
2. Python Test: Validate digital timing logic and state machine integration
3. Hardware Test: Validate complete system with actual tone sources

---

## Design Background

The pure tone detection system serves as an emergency signal for the AMazeEng MARV robot. When a valid 2800 Hz dual-tone sequence is detected, the robot transitions from MAZE state to SOS state, triggering emergency protocols.

**Design Considerations**:

1. **Frequency Selection**: 2800 Hz chosen to avoid common ambient noise frequencies while remaining easily generated by standard tone sources.

2. **Dual-Tone Validation**: Two consecutive tones required to prevent false triggering from random noise or single transient sounds.

3. **Duration Windows**: 500-1000 ms duration requirement ensures intentional tone generation while preventing false detection from brief audio events.

4. **Gap Timeout**: 2000 ms maximum gap ensures both tones are part of the same intentional sequence.

5. **Analog Implementation**: Hardware filtering provides robustness and reduces computational load on the ESP32.

---

## MATLAB Integration

For gain calculation analysis and frequency response optimization, see:

**MATLAB Gain Analysis**: `../../MATLAB_Simulations/Pure_Tone_Gain_Analysis/README.md`

The MATLAB simulations complement these LTspice circuits by providing:
- Gain stage optimization for MCP6024 op-amp
- Frequency response feasibility analysis
- Design decision validation
- Output signal level predictions

---

## References

- **Project Report**: ERD320_Report.pdf (main project documentation)
- **QTP Specification**: QTP-SNC-07 (Pure Tone Detection validation)
- **ESP32 Firmware**: SNC subsystem firmware implementing digital validation logic
- **MCP6024 Datasheet**: Op-amp specifications and limitations

---

## Notes

- All simulations use standard LTspice component models
- MCP6024 op-amp model may require download from manufacturer
- Simulation results validated against physical prototype measurements
- Component tolerances not explicitly modeled - use worst-case analysis for production
- All documentation follows formal language guidelines without emojis per project requirements
