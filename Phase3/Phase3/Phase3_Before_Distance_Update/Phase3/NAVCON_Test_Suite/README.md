# MARV NAVCON Testing Suite

## Overview

This comprehensive Python testing suite emulates the SS (Sensor Subsystem) and MDPS (Motor Drive & Power Supply) subsystems to thoroughly test your SNC (Sensor Navigation Control) subsystem through complete maze navigation scenarios.

The test suite is based on the AMazeEng MARV QTPs 2025 documentation and follows the exact communication protocol and flow patterns observed in the client log analysis.

## Features

### 🎯 **Complete SNC Testing**
- **QTP2 Compliance**: Tests basic NAVCON functionality
- **QTP3 Compliance**: Advanced navigation scenarios
- **Full Maze Navigation**: Complete end-to-end testing
- **Protocol Validation**: Ensures SCS protocol adherence

### 📡 **Professional Interface**
- **Serial Port Selection**: Easy COM port configuration
- **Real-time Monitoring**: Live packet analysis and display
- **Professional GUI**: Modern, intuitive interface
- **Comprehensive Logging**: Detailed test logs with timestamps

### 🔍 **Advanced Diagnostics**
- **Packet Analysis**: Real-time SCS packet interpretation
- **Performance Metrics**: Success rates, timing analysis
- **Error Detection**: Protocol violations and timeout handling
- **Visual Feedback**: Color-coded packet types and status

### 🧪 **Test Scenarios**
- **SS Emulation**: Sensor color detection, calibration, line detection
- **MDPS Emulation**: Motor commands, rotation requests, speed control
- **NAVCON Testing**: Green line detection, rotation sequences, maze navigation
- **State Management**: Proper IDLE → CAL → MAZE → SOS transitions

## Installation

### Prerequisites
- Python 3.7 or higher
- ESP32 with your SNC code uploaded
- USB cable for serial connection

### Setup
1. **Install Python dependencies:**
   ```bash
   pip install -r requirements.txt
   ```

2. **Connect your SNC ESP32:**
   - Connect ESP32 to computer via USB
   - Note the COM port (e.g., COM3, COM5)
   - Ensure baud rate is set to 19200 (default)

## Usage

### Starting the Test Suite
```bash
python navcon_tester.py
```

### Basic Operation
1. **Select Serial Port**: Choose your ESP32's COM port from dropdown
2. **Set Baud Rate**: Default 19200 (matches ESP32 configuration)
3. **Connect**: Click "🔌 Connect" to establish communication
4. **Choose Test Scenario**: Select from predefined test cases
5. **Start Test**: Click "▶️ Start Test" to begin

### Test Scenarios

#### **QTP2: Basic NAVCON**
Tests fundamental NAVCON functionality:
- SS/MDPS calibration sequence
- Transition to MAZE state
- Green line detection simulation
- Basic rotation commands
- Speed control verification

#### **QTP3: Advanced NAVCON**
Advanced navigation testing:
- Multiple line color scenarios
- Complex routing decisions
- Angle correction algorithms
- Error recovery testing

#### **Full Maze Navigation**
Complete end-to-end testing:
- Simulates entire maze environment
- Multiple green line encounters
- Complex navigation patterns
- Performance under stress

## Understanding the Display

### 📊 **Packet Monitor Tab**
Real-time packet display showing:
```
HH:MM:SS.mmm || SEQ || DIRECTION || (SYS-SUB-IST) || STATE | SUBSYSTEM | IST || DAT1 | DAT0 | DEC || CTRL
```

**Example:**
```
14:23:15.423 ||   5 || SENT     || (2-2-1) || MAZE | MDPS | 1 ||  90 |   0 |   0 || 161
14:23:15.445 ||   6 || RECEIVED || (2-1-3) || MAZE | SNC  | 3 ||  50 |  50 |   0 || 147
```

### 📈 **Test Statistics Tab**
Comprehensive metrics:
- **Test Duration**: Real-time test timing
- **Packets Sent/Received**: Communication statistics
- **Success Rate**: Protocol compliance percentage
- **Current State**: Live system state monitoring
- **Event Counters**: Touch events, rotations, detections

### 🔍 **Packet Analysis**
Real-time interpretation:
- Raw byte analysis
- Protocol field breakdown
- NAVCON command identification
- Error detection and reporting

## Protocol Implementation

### SCS Packet Structure
```
Byte 0: CONTROL = (SYS<1:0> | SUB<1:0> | IST<3:0>)
Byte 1: DAT1    = Upper data byte
Byte 2: DAT0    = Lower data byte
Byte 3: DEC     = Decimal/general purpose
```

### System States
- **IDLE (0)**: System startup/shutdown
- **CAL (1)**: Calibration phase
- **MAZE (2)**: Active navigation
- **SOS (3)**: Emergency state

### Subsystem IDs
- **HUB (0)**: Test suite controller
- **SNC (1)**: Your navigation system
- **MDPS (2)**: Motor/power emulation
- **SS (3)**: Sensor emulation

## Test Flow Explanation

### Phase 1: Initialization
1. **HUB → SNC**: Initial contact packet `(0-0-0)`
2. **SNC → HUB**: Ready acknowledgment `(0-1-0)`

### Phase 2: Calibration
1. **SS Emulation**: Calibration sequence `(1-3-0)` → `(1-3-1)`
2. **MDPS Emulation**: Motor calibration `(1-2-0)` → `(1-2-1)`
3. **SNC Response**: Calibration complete `(1-1-0)`

### Phase 3: MAZE Navigation
1. **State Transition**: SNC enters MAZE state `(2-1-*)`
2. **NAVCON Activation**: IST=3 indicates active navigation `(2-1-3)`
3. **SS Simulation**: Green line detection `(2-3-1)` with color data
4. **MDPS Simulation**: Rotation commands `(2-2-1)` with angle data
5. **Response Analysis**: Monitor SNC navigation decisions

### Phase 4: Validation
- **Protocol Compliance**: Verify correct packet format
- **Timing Analysis**: Ensure response within specifications
- **Logic Verification**: Validate navigation decisions
- **Error Handling**: Test edge cases and recovery

## Expected SNC Behavior

### 🎯 **Correct NAVCON Operation**
When functioning properly, your SNC should:

1. **Respond to calibration** within 60 seconds
2. **Transition to MAZE state** after calibration complete
3. **Generate IST=3 packets** during active navigation
4. **Request rotations** via IST=1 packets when encountering lines
5. **Provide speed commands** with appropriate DAT1/DAT0 values

### ⚠️ **Common Issues Detected**
The test suite will identify:
- **Missing responses**: Timeout errors
- **Protocol violations**: Incorrect packet format
- **State machine errors**: Invalid transitions
- **Timing issues**: Delayed responses
- **Logic errors**: Inappropriate navigation decisions

## Troubleshooting

### Connection Issues
- **"No COM ports found"**: Ensure ESP32 is connected and drivers installed
- **"Connection failed"**: Check ESP32 is not in use by Arduino IDE
- **"Permission denied"**: Close Arduino Serial Monitor

### Communication Issues
- **"No SNC response"**: Verify ESP32 code is running correctly
- **"Corrupted packets"**: Check baud rate (19200) and wiring
- **"Timeout errors"**: Ensure SNC is responding within time limits

### Test Failures
- **"Invalid state transition"**: Review SNC state machine logic
- **"Protocol violation"**: Check SCS packet format implementation
- **"NAVCON not active"**: Verify IST=3 generation in MAZE state

## Log Files

Test logs are automatically saved to:
```
NAVCON_Test_Suite/navcon_test_log_YYYYMMDD_HHMMSS.txt
```

Log format includes:
- Timestamp for each packet
- Packet direction (SENT/RECEIVED)
- Full packet analysis
- Test phase annotations
- Error conditions and warnings

## Advanced Features

### Custom Test Creation
Modify `create_test_scenarios()` to add custom tests:
```python
custom_test = NAVCONTestScenario("Custom Test", "Description")
custom_test.add_step(SCSPacket(control, dat1, dat0, dec), "Step description")
```

### Performance Monitoring
Real-time metrics tracking:
- Packet transmission rates
- Response time analysis
- Success/failure ratios
- Protocol compliance scoring

### Debugging Support
- Raw packet byte display
- Protocol field interpretation
- State machine visualization
- Error condition highlighting

## Support

For issues or questions:
1. Check the troubleshooting section above
2. Review the AMazeEng MARV QTPs 2025 documentation
3. Analyze packet logs for protocol violations
4. Verify ESP32 SNC implementation against SCS specifications

## Version History

- **v1.0**: Initial release with QTP2/QTP3 compliance
- Comprehensive SS/MDPS emulation
- Professional GUI with real-time monitoring
- Full packet analysis and logging