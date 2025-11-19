# MATLAB Pure Tone Gain Analysis

## Overview

This directory contains MATLAB analysis scripts and results for the pure tone detection analog signal chain gain calculations. These simulations complement the LTspice circuit simulations by providing detailed analysis of amplifier gain requirements, MCP6024 op-amp limitations, and output signal level predictions.

## MATLAB Scripts

### 1. Gain_Pure_Tone.m

**Purpose**: Initial gain calculation and feasibility analysis for pure tone detection

**Description**: This script analyzes the complete gain chain required for pure tone detection from microphone input through to comparator threshold. It calculates required gain at each stage and validates feasibility with available op-amp characteristics.

**Analysis Performed**:

1. **Input Signal Level Estimation**:
   - Typical microphone output for 2800 Hz tone
   - Distance-dependent attenuation
   - Ambient noise floor considerations

2. **Required Gain Calculation**:
   - Preamplifier stage gain
   - Filter passband gain
   - Variable gain amplifier range
   - Total system gain requirement

3. **MCP6024 Limitations**:
   - Gain-bandwidth product constraints
   - Maximum gain at 2800 Hz
   - Slew rate limitations
   - Output swing capabilities

4. **Threshold Analysis**:
   - Comparator threshold selection
   - Hysteresis requirements
   - Noise margin calculations

**Key Outputs**:
- Required gain per stage
- Feasibility assessment for MCP6024
- Recommended gain distribution
- Output signal level predictions

---

### 2. Gain_Pure_Tone_2.m

**Purpose**: Refined gain analysis with updated component selections

**Description**: This is an updated version of the gain analysis incorporating refined component values and design decisions made after initial prototyping. It includes actual measured values and updated MCP6024 characterization.

**Design Refinements**:
- Updated component values based on E12/E24 standard series
- Measured microphone sensitivity data
- Actual filter Q factor from prototype
- Revised threshold levels based on hardware testing

**Additional Analysis**:
- Gain variation across component tolerances
- Temperature effects on gain stages
- Supply voltage sensitivity
- Worst-case gain calculations

**Validation Against Hardware**:
This script includes validation comparing calculated values against measured prototype performance, confirming the analytical model accuracy.

---

## Analysis Results

### CSV Output Files

#### MCP6024_Analysis_Results.csv

**Description**: Comprehensive analysis of MCP6024 op-amp characteristics at 2800 Hz operating frequency.

**Data Columns**:
- Gain setting (resistor ratio)
- Calculated gain (dB)
- Bandwidth at gain setting
- Phase margin
- Output swing (Vpp)
- Suitability for pure tone application (Pass/Fail)

**Usage**: Reference this data when selecting resistor values for gain stages to ensure operation within MCP6024 specifications.

---

#### Filter_Comparison_Results.csv

**Description**: Comparison of various filter topologies considered for the 2800 Hz bandpass filter.

**Filter Types Analyzed**:
- Sallen-Key bandpass
- Multiple Feedback (MFB) bandpass
- State-variable filter
- Twin-T notch filter

**Comparison Criteria**:
- Component count
- Q factor achievability
- Sensitivity to component tolerances
- Power consumption
- Complexity

**Result**: MFB topology selected for implementation (see LTspice simulations).

---

### PDF Analysis Reports

#### MCP6024_Design_Decision.pdf

**Purpose**: Design decision documentation for op-amp selection and usage

**Contents**:
1. **Op-Amp Selection Rationale**:
   - Why MCP6024 was chosen over alternatives
   - Cost, availability, and specifications comparison
   - 3.3V single-supply operation requirement

2. **Gain Stage Configuration**:
   - Recommended gain per stage
   - Resistor value selections
   - Stability considerations

3. **Design Trade-offs**:
   - Gain-bandwidth vs. stage count
   - Noise vs. power consumption
   - Component tolerance sensitivity

**Conclusion**: Validates MCP6024 as appropriate for pure tone detection application with documented gain distribution strategy.

---

#### MCP6024_Frequency_Limitations.pdf

**Purpose**: Analysis of MCP6024 frequency response limitations at 2800 Hz

**Contents**:
1. **Gain-Bandwidth Product Analysis**:
   - GBW specification: 10 MHz typical
   - Maximum gain achievable at 2800 Hz
   - Phase shift at operating frequency

2. **Multi-Stage Considerations**:
   - Cumulative phase shift through gain chain
   - Stability margins for cascaded stages
   - Compensation requirements

3. **Frequency Response Verification**:
   - Calculated response vs. datasheet curves
   - Validation with LTspice AC analysis
   - Confirmation of adequate bandwidth

**Key Finding**: MCP6024 provides sufficient bandwidth for pure tone detection with proper gain distribution across multiple stages.

---

#### MCP6024_Output_Signal_Level.pdf

**Purpose**: Predicted output signal levels throughout the gain chain

**Contents**:
1. **Signal Level Budget**:
   - Input from microphone: -40 dBV to -20 dBV (estimated)
   - After preamplifier: Calculated level
   - After filter: Calculated level
   - After variable gain: Calculated level
   - At comparator input: Target level for reliable detection

2. **Dynamic Range Analysis**:
   - Minimum detectable signal
   - Maximum signal before clipping
   - Required dynamic range for reliable operation

3. **Headroom Calculations**:
   - Peak signal levels
   - Available headroom before distortion
   - Clipping margin with 3.3V supply

**Key Output**: Validates that output signal levels are sufficient for reliable comparator triggering while maintaining adequate headroom.

---

#### Feasibility_Map.pdf

**Purpose**: Visual representation of design feasibility across parameter space

**Description**: This document presents a graphical feasibility map showing the viable operating region for the pure tone detector considering all constraints simultaneously.

**Parameters Mapped**:
- Input signal level (x-axis)
- Required gain (y-axis)
- Color-coded regions:
  - Green: Feasible with margin
  - Yellow: Feasible but tight margins
  - Red: Not feasible (exceeds limitations)

**Constraints Visualized**:
- MCP6024 gain-bandwidth limit
- Maximum output swing
- Minimum SNR requirement
- Component tolerance sensitivity

**Usage**: Reference this map to understand the viable operating envelope and identify which constraints are most limiting.

---

## MATLAB Analysis Methodology

### Running the Scripts

1. **MATLAB Version**: R2019b or later recommended (compatible with earlier versions)

2. **Required Toolboxes**:
   - Signal Processing Toolbox (for filter analysis)
   - No additional toolboxes required for basic operation

3. **Execution**:
   ```matlab
   % Navigate to directory
   cd('/home/user/ERD320-SNC/Simulation/MATLAB_Simulations/Pure_Tone_Gain_Analysis')

   % Run gain analysis
   Gain_Pure_Tone

   % Run refined analysis
   Gain_Pure_Tone_2
   ```

4. **Outputs**:
   - Figures displaying gain calculations and frequency response
   - CSV files with numerical results
   - Console output with key design parameters

### Modifying Analysis Parameters

Both scripts can be modified to analyze different scenarios:

**Configurable Parameters**:
- Microphone sensitivity
- Desired detection range
- Filter center frequency
- Filter Q factor
- Comparator threshold voltage
- Supply voltage

**Procedure**:
1. Edit the parameter section at the top of the script
2. Re-run the analysis
3. Review updated outputs and feasibility assessment

---

## Integration with LTspice Simulations

The MATLAB gain analysis informs the LTspice circuit design:

**Analysis Flow**:
1. MATLAB: Calculate required gain distribution
2. MATLAB: Verify MCP6024 feasibility at each stage
3. LTspice: Implement circuit with calculated component values
4. LTspice: Validate frequency response and time-domain performance
5. Hardware: Build prototype and verify against both MATLAB predictions and LTspice simulations

**Consistency Validation**:
- MATLAB predicted gains match LTspice AC analysis
- Output signal levels consistent between MATLAB calculations and LTspice transient analysis
- MCP6024 limitations respected in both environments

**See Also**: `../../LTspice_Simulations/Pure_Tone_Detection/README.md`

---

## Design Decisions Based on Analysis

### Key Decisions Validated by MATLAB Analysis

1. **Op-Amp Selection**: MCP6024 confirmed suitable for 2800 Hz application with adequate GBW and 3.3V operation.

2. **Gain Distribution**: Multi-stage approach selected with distributed gain (moderate gain per stage) rather than single high-gain stage.

3. **Filter Topology**: MFB bandpass selected based on component count, Q factor, and simplicity.

4. **Variable Gain Requirement**: Analysis confirmed need for adjustable gain stage to accommodate varying input levels.

5. **Threshold Setting**: Comparator threshold voltage determined from output signal level analysis.

### Design Constraints Identified

1. **Maximum Practical Gain per Stage**: Limited to ~20-30 dB at 2800 Hz to maintain stability and bandwidth.

2. **Minimum Input Signal**: ~-40 dBV minimum detectable signal based on noise floor analysis.

3. **Maximum Input Signal**: ~-10 dBV maximum before requiring gain reduction to prevent clipping.

4. **Total Dynamic Range**: ~30 dB achievable with variable gain adjustment.

---

## Design Evolution

### Phase 1 Analysis (Gain_Pure_Tone.m)

Initial feasibility study established:
- Pure tone detection is feasible with MCP6024
- Three-stage gain approach recommended
- Filter Q factor requirements
- Initial component value estimates

### Phase 3 Refinement (Gain_Pure_Tone_2.m)

Updated analysis incorporating:
- Measured microphone characteristics
- Prototype filter Q factor data
- Actual MCP6024 performance at 3.3V
- Refined threshold requirements

### Result

The refined analysis validated the Phase 1 design approach while providing updated component values for final implementation. Both versions preserved in this directory for design traceability.

---

## Validation Against Hardware

### Comparison with Physical Prototype

The MATLAB predictions were validated against the physical prototype:

**Validation Points**:
1. **Gain Measurement**: Measured gain per stage within 2 dB of calculated values
2. **Frequency Response**: Center frequency measured at 2805 Hz (target: 2800 Hz)
3. **Output Levels**: Peak detector output within 10% of predicted value
4. **Detection Threshold**: Comparator triggers at predicted input level

**Discrepancies**: Minor variations attributed to component tolerances and parasitic capacitances not modeled in MATLAB.

**Conclusion**: MATLAB analysis provides accurate predictions suitable for design decision-making.

---

## References

- **MCP6024 Datasheet**: Microchip Technology Inc.
- **LTspice Simulations**: `../../LTspice_Simulations/Pure_Tone_Detection/`
- **Python Test Suite**: `../../Python_Tests/Command_Tests/test_pure_tone.py`
- **Project Report**: ERD320_Report.pdf
- **QTP Specification**: QTP-SNC-07 (Pure Tone Detection)

---

## Notes

- All analysis assumes ideal component values. Hardware implementation requires tolerance analysis.
- MCP6024 model parameters taken from datasheet typical values. Production circuits should consider worst-case specifications.
- Temperature effects not explicitly modeled. Operating temperature range assumed to be 0-50°C.
- Supply voltage assumed to be 3.3V ±5%. Significant deviations require re-analysis.
- All documentation follows formal language guidelines without emojis per project requirements.
