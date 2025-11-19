# Legacy Test Applications

## Overview

This directory contains legacy test applications developed during earlier phases of the AMazeEng MARV project. These applications represent historical testing approaches and may contain older protocol implementations or test methodologies that have since been superseded by the Command Test Suite.

## Purpose

Legacy test files are preserved for:
- Historical reference and development traceability
- Alternative testing approaches that may be useful for debugging
- Baseline comparison with current test implementations
- Recovery of specific test scenarios not covered in current suite

## Contents

This directory contains test files copied from:
- `Coms tester/` directory: Communication protocol test utilities
- `Phase 1/NAVCON/` directory: Early NAVCON development test files
- Other legacy test directories from previous development phases

## Important Notes

### Compatibility Warnings

1. **Protocol Version**: Legacy tests may implement earlier versions of the SCS protocol. Packet structures and command definitions may differ from current specification.

2. **Dependencies**: Legacy tests may have different dependency requirements or use outdated library versions.

3. **Serial Configuration**: Baud rates, packet formats, and timing assumptions may differ from current implementation.

4. **State Machine**: Earlier tests may not fully implement the complete state machine (IDLE, CAL, MAZE, SOS).

### Current Alternatives

For current development and validation, use the following instead:
- **Command Testing**: Use `../Command_Tests/` suite
- **NAVCON Testing**: Use `../NAVCON_Suite/navcon_tester.py`
- **QTP Validation**: Use `../Command_Tests/hub_testing_suite.py`

## Usage Guidance

### When to Use Legacy Tests

Consider using legacy tests when:
- Investigating historical behavior or debugging regression issues
- Comparing current implementation against baseline behavior
- Recovering specific test scenarios not documented in current suite
- Analyzing evolution of protocol or algorithm implementations

### How to Use Legacy Tests

1. Review the specific test file to understand its purpose and requirements
2. Check for any hardcoded serial port configurations or baud rates
3. Verify compatibility with current hardware and firmware
4. Run tests in isolation to avoid conflicts with current protocol implementation
5. Document any differences observed between legacy and current behavior

## File Descriptions

Due to the varied nature of legacy test files, each file should be examined individually to determine:
- Original purpose and test scope
- Protocol version implemented
- Hardware requirements
- Expected inputs and outputs

Common legacy test file patterns include:
- **Tester.py / ComsTester.py**: Early communication protocol validators
- **navcon_*.py**: NAVCON algorithm development tests
- Other test utilities from Phase 1 development

## Migration Notes

Key differences between legacy and current implementations:

### Protocol Evolution
- Early tests may use 3-byte packets instead of current 4-byte SCS protocol
- Command structure and encoding may differ
- State machine may be simplified or incomplete

### Architecture Changes
- Modern tests use centralized `scs_protocol.py` and `gui_framework.py`
- Legacy tests may implement protocol inline or use different helper modules
- GUI frameworks may differ (older tkinter implementations)

### Testing Methodology
- Current tests emphasize automation and QTP alignment
- Legacy tests may focus on manual validation or specific debug scenarios
- Logging and reporting formats may differ

## Maintenance Status

**Status**: Archived / Read-Only

Legacy tests are maintained for historical reference only. Active development and maintenance occur in:
- `../Command_Tests/`
- `../NAVCON_Suite/`

## Related Documentation

- Current Command Test Suite: `../Command_Tests/README.md`
- NAVCON Test Suite: `../NAVCON_Suite/README.md`
- Main Simulation README: `../../README.md`
- Project Development History: Project documentation folder

## Support

For questions about legacy test files:
1. Review commit history to understand original context
2. Check Phase 1 documentation for protocol specifications
3. Compare with current implementation in Command Test Suite
4. Consult project documentation for development timeline

Legacy tests should not be used for production validation or QTP compliance testing.
