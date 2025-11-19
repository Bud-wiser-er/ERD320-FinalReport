#!/usr/bin/env python3
"""
SCS Protocol Utilities Module
Provides packet structures, encoding/decoding, and protocol definitions
for the Subsystem Communication Standard (SCS)

Author: ERD320 SNC Team
Date: 2025-01-18
"""

from enum import Enum
from dataclasses import dataclass
from typing import Tuple, Optional
import struct


# ==================== SCS PROTOCOL DEFINITIONS ====================

class SystemState(Enum):
    """System States - SYS[1:0] bits in control byte"""
    IDLE = 0    # 00 - System startup, awaiting touch
    CAL = 1     # 01 - Calibration phase
    MAZE = 2    # 10 - Active navigation
    SOS = 3     # 11 - Emergency state


class SubsystemID(Enum):
    """Subsystem IDs - SUB[1:0] bits in control byte"""
    HUB = 0     # 00 - Test controller/HUB
    SNC = 1     # 01 - Sensor Navigation Control (Main subsystem)
    MDPS = 2    # 10 - Motor Drive & Power Supply
    SS = 3      # 11 - Sensor Subsystem


# Color encoding for SS sensor data
class SensorColor(Enum):
    """Sensor color values for line detection"""
    WHITE = 0
    RED = 1
    GREEN = 2
    BLUE = 3
    BLACK = 4


# ==================== SCS PACKET STRUCTURE ====================

@dataclass
class SCSPacket:
    """
    SCS Packet Structure (4 bytes):

    Byte 0 - CONTROL: (SYS<1:0> | SUB<1:0> | IST<3:0>)
        - SYS[7:6]: System state (IDLE=0, CAL=1, MAZE=2, SOS=3)
        - SUB[5:4]: Subsystem ID (HUB=0, SNC=1, MDPS=2, SS=3)
        - IST[3:0]: Instruction/Status code (0-15)

    Byte 1 - DAT1: Upper data byte
    Byte 2 - DAT0: Lower data byte
    Byte 3 - DEC: Decimal/direction/special purpose byte
    """
    control: int    # CONTROL byte (SYS | SUB | IST)
    dat1: int       # DAT1 - Upper data byte
    dat0: int       # DAT0 - Lower data byte
    dec: int        # DEC - Decimal/direction byte

    def __post_init__(self):
        """Validate packet byte values"""
        for field, value in [('control', self.control), ('dat1', self.dat1),
                             ('dat0', self.dat0), ('dec', self.dec)]:
            if not 0 <= value <= 255:
                raise ValueError(f"{field} must be 0-255, got {value}")

    def to_bytes(self) -> bytes:
        """Convert packet to 4-byte sequence"""
        return bytes([self.control, self.dat1, self.dat0, self.dec])

    @classmethod
    def from_bytes(cls, data: bytes) -> 'SCSPacket':
        """Create packet from 4-byte sequence"""
        if len(data) != 4:
            raise ValueError(f"Packet must be 4 bytes, got {len(data)}")
        return cls(data[0], data[1], data[2], data[3])

    def get_sys_state(self) -> SystemState:
        """Extract system state from control byte"""
        return SystemState((self.control >> 6) & 0x03)

    def get_subsystem(self) -> SubsystemID:
        """Extract subsystem ID from control byte"""
        return SubsystemID((self.control >> 4) & 0x03)

    def get_ist(self) -> int:
        """Extract IST (instruction/status) from control byte"""
        return self.control & 0x0F

    def __str__(self) -> str:
        """Human-readable packet representation"""
        sys_state = self.get_sys_state()
        subsystem = self.get_subsystem()
        ist = self.get_ist()

        return (f"({sys_state.value}-{subsystem.value}-{ist}) || "
                f"{sys_state.name:4} | {subsystem.name:4} | {ist:2} || "
                f"{self.dat1:3} | {self.dat0:3} | {self.dec:3} || "
                f"{self.control:3}")

    def get_detailed_description(self) -> str:
        """Get detailed packet description based on state and subsystem"""
        sys_state = self.get_sys_state()
        subsystem = self.get_subsystem()
        ist = self.get_ist()

        # IDLE state descriptions
        if sys_state == SystemState.IDLE:
            if subsystem == SubsystemID.HUB and ist == 0:
                return "HUB: Initial contact"
            elif subsystem == SubsystemID.SNC and ist == 0:
                return f"SNC: Ready (Touch={self.dat1}, Distance={self.dat0})"

        # CAL state descriptions
        elif sys_state == SystemState.CAL:
            if subsystem == SubsystemID.SS:
                if ist == 0:
                    return "SS: Calibration start (no touch)"
                elif ist == 1:
                    return "SS: Calibration complete (touch detected)"
            elif subsystem == SubsystemID.MDPS:
                if ist == 0:
                    return f"MDPS: Calibration start (vL={self.dat1}, vR={self.dat0})"
                elif ist == 1:
                    return f"MDPS: Calibration rotation ({self.dat1}°)"
            elif subsystem == SubsystemID.SNC and ist == 0:
                return f"SNC: In calibration (Touch={self.dat1})"

        # MAZE state descriptions
        elif sys_state == SystemState.MAZE:
            if subsystem == SubsystemID.SNC:
                if ist == 1:
                    angle = (self.dat1 << 8) | self.dat0
                    direction = "RIGHT" if self.dec == 2 else "LEFT" if self.dec == 1 else "UNKNOWN"
                    return f"SNC: Rotation request ({angle/10:.1f}° {direction})"
                elif ist == 2:
                    return "SNC: Stop/Reverse command"
                elif ist == 3:
                    return f"SNC: Speed command (vR={self.dat1}, vL={self.dat0}, DEC={self.dec})"

            elif subsystem == SubsystemID.MDPS:
                if ist == 1:
                    return f"MDPS: Stop/Rotate (angle={self.dat1}°)"
                elif ist == 2:
                    return f"MDPS: Confirm (dist={self.dat1}{self.dat0:02d}mm, dir={self.dec})"
                elif ist == 3:
                    return f"MDPS: Forward (vR={self.dat1}, vL={self.dat0})"
                elif ist == 4:
                    dist_m = self.dat1
                    dist_cm = self.dat0
                    return f"MDPS: Distance update ({dist_m}.{dist_cm:02d}m)"

            elif subsystem == SubsystemID.SS:
                if ist == 1:
                    color_desc = self._decode_color_packet()
                    return f"SS: Color data ({color_desc})"
                elif ist == 2:
                    return f"SS: Angle data ({self.dat1}°)"
                elif ist == 3:
                    return "SS: End-of-maze signal"

        # Generic fallback
        return f"{sys_state.name}:{subsystem.name}:IST{ist}"

    def _decode_color_packet(self) -> str:
        """Decode color encoding from SS:MAZE:IST1 packet"""
        color_val = self.dat0

        # Special cases
        if color_val == 0:
            return "All WHITE"
        elif color_val == 73:  # 0b01001001 - all sensors RED
            return "All RED (End-of-Maze)"

        # Extract individual sensor colors
        # Encoding: (S3[7:6] | S2[4:3] | S1[1:0])
        s1 = color_val & 0x03
        s2 = (color_val >> 3) & 0x03
        s3 = (color_val >> 6) & 0x03

        color_map = {0: "WHITE", 1: "RED", 2: "GREEN", 3: "BLUE", 4: "BLACK"}

        # Find which sensor has color
        colors = []
        if s1 in [2, 3, 4]:  # Edge sensor (left)
            colors.append(f"S1={color_map.get(s1, str(s1))}")
        if s2 in [1, 2, 3, 4]:  # Center sensor
            colors.append(f"S2={color_map.get(s2, str(s2))}")
        if s3 in [2, 3, 4]:  # Edge sensor (right)
            colors.append(f"S3={color_map.get(s3, str(s3))}")

        if colors:
            return ", ".join(colors)
        return f"Color code: {color_val}"


# ==================== PACKET CONSTRUCTION HELPERS ====================

def create_control_byte(sys_state: SystemState, subsystem: SubsystemID, ist: int) -> int:
    """
    Create control byte from components

    Args:
        sys_state: System state (IDLE, CAL, MAZE, SOS)
        subsystem: Subsystem ID (HUB, SNC, MDPS, SS)
        ist: Instruction/Status code (0-15)

    Returns:
        8-bit control byte
    """
    if not 0 <= ist <= 15:
        raise ValueError(f"IST must be 0-15, got {ist}")

    return (sys_state.value << 6) | (subsystem.value << 4) | (ist & 0x0F)


def parse_control_byte(control: int) -> Tuple[SystemState, SubsystemID, int]:
    """
    Parse control byte into components

    Args:
        control: 8-bit control byte

    Returns:
        Tuple of (SystemState, SubsystemID, IST)
    """
    sys_state = SystemState((control >> 6) & 0x03)
    subsystem = SubsystemID((control >> 4) & 0x03)
    ist = control & 0x0F
    return sys_state, subsystem, ist


# ==================== COMMON PACKET CONSTRUCTORS ====================

def make_idle_hub_packet() -> SCSPacket:
    """Create IDLE:HUB:0 initial contact packet"""
    ctrl = create_control_byte(SystemState.IDLE, SubsystemID.HUB, 0)
    return SCSPacket(ctrl, 0, 0, 0)


def make_idle_snc_packet(touch_count: int = 1, distance: int = 50) -> SCSPacket:
    """Create IDLE:SNC:0 ready packet"""
    ctrl = create_control_byte(SystemState.IDLE, SubsystemID.SNC, 0)
    return SCSPacket(ctrl, touch_count, distance, 0)


def make_cal_ss_packet(ist: int) -> SCSPacket:
    """Create CAL:SS packet (IST=0 for start, IST=1 for complete)"""
    ctrl = create_control_byte(SystemState.CAL, SubsystemID.SS, ist)
    return SCSPacket(ctrl, 0, 0, 0)


def make_cal_mdps_packet(ist: int, speed: int = 10, angle: int = 90) -> SCSPacket:
    """Create CAL:MDPS packet"""
    ctrl = create_control_byte(SystemState.CAL, SubsystemID.MDPS, ist)
    if ist == 0:  # Start calibration
        return SCSPacket(ctrl, speed, speed, 0)
    elif ist == 1:  # Rotation calibration
        return SCSPacket(ctrl, angle, 0, 0)
    return SCSPacket(ctrl, 0, 0, 0)


def make_maze_mdps_packet(ist: int, dat1: int = 0, dat0: int = 0, dec: int = 0) -> SCSPacket:
    """Create MAZE:MDPS packet"""
    ctrl = create_control_byte(SystemState.MAZE, SubsystemID.MDPS, ist)
    return SCSPacket(ctrl, dat1, dat0, dec)


def make_maze_ss_color_packet(color_code: int) -> SCSPacket:
    """Create MAZE:SS:1 color data packet"""
    ctrl = create_control_byte(SystemState.MAZE, SubsystemID.SS, 1)
    return SCSPacket(ctrl, 0, color_code, 0)


def make_maze_ss_angle_packet(angle: int) -> SCSPacket:
    """Create MAZE:SS:2 angle data packet"""
    ctrl = create_control_byte(SystemState.MAZE, SubsystemID.SS, 2)
    return SCSPacket(ctrl, angle, 0, 0)


def make_maze_ss_eom_packet() -> SCSPacket:
    """Create MAZE:SS:3 end-of-maze packet"""
    ctrl = create_control_byte(SystemState.MAZE, SubsystemID.SS, 3)
    return SCSPacket(ctrl, 0, 0, 0)


# ==================== COLOR ENCODING HELPERS ====================

def encode_color_byte(s1: SensorColor = SensorColor.WHITE,
                     s2: SensorColor = SensorColor.WHITE,
                     s3: SensorColor = SensorColor.WHITE) -> int:
    """
    Encode sensor colors into single byte

    Encoding: (S3[7:6] | S2[4:3] | S1[1:0])

    Args:
        s1: Left edge sensor color
        s2: Center sensor color
        s3: Right edge sensor color

    Returns:
        8-bit color encoding
    """
    # Map enum to bits
    color_bits = {
        SensorColor.WHITE: 0,
        SensorColor.RED: 1,
        SensorColor.GREEN: 2,
        SensorColor.BLUE: 3,
        SensorColor.BLACK: 4
    }

    s1_bits = color_bits[s1] & 0x03
    s2_bits = color_bits[s2] & 0x03
    s3_bits = color_bits[s3] & 0x03

    return (s3_bits << 6) | (s2_bits << 3) | s1_bits


def decode_color_byte(color_byte: int) -> Tuple[SensorColor, SensorColor, SensorColor]:
    """
    Decode color byte into individual sensor colors

    Returns:
        Tuple of (S1, S2, S3) colors
    """
    bits_to_color = {
        0: SensorColor.WHITE,
        1: SensorColor.RED,
        2: SensorColor.GREEN,
        3: SensorColor.BLUE,
        4: SensorColor.BLACK
    }

    s1_bits = color_byte & 0x03
    s2_bits = (color_byte >> 3) & 0x03
    s3_bits = (color_byte >> 6) & 0x03

    s1 = bits_to_color.get(s1_bits, SensorColor.WHITE)
    s2 = bits_to_color.get(s2_bits, SensorColor.WHITE)
    s3 = bits_to_color.get(s3_bits, SensorColor.WHITE)

    return s1, s2, s3


# Predefined color patterns
COLOR_ALL_WHITE = 0
COLOR_S2_GREEN = encode_color_byte(SensorColor.WHITE, SensorColor.GREEN, SensorColor.WHITE)  # 16
COLOR_S2_RED = encode_color_byte(SensorColor.WHITE, SensorColor.RED, SensorColor.WHITE)      # 8
COLOR_S2_BLUE = encode_color_byte(SensorColor.WHITE, SensorColor.BLUE, SensorColor.WHITE)    # 24
COLOR_S2_BLACK = encode_color_byte(SensorColor.WHITE, SensorColor.BLACK, SensorColor.WHITE)  # 32
COLOR_S1_GREEN = encode_color_byte(SensorColor.GREEN, SensorColor.WHITE, SensorColor.WHITE)  # 2
COLOR_S3_GREEN = encode_color_byte(SensorColor.WHITE, SensorColor.WHITE, SensorColor.GREEN)  # 128
COLOR_ALL_RED = 73  # Special end-of-maze pattern


# ==================== VALIDATION FUNCTIONS ====================

def is_valid_transition(current_state: SystemState, next_state: SystemState,
                       conditions_met: dict) -> bool:
    """
    Validate state transition according to SCS state machine

    Args:
        current_state: Current system state
        next_state: Desired next state
        conditions_met: Dictionary of transition conditions

    Returns:
        True if transition is valid
    """
    # IDLE -> CAL requires touch sensor
    if current_state == SystemState.IDLE and next_state == SystemState.CAL:
        return conditions_met.get('touch_sensor', False)

    # CAL -> MAZE requires EOC from SS and MDPS
    if current_state == SystemState.CAL and next_state == SystemState.MAZE:
        return conditions_met.get('ss_eoc', False) and conditions_met.get('mdps_eoc', False)

    # MAZE <-> SOS requires pure tone detection
    if (current_state == SystemState.MAZE and next_state == SystemState.SOS) or \
       (current_state == SystemState.SOS and next_state == SystemState.MAZE):
        return conditions_met.get('pure_tone', False)

    # MAZE -> IDLE requires end-of-maze
    if current_state == SystemState.MAZE and next_state == SystemState.IDLE:
        return conditions_met.get('end_of_maze', False)

    return False


if __name__ == "__main__":
    # Test packet creation and parsing
    print("=== SCS Protocol Test ===\n")

    # Test IDLE packet
    idle_pkt = make_idle_hub_packet()
    print(f"IDLE:HUB:0 = {idle_pkt}")
    print(f"  Description: {idle_pkt.get_detailed_description()}\n")

    # Test CAL packets
    cal_ss_pkt = make_cal_ss_packet(1)
    print(f"CAL:SS:1 = {cal_ss_pkt}")
    print(f"  Description: {cal_ss_pkt.get_detailed_description()}\n")

    # Test MAZE packets with colors
    green_pkt = make_maze_ss_color_packet(COLOR_S2_GREEN)
    print(f"MAZE:SS:1 (GREEN) = {green_pkt}")
    print(f"  Description: {green_pkt.get_detailed_description()}\n")

    # Test angle packet
    angle_pkt = make_maze_ss_angle_packet(35)
    print(f"MAZE:SS:2 (35°) = {angle_pkt}")
    print(f"  Description: {angle_pkt.get_detailed_description()}\n")

    # Test byte conversion
    test_pkt = SCSPacket(177, 0, 16, 0)
    bytes_data = test_pkt.to_bytes()
    print(f"Packet bytes: {' '.join(f'{b:02X}' for b in bytes_data)}")
    reconstructed = SCSPacket.from_bytes(bytes_data)
    print(f"Reconstructed: {reconstructed}\n")

    print("=== Protocol test complete ===")
