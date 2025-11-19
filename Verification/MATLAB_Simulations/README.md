# MATLAB Simulation Results

**Mathematical Analysis and Algorithm Validation**
**SNC Subsystem - ERD320 AMazeEng MARV**
**Last Updated:** 2025-01-18

---

## Overview

This directory contains comprehensive MATLAB simulation results that provide mathematical analysis and validation for critical SNC subsystem components. MATLAB simulations were used to:

1. **Design Validation:** Verify analog circuit designs before hardware implementation
2. **Performance Prediction:** Calculate expected system performance
3. **Algorithm Development:** Develop and validate NAVCON navigation algorithm
4. **Design Decision Support:** Provide quantitative justification for design choices

### Simulation Scope

- **Pure Tone Gain Analysis:** Amplifier design for tone detection circuit
- **NAVCON Geometric Analysis:** Navigation algorithm geometric modeling

---

## Pure Tone Gain Analysis

**Directory:** `Pure_Tone_Gain_Analysis/`

### Purpose

Design and validate the analog amplifier chain for pure tone detection (2800 Hz, 4000 Hz) to ensure:
- Sufficient gain for microphone signals
- Adequate signal levels for ESP32 ADC input
- Proper component selection (op-amp feasibility)
- Realistic performance expectations

### Analysis Files

#### 1. Feasibility Map

**File:** `Feasibility_Map.pdf`

**Analysis Content:**
- Overall system feasibility analysis
- Input signal levels (microphone output)
- Required ADC input levels
- Total gain requirements
- Feasibility boundary conditions

**Key Results:**
- **Microphone Output:** 1-10 mV RMS (typical for electret mic at 70 dB SPL)
- **ADC Input Requirement:** 100 mV - 1 V (ESP32 ADC optimal range)
- **Required Gain:** 40-60 dB
- **Feasibility:** ✅ Achievable with cascaded op-amp stages

**Conclusion:** Pure tone detection system is feasible with MCP6024 op-amp

---

#### 2. MCP6024 Design Decision

**File:** `MCP6024_Design_Decision.pdf`

**Analysis Content:**
- Op-amp selection criteria
- MCP6024 specifications review
- Comparison with alternative op-amps
- Design decision justification

**MCP6024 Specifications:**
- **Supply Voltage:** 2.7V - 6.0V (compatible with 3.3V ESP32)
- **Gain-Bandwidth Product:** 10 MHz
- **Slew Rate:** 4 V/μs
- **Input Offset Voltage:** <1 mV
- **Quad Op-Amp Package:** Cost-effective

**Key Results:**
- MCP6024 suitable for audio frequency amplification
- Bandwidth adequate for 2800 Hz and 4000 Hz
- Low noise performance
- Single power supply operation (rail-to-rail I/O)
- Cost-effective solution

**Decision:** ✅ MCP6024 selected for final design

---

#### 3. MCP6024 Frequency Limitations

**File:** `MCP6024_Frequency_Limitations.pdf`

**Analysis Content:**
- Gain-bandwidth product analysis
- Frequency response at different gain settings
- Phase margin analysis
- Stability considerations

**Calculations:**

**Stage 1 (Gain = 20 dB = 10×):**
- Available bandwidth: 10 MHz / 10 = 1 MHz
- Operating frequency: 2800-4000 Hz
- Bandwidth margin: 1 MHz / 4 kHz = 250× margin ✓

**Stage 2 (Gain = 20 dB = 10×):**
- Available bandwidth: 10 MHz / 10 = 1 MHz
- Cascaded response: Well within limits ✓

**Key Results:**
- ✅ Frequency range (2800-4000 Hz) well within capabilities
- ✅ Gain settings do not exceed GBW limitations
- ✅ Adequate phase margin for stability
- ✅ No frequency-dependent gain roll-off at operating frequencies

**Conclusion:** MCP6024 has no frequency limitations for this application

---

#### 4. MCP6024 Output Signal Level

**File:** `MCP6024_Output_Signal_Level.pdf`

**Analysis Content:**
- Output voltage swing analysis
- Signal level predictions
- ADC input compatibility
- Clipping risk assessment

**Signal Chain Analysis:**

**Input Stage:**
- Microphone output: 1-10 mV RMS
- Worst case (quiet tone): 1 mV RMS

**After Stage 1 (Gain = 20 dB):**
- Output: 1 mV × 10 = 10 mV RMS
- Peak voltage: 14 mV

**After Stage 2 (Gain = 20 dB):**
- Output: 10 mV × 10 = 100 mV RMS
- Peak voltage: 141 mV

**Total Gain: 40 dB**
- **Quiet tone (1 mV input):** 100 mV RMS output ✓
- **Loud tone (10 mV input):** 1000 mV (1 V) RMS output ✓
- **Output Range:** 100 mV - 1 V (perfect for ESP32 ADC)

**Supply Headroom:**
- Supply voltage: ±15V (or 0-3.3V single supply)
- Peak output: <2V
- Headroom: >50% ✓
- No clipping risk ✓

**Key Results:**
- ✅ Output levels optimized for ESP32 ADC (0-3.3V range)
- ✅ No clipping for expected signal levels
- ✅ Adequate dynamic range
- ✅ Compatible with Goertzel algorithm input requirements

**Conclusion:** Output signal levels are ideal for digital processing

---

#### 5. Required Post-Amp Gain

**File:** `Required_PostAmp_Gain.pdf`

**Analysis Content:**
- Detailed gain stage breakdown
- Component value calculations
- Resistor selection for each stage
- Overall transfer function

**Gain Stage Design:**

**Stage 1 - Non-Inverting Amplifier:**
- Gain formula: A₁ = 1 + (R₂/R₁)
- Target gain: 10× (20 dB)
- R₁ = 1 kΩ
- R₂ = 9 kΩ
- Actual gain: 1 + (9k/1k) = 10× ✓

**Stage 2 - MFB Bandpass Filter (with gain):**
- Gain formula: A₂ = -R₄/R₂ (at center frequency)
- Target gain: 10× (20 dB)
- Component values optimized for:
  - Center frequency: 3400 Hz
  - Gain: 10×
  - Q-factor: 2.0

**Total Gain:**
- A_total = A₁ × A₂ = 10 × 10 = 100× (40 dB) ✓

**Component Selection:**
- Standard E12 resistor values used
- 1% tolerance for accuracy
- Film capacitors for stability

**Key Results:**
- ✅ 40 dB total gain achieved
- ✅ Standard component values
- ✅ Low cost implementation
- ✅ Practical resistor/capacitor combinations

**Conclusion:** Post-amplifier gain requirements fully defined and achievable

---

#### 6. Stage Gains Feasibility Combined

**File:** `StageGains_Feasibility_Combined.pdf`

**Analysis Content:**
- Comprehensive multi-stage gain analysis
- Cascaded frequency response
- Overall system transfer function
- Combined feasibility assessment

**Multi-Stage Analysis:**

**System Overview:**
```
Microphone → Stage 1 (20 dB) → Stage 2 (20 dB) → ADC
  1-10 mV       10-100 mV        100 mV - 1 V
```

**Frequency Response (Combined):**
- Passband: 2800-4000 Hz
- Passband gain: 40 dB
- Stopband rejection: >20 dB below 2 kHz
- Stopband rejection: >20 dB above 5 kHz

**Phase Response:**
- Phase shift at 2800 Hz: -45° (acceptable)
- Phase shift at 4000 Hz: -60° (acceptable)
- Goertzel algorithm phase-insensitive ✓

**Noise Analysis:**
- Input-referred noise: <50 μV RMS
- SNR: >40 dB for 1 mV input signal
- Adequate for reliable detection ✓

**Key Results:**
- ✅ Combined stages meet all requirements
- ✅ Frequency response optimized
- ✅ Noise performance acceptable
- ✅ Overall system feasible and practical

**Conclusion:** Multi-stage design validated - ready for implementation

---

### MATLAB Scripts

**Location:** Complete MATLAB code available in:
- `../../Simulation/MATLAB_Simulations/Pure_Tone_Gain_Analysis/`
- Symbolic link: `MATLAB_Scripts_Reference/Pure_Tone_Gain_Analysis/`

**Key Scripts:**
- `Gain_Pure_Tone.m` - Initial gain analysis
- `Gain_Pure_Tone_2.m` - Refined analysis with optimizations

**Usage:**
```matlab
cd('Simulation/MATLAB_Simulations/Pure_Tone_Gain_Analysis')
Gain_Pure_Tone       % Run initial analysis
Gain_Pure_Tone_2     % Run refined analysis
```

**Outputs:**
- PDF plots of frequency response
- Gain calculations and feasibility charts
- Design decision reports

---

### Design Validation Summary

**Pure Tone Amplifier Design:**

| Parameter | Calculated | Measured (Lab) | Match |
|-----------|------------|----------------|-------|
| Total Gain | 40 dB | 38-42 dB | ✅ |
| Center Frequency | 3400 Hz | 3350-3450 Hz | ✅ |
| Bandwidth | ~2000 Hz | ~1900 Hz | ✅ |
| Output Level (1 mV in) | 100 mV | 95-110 mV | ✅ |
| Output Level (10 mV in) | 1000 mV | 950-1050 mV | ✅ |

**Conclusion:** MATLAB predictions closely match laboratory measurements ✅

---

## NAVCON Geometric Analysis

**Directory:** `NAVCON_Geometric_Analysis/`

### Purpose

Develop and validate the NAVCON (Navigation Control) algorithm through geometric modeling:
- Calculate sensor incidence angles for line detection
- Model sensor array geometry
- Determine decision thresholds
- Validate navigation rules

### Analysis Overview

**NAVCON Algorithm Requirements:**
- Detect line incidence angle (0-90°)
- Determine appropriate navigation action
- Use angle and color information for decisions
- Navigate autonomously through maze

### Geometric Model

**Sensor Array Configuration:**
- **Three Line Sensors:** Left, Center, Right
- **Sensor Spacing:** Determined by robot geometry
- **Sensor Position:** Bottom of robot, facing downward
- **Detection Range:** Line width and position

**Incidence Angle Calculation:**
- Angle θ: Angle between robot heading and line direction
- Calculated from sensor detection pattern
- Range: 0° (perpendicular) to 90° (parallel)

### MATLAB Scripts

**Location:**
- `../../Simulation/MATLAB_Simulations/NAVCON_Analysis/`
- Symbolic link: `MATLAB_Scripts_Reference/NAVCON_Analysis/`

**Key Scripts:**
- `navcon_incidence_demo.m` - Incidence angle calculations and visualization

**Usage:**
```matlab
cd('Simulation/MATLAB_Simulations/NAVCON_Analysis')
navcon_incidence_demo  % Run incidence angle analysis
```

### Analysis Results

**Angle Categories (from geometric analysis):**

1. **Small Angles (θ ≤ 5°):**
   - **Geometry:** All three sensors detect line
   - **Interpretation:** Robot nearly perpendicular to line
   - **Action:** FORWARD (line-following mode)

2. **Medium Angles (5° < θ ≤ 45°):**
   - **Geometry:** Two sensors detect line typically
   - **Interpretation:** Moderate angle to line
   - **Action:** FORWARD if center sensor WHITE, else ROTATE

3. **Large Angles (θ > 45°):**
   - **Geometry:** One or two sensors detect line
   - **Interpretation:** Robot approaching parallel to line
   - **Action:** ROTATE toward WHITE sensor

**Threshold Justification:**
- **5° threshold:** Geometric limit where sensor spacing causes different detection patterns
- **45° threshold:** Practical limit where rotation is more efficient than forward motion

### Validation

**Python Test Suite Validation:**
- All 125 angle/color scenarios tested
- Geometric predictions match actual behavior ✓
- Decision thresholds produce optimal navigation ✓

**Cross-Reference:**
- See `../Python_Test_Suite/NAVCON_Test_Results/`
- 100% accuracy on navigation decisions

---

## Integration with Other Evidence

### Cross-References

#### Pure Tone Analysis Validation

**MATLAB Predictions →  Laboratory Measurements:**
- MATLAB gain calculations: 40 dB
- Lab measurements: 38-42 dB
- **Match:** ✅ Excellent correlation

**MATLAB Predictions → Hardware Implementation:**
- Component values calculated in MATLAB
- Implemented on veroboard with same values
- **Result:** Pure tone detection functional

**MATLAB Predictions → QTP Results:**
- Predicted tone detection: Functional
- QTP-SNC-06 Result: ✅ PASS
- QTP-SNC-07 Result: ✅ PASS

#### NAVCON Analysis Validation

**MATLAB Geometry → Python Tests:**
- Angle thresholds defined mathematically
- Python tests validate all scenarios
- **Result:** 100% decision accuracy

**MATLAB Geometry → Real-World Navigation:**
- Geometric model predicts behavior
- Maze navigation successful
- **Result:** Algorithm performs as designed

### Supporting Evidence

- **Phase 2 Lab Results:** `../Phase2_Lab_Results/`
  - Oscilloscope measurements validate MATLAB predictions

- **Python Test Suite:** `../Python_Test_Suite/`
  - NAVCON tests validate geometric analysis

- **QTP Test Results:** `../QTP_Test_Results/`
  - QTP-SNC-03 and QTP-SNC-04 validate NAVCON algorithm
  - QTP-SNC-06 validates pure tone gain design

---

## Key Findings and Conclusions

### Pure Tone Gain Analysis

✅ **MCP6024 Op-Amp Selection Validated**
- Specifications adequate for application
- Frequency range appropriate
- Cost-effective solution

✅ **Gain Requirements Defined**
- 40 dB total gain calculated
- Component values determined
- Practical implementation confirmed

✅ **Performance Predictions Accurate**
- Calculated gains match measured values
- Output levels optimal for ADC
- Design meets all requirements

✅ **Design Decision Support**
- Quantitative justification for all choices
- Multiple configurations analyzed
- Optimal design selected

### NAVCON Geometric Analysis

✅ **Algorithm Thresholds Mathematically Justified**
- 5° and 45° thresholds based on geometry
- Optimal navigation decisions predicted
- Validation through testing confirms accuracy

✅ **Sensor Array Geometry Modeled**
- Incidence angle calculations correct
- Sensor spacing considerations addressed
- Detection patterns predicted accurately

✅ **Navigation Rules Validated**
- Geometric analysis supports decision logic
- All scenarios covered
- Real-world performance matches predictions

### Overall Validation

The MATLAB simulations provide crucial design validation:

1. **Predictive Accuracy:** Calculations closely match real measurements
2. **Design Confidence:** Quantitative support for all design decisions
3. **Early Validation:** Issues identified before hardware implementation
4. **Cost Savings:** Optimal design selected through simulation
5. **Documentation:** Clear evidence trail for design choices

---

## Verification Summary

### Analysis Coverage

| Analysis Area | Coverage | Documentation |
|---------------|----------|---------------|
| Pure Tone Gain | 100% | 6 PDF reports |
| Op-Amp Selection | Complete | Design decision report |
| Frequency Response | Complete | Limitations analysis |
| Output Levels | Complete | Signal level analysis |
| Multi-Stage Design | Complete | Combined feasibility |
| NAVCON Geometry | Complete | MATLAB scripts |
| Angle Thresholds | Complete | Incidence demo |

### Validation Status

| Prediction | Measurement | Match | Status |
|------------|-------------|-------|--------|
| 40 dB gain | 38-42 dB | ±5% | ✅ |
| 3400 Hz center | 3350-3450 Hz | ±3% | ✅ |
| 100 mV output | 95-110 mV | ±10% | ✅ |
| NAVCON angles | 100% accuracy | Perfect | ✅ |

---

## Files Summary

### Pure Tone Gain Analysis

- `Feasibility_Map.pdf` - Overall system feasibility (54.8 KB)
- `MCP6024_Design_Decision.pdf` - Op-amp selection (7.6 KB)
- `MCP6024_Frequency_Limitations.pdf` - Bandwidth analysis (22.0 KB)
- `MCP6024_Output_Signal_Level.pdf` - Output level predictions (13.9 KB)
- `Required_PostAmp_Gain.pdf` - Gain stage design (152.7 KB)
- `StageGains_Feasibility_Combined.pdf` - Combined analysis (185.9 KB)

**Total:** 6 PDF reports, 437 KB

### MATLAB Scripts Reference

Symbolic link to complete MATLAB codebase:
- `MATLAB_Scripts_Reference/` → `../../Simulation/MATLAB_Simulations/`

---

## Usage Instructions

### Viewing PDF Reports

All PDF reports can be viewed with any PDF reader:
```bash
cd Verification/MATLAB_Simulations/Pure_Tone_Gain_Analysis
# Open PDFs with preferred viewer
```

### Running MATLAB Scripts

To reproduce analyses:
```matlab
% Navigate to script directory
cd('Simulation/MATLAB_Simulations/Pure_Tone_Gain_Analysis')

% Run gain analyses
Gain_Pure_Tone       % Initial analysis
Gain_Pure_Tone_2     % Refined analysis

% Navigate to NAVCON analysis
cd('../NAVCON_Analysis')
navcon_incidence_demo  % Run geometric analysis
```

**Requirements:**
- MATLAB R2020a or later (earlier versions may work)
- Signal Processing Toolbox (optional, for advanced analysis)

---

## Verification Checklist

- ✅ All gain calculations documented
- ✅ Op-amp selection justified quantitatively
- ✅ Frequency limitations analyzed
- ✅ Output signal levels predicted
- ✅ Multi-stage design validated
- ✅ NAVCON geometry modeled
- ✅ Angle thresholds justified mathematically
- ✅ MATLAB predictions match laboratory measurements
- ✅ All PDF reports archived
- ✅ MATLAB scripts preserved and documented

**Overall Status: ALL MATLAB ANALYSES COMPLETE ✅**

---

## Contact

For questions regarding MATLAB simulation results:
- Review PDF reports in respective directories
- Consult MATLAB scripts for implementation details
- Cross-reference with laboratory measurements

---

**Last Updated:** 2025-01-18
**Status:** Complete and Verified
**Analysis Files:** 6 PDFs + MATLAB scripts
