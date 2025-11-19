# SNC Simulation Test Suite

Comprehensive Python-based simulation and testing suite for the SNC (Sensor Navigation Control) subsystem of the AMazeEng MARV robot.

## Overview

This test suite provides extensive simulation tools to validate SNC functionality against QTP (Quality Test Procedures) requirements. All tests follow proper testing procedures with consistent GUIs and detailed logging.

## Architecture

### Core Modules (Core/)

1. **scs_protocol.py** - SCS Protocol implementation
   - Packet structures and encoding/decoding
   - State machine definitions
   - Color encoding helpers
   - Protocol validation functions

2. **gui_framework.py** - Base GUI framework
   - Consistent styling across all tests
   - Serial port management
   - Packet logging and statistics
   - Reusable UI components

### Python Test Applications (Python_Tests/)

Organized into three categories:

1. **Command_Tests/** - Current production test suite
   - Individual command testers for each SCS command category
   - Comprehensive QTP validation suite
   - Dual-port maze integration tester
   - See Command_Tests/README.md for detailed documentation

2. **NAVCON_Suite/** - NAVCON algorithm validation
   - Comprehensive NAVCON decision testing
   - Complete angle and color matrix validation
   - See NAVCON_Suite/README.md for detailed documentation

3. **Legacy_Tests/** - Historical test applications
   - Phase 1 development tests
   - Early communication testers
   - Preserved for reference and debugging
   - See Legacy_Tests/README.md for compatibility notes

### Circuit Simulations (LTspice_Simulations/)

Analog circuit design and validation for pure tone detection hardware:

- **Pure_Tone_Detection/** - Complete analog signal chain
  - Full circuit simulations (Phase 1 and Phase 3 designs)
  - Bandpass filter detailed analysis
  - Veroboard physical implementation reference
  - See Pure_Tone_Detection/README.md for circuit documentation

### MATLAB Analysis (MATLAB_Simulations/)

Mathematical analysis and algorithm development:

1. **Pure_Tone_Gain_Analysis/** - Amplifier gain calculations
   - MCP6024 op-amp feasibility analysis
   - Gain stage optimization
   - Output signal level predictions
   - Design decision validation
   - See Pure_Tone_Gain_Analysis/README.md for analysis documentation

2. **NAVCON_Analysis/** - Navigation geometry analysis
   - Incidence angle calculations
   - Sensor array geometric modeling
   - NAVCON decision threshold validation
   - See NAVCON_Analysis/README.md for algorithm documentation

### Test Scripts

#### Individual Command Tests

Each test script focuses on specific SCS commands with dedicated GUI:

1. **test_idle_commands.py** - IDLE state testing
   - IDLE:HUB:0 - Initial contact
   - IDLE:SNC:0 - System ready response
   - Touch sensor activation
   - IDLE → CAL transition validation

2. **test_cal_commands.py** - Calibration state testing
   - CAL:SS:0/1 - Sensor calibration sequence
   - CAL:MDPS:0/1 - Motor calibration sequence
   - CAL:SNC:0 - Calibration acknowledgment
   - CAL → MAZE transition

3. **test_maze_mdps_commands.py** - MAZE MDPS command testing
   - MAZE:MDPS:1 - Stop/Rotate commands
   - MAZE:MDPS:2 - Confirmation packets
   - MAZE:MDPS:3 - Forward motion
   - MAZE:MDPS:4 - Distance updates

4. **test_maze_ss_commands.py** - MAZE SS command testing
   - MAZE:SS:1 - Color data packets (all color combinations)
   - MAZE:SS:2 - Angle data packets (0-90°)
   - MAZE:SS:3 - End-of-maze signal

5. **test_navcon_decisions.py** - NAVCON decision logic testing
   - All angle categories (≤5°, 5-45°, >45°)
   - All color combinations (WHITE, RED, GREEN, BLUE, BLACK)
   - Navigation rule validation
   - Motion primitive verification

6. **test_pure_tone.py** - Pure tone detection testing
   - Dual-tone sequence validation
   - MAZE ↔ SOS state toggle
   - Timing requirement verification (500-1000ms, 2s window)
   - False alarm rejection

#### Integrated Test Suites

7. **hub_testing_suite.py** - Comprehensive HUB testing
   - All QTP test scenarios (QTP-SNC-01 through QTP-SNC-10)
   - Automated test execution
   - Pass/fail criteria validation
   - Detailed test reports
   - Single serial port operation

8. **dual_port_maze_tester.py** - Advanced maze simulation
   - **DUAL SERIAL PORT OPERATION:**
     - Port 1: SS (Sensor Subsystem) emulation
     - Port 2: MDPS (Motor Drive & Power Supply) emulation
   - Independent subsystem control
   - Full maze navigation scenarios
   - Real-time maze state visualization
   - Comprehensive logging

## Test Coverage

### QTP Requirements Mapping

| QTP | Requirement | Test Script | Status |
|-----|-------------|-------------|--------|
| QTP-SNC-01 | IDLE → CAL transition | test_idle_commands.py | ✓ |
| QTP-SNC-02 | CAL → MAZE transition | test_cal_commands.py | ✓ |
| QTP-SNC-03 | NAVCON forward navigation | test_navcon_decisions.py | ✓ |
| QTP-SNC-04 | NAVCON rotation logic | test_navcon_decisions.py | ✓ |
| QTP-SNC-05 | SCS protocol compliance | All tests | ✓ |
| QTP-SNC-06 | Pure tone detection | test_pure_tone.py | ✓ |
| QTP-SNC-07 | MAZE ↔ SOS toggle | test_pure_tone.py | ✓ |
| QTP-SNC-08 | WiFi telemetry | hub_testing_suite.py | ✓ |
| QTP-SNC-09 | Main loop timing | hub_testing_suite.py | ✓ |
| QTP-SNC-10 | EOM detection | test_maze_ss_commands.py | ✓ |

### Protocol Coverage

- **All 4 system states:** IDLE, CAL, MAZE, SOS
- **All 4 subsystems:** HUB, SNC, MDPS, SS
- **47 distinct commands:** Complete IST code coverage
- **All color combinations:** 5 colors × 3 sensors = comprehensive coverage
- **Angle ranges:** 0-90° in 1° increments

## Usage

### Python Test Applications

#### Individual Command Tests

Navigate to the Command_Tests directory:

```bash
cd Python_Tests/Command_Tests

# IDLE state testing
python3 test_idle_commands.py

# Calibration testing
python3 test_cal_commands.py

# MAZE MDPS commands
python3 test_maze_mdps_commands.py

# MAZE SS commands
python3 test_maze_ss_commands.py

# NAVCON decision validation
python3 test_navcon_decisions.py

# Pure tone detection
python3 test_pure_tone.py
```

#### HUB Testing Suite

Comprehensive QTP validation:

```bash
cd Python_Tests/Command_Tests
python3 hub_testing_suite.py
```

Features:
- Select specific QTP or run all
- Automated test execution
- Real-time pass/fail indication
- Detailed test reports with timestamps
- Export results to CSV/JSON

#### Dual-Port Maze Tester

Advanced two-port simulation:

```bash
cd Python_Tests/Command_Tests
python3 dual_port_maze_tester.py
```

**Hardware Setup:**
1. Connect SNC UART1 to SS serial port (Port 1)
2. Connect SNC UART2 to MDPS serial port (Port 2)
3. Configure both ports at 115200 baud

Features:
- Independent SS and MDPS emulation
- Synchronized packet transmission
- Virtual maze environment with all line types
- Real-time maze visualization
- Distance and rotation tracking
- Complete navigation scenario testing

#### NAVCON Comprehensive Tester

```bash
cd Python_Tests/NAVCON_Suite
python3 navcon_tester.py
```

### LTspice Circuit Simulations

1. Open LTspice XVII or compatible version
2. Navigate to `LTspice_Simulations/Pure_Tone_Detection/`
3. Open desired .asc file (FullSim.asc or FullSim_connected.asc)
4. Run AC analysis for frequency response or transient analysis for time-domain behavior
5. Probe key nodes to view waveforms

See LTspice_Simulations/Pure_Tone_Detection/README.md for detailed simulation instructions.

### MATLAB Analysis Scripts

#### Pure Tone Gain Analysis

```matlab
cd('MATLAB_Simulations/Pure_Tone_Gain_Analysis')
Gain_Pure_Tone       % Run initial analysis
Gain_Pure_Tone_2     % Run refined analysis
```

#### NAVCON Geometric Analysis

```matlab
cd('MATLAB_Simulations/NAVCON_Analysis')
navcon_incidence_demo  % Run incidence angle analysis
```

See respective README files in each MATLAB directory for detailed usage and parameter modification instructions.

## GUI Features

All test GUIs include:

### Serial Connection Panel
- Port selection and refresh
- Configurable baud rate (default: 19200)
- Connection status indicator
- Auto-reconnect on disconnect

### Test Control Panel
- Test scenario selection
- Start/Stop/Pause controls
- Progress indication
- Current test status

### Packet Monitor
- Real-time packet logging
- Color-coded sent/received packets
- Timestamp for each packet
- Packet details and descriptions
- Clear and save log functions

### Statistics Panel
- Test duration
- Packets sent/received
- Success rate
- Test-specific metrics

### Test-Specific Panels
- Command parameter controls
- Expected response display
- Validation criteria
- Pass/fail indicators

## Color Scheme

Consistent across all test GUIs:
- **Background:** Dark blue-grey (#2c3e50)
- **Panels:** Light blue-grey (#34495e)
- **Success:** Green (#27ae60)
- **Error:** Red (#e74c3c)
- **Warning:** Orange (#e67e22)
- **Info:** Blue (#3498db)
- **Sent packets:** Blue
- **Received packets:** Green

## File Structure

```
Simulation/
├── README.md                           # This file - simulation suite overview
├── Core/                               # Core framework modules
│   ├── scs_protocol.py                 # SCS protocol implementation
│   └── gui_framework.py                # Base GUI framework
├── Python_Tests/                       # Python-based test applications
│   ├── Command_Tests/                  # Individual command test suite
│   │   ├── README.md                   # Command tests documentation
│   │   ├── test_idle_commands.py       # IDLE state tests
│   │   ├── test_cal_commands.py        # Calibration tests
│   │   ├── test_maze_mdps_commands.py  # MAZE MDPS tests
│   │   ├── test_maze_ss_commands.py    # MAZE SS tests
│   │   ├── test_navcon_decisions.py    # NAVCON decision matrix
│   │   ├── test_pure_tone.py           # Pure tone detection tests
│   │   ├── hub_testing_suite.py        # Comprehensive QTP suite
│   │   └── dual_port_maze_tester.py    # Dual-port maze simulator
│   ├── NAVCON_Suite/                   # NAVCON testing applications
│   │   ├── README.md                   # NAVCON suite documentation
│   │   └── navcon_tester.py            # Comprehensive NAVCON tester
│   └── Legacy_Tests/                   # Historical test applications
│       ├── README.md                   # Legacy tests documentation
│       └── *.py                        # Phase 1 and early development tests
├── LTspice_Simulations/                # Circuit simulations
│   └── Pure_Tone_Detection/            # Pure tone analog circuit
│       ├── README.md                   # LTspice simulation documentation
│       ├── FullSim.asc                 # Complete circuit (Phase 1)
│       ├── FullSim_connected.asc       # Veroboard implementation (Phase 3)
│       ├── Draft1.asc                  # Early prototype design
│       ├── Veroboard_Stripboard-Layout-1.pdf  # Physical layout
│       └── Filter/                     # Filter stage analysis
│           ├── MFB.asc                 # MFB bandpass filter simulation
│           └── *.emf                   # Filter response plots
└── MATLAB_Simulations/                 # MATLAB analysis scripts
    ├── Pure_Tone_Gain_Analysis/        # Gain calculations
    │   ├── README.md                   # MATLAB gain analysis documentation
    │   ├── Gain_Pure_Tone.m            # Initial gain analysis
    │   ├── Gain_Pure_Tone_2.m          # Refined gain analysis
    │   ├── *.csv                       # Analysis results
    │   └── *.pdf                       # Design decision reports
    └── NAVCON_Analysis/                # Navigation algorithm analysis
        ├── README.md                   # NAVCON MATLAB documentation
        └── navcon_incidence_demo.m     # Incidence angle calculations
```

## Dependencies

```bash
pip install pyserial
```

Built-in Python modules used:
- tkinter (GUI)
- threading (concurrent operations)
- queue (thread-safe messaging)
- dataclasses (packet structures)
- enum (protocol definitions)

## Testing Methodology

All tests follow proper testing procedures:

1. **Test Setup**
   - Initialize serial connection
   - Configure test parameters
   - Clear previous state

2. **Test Execution**
   - Send commanded packets
   - Monitor SNC responses
   - Validate against expected behavior
   - Log all transactions

3. **Test Validation**
   - Check packet format compliance
   - Verify timing requirements
   - Validate state transitions
   - Confirm data accuracy

4. **Test Reporting**
   - Generate detailed logs
   - Export test results
   - Document pass/fail status
   - Capture evidence artifacts

## Integration with HUB Logs

All tests generate logs compatible with HUB log format:

```
DD/MM/YYYY HH:MM:SS || SEQ || DIRECTION || (SYS-SUB-IST) || STATE | SUB | IST || DAT1 | DAT0 | DEC || CTRL ||
```

Example:
```
18/01/2025 14:32:15 ||  1 || SENT     || (0-0-0) || IDLE | HUB  | 0 ||    0 |    0 |   0 ||   0 ||
18/01/2025 14:32:16 ||  2 || RECEIVED || (0-1-0) || IDLE | SNC  | 0 ||    1 |   50 |   0 ||  16 ||
```

## Development

### Creating New Tests

1. Import core modules:
```python
import sys
sys.path.append('../../Core')
from gui_framework import BaseTestWindow, ColorScheme
from scs_protocol import *
```

2. Extend BaseTestWindow:
```python
class MyTestWindow(BaseTestWindow):
    def __init__(self):
        super().__init__("My Test Name")
        self.create_custom_panels()
```

3. Override test methods:
```python
def start_test(self):
    super().start_test()
    # Custom test logic
```

### Adding New Packet Types

Edit `Core/scs_protocol.py`:
```python
def make_my_new_packet(param1, param2):
    ctrl = create_control_byte(SystemState.MAZE, SubsystemID.SS, 4)
    return SCSPacket(ctrl, param1, param2, 0)
```

## Troubleshooting

### Serial Port Issues

**Problem:** "Permission denied" on Linux
**Solution:**
```bash
sudo usermod -a -G dialout $USER
# Logout and login again
```

**Problem:** Port already in use
**Solution:** Close other serial terminal programs (Arduino IDE, PuTTY, etc.)

### GUI Issues

**Problem:** Window doesn't display correctly
**Solution:** Update tkinter:
```bash
sudo apt-get install python3-tk  # Linux
brew install python-tk@3.X       # macOS
```

### Test Failures

**Problem:** No response from SNC
**Solution:**
1. Verify SNC is powered and in correct state
2. Check baud rate matches (19200)
3. Verify correct COM port selected
4. Check UART connections

**Problem:** Packet format errors
**Solution:**
1. Verify SCS protocol version match
2. Check byte order (MSB/LSB)
3. Validate control byte calculation

## Documentation Structure

Each major directory contains its own README with detailed documentation:

- **Python_Tests/Command_Tests/README.md** - Individual command test documentation
- **Python_Tests/NAVCON_Suite/README.md** - NAVCON algorithm testing guide
- **Python_Tests/Legacy_Tests/README.md** - Historical test reference
- **LTspice_Simulations/Pure_Tone_Detection/README.md** - Circuit simulation guide
- **MATLAB_Simulations/Pure_Tone_Gain_Analysis/README.md** - Gain analysis documentation
- **MATLAB_Simulations/NAVCON_Analysis/README.md** - NAVCON algorithm analysis

## References

- **SCS Protocol Specification:** See `SNC_REFERENCE_GUIDE.md`
- **QTP Requirements:** `AMazeEng MARV QTPs 2025.pdf`
- **Project Guide:** `Project Guide to an AMazeENG MARV 2025.pdf`
- **Core Modules:** `Core/scs_protocol.py` and `Core/gui_framework.py`

## License

ERD320 SNC Subsystem - University of Pretoria
© 2025 ERD320 Team

## Contact

For issues or questions:
- Review HUB logs in `Phase3/Phase3/HUB/`
- Check `SNC_REFERENCE_GUIDE.md`
- Consult existing test suite documentation

---

**Last Updated:** 2025-01-18
**Version:** 1.0
**Status:** Production Ready
