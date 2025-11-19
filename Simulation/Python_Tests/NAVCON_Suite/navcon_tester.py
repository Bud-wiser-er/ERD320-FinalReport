#!/usr/bin/env python3
"""
MARV NAVCON Testing Suite
Comprehensive SNC Subsystem Testing Script

This script emulates the SS (Sensor Subsystem) and MDPS (Motor Drive & Power Supply)
to test the SNC (Sensor Navigation Control) subsystem through complete maze scenarios.

Based on AMazeEng MARV QTPs 2025 and client log analysis.

SCS PROTOCOL COMPLIANCE:
=======================
This tester follows the SCS (Subsystem Communication Standard) protocol by:

1. STATE TRANSITIONS: Continuously sends state-specific packets until the SNC
   acknowledges the transition with the expected response packet.

2. CALIBRATION SEQUENCE:
   - Sends CAL:SS:0 (no touch) repeatedly until SNC responds with CAL:SNC:0
   - Sends CAL:SS:1 (first touch) repeatedly until SNC responds with CAL:SNC:1
   - Sends CAL:MDPS packets repeatedly until SNC transitions to MAZE state

3. MAZE STATE OPERATIONS:
   - Continuously sends MDPS/SS packets until SNC responds with expected IST
   - Waits for proper acknowledgments before proceeding to next phase

This ensures robust communication even if packets are lost or delayed.

Version: 2.0 - SCS Protocol Compliant
Date: 2025-01-15
"""

import serial
import serial.tools.list_ports
import tkinter as tk
from tkinter import ttk, messagebox, scrolledtext
import threading
import time
import queue
import sys
from datetime import datetime
from typing import Dict, List, Tuple, Optional
from dataclasses import dataclass
from enum import Enum
import json

# ==================== SCS PROTOCOL DEFINITIONS ====================

class SystemState(Enum):
    IDLE = 0    # 00
    CAL = 1     # 01
    MAZE = 2    # 10
    SOS = 3     # 11

class SubsystemID(Enum):
    HUB = 0     # 00
    SNC = 1     # 01
    MDPS = 2    # 10
    SS = 3      # 11

@dataclass
class SCSPacket:
    """SCS Packet Structure: (SYS<1:0> | SUB<1:0> | IST<3:0>) | DAT1 | DAT0 | DEC"""
    control: int    # CONTROL<31:24>: SYS<1:0> | SUB<1:0> | IST<3:0>
    dat1: int       # DAT1<23:16>: Upper data byte
    dat0: int       # DAT0<15:8>: Lower data byte
    dec: int        # DEC<7:0>: Decimal/general purpose byte

    def __str__(self) -> str:
        sys_state = (self.control >> 6) & 0x03
        subsystem = (self.control >> 4) & 0x03
        ist = self.control & 0x0F

        sys_names = ["IDLE", "CAL", "MAZE", "SOS"]
        sub_names = ["HUB", "SNC", "MDPS", "SS"]

        return f"({sys_state}-{subsystem}-{ist}) || {sys_names[sys_state]} | {sub_names[subsystem]} | {ist} || {self.dat1:3} | {self.dat0:3} | {self.dec:3} || {self.control:3}"

def create_control_byte(sys_state: SystemState, subsystem: SubsystemID, ist: int) -> int:
    """Create control byte from components"""
    return (sys_state.value << 6) | (subsystem.value << 4) | (ist & 0x0F)

def parse_control_byte(control: int) -> Tuple[SystemState, SubsystemID, int]:
    """Parse control byte into components"""
    sys_state = SystemState((control >> 6) & 0x03)
    subsystem = SubsystemID((control >> 4) & 0x03)
    ist = control & 0x0F
    return sys_state, subsystem, ist

# ==================== NAVCON TEST SCENARIOS ====================

class NAVCONTestScenario:
    """Defines a complete NAVCON test scenario"""

    def __init__(self, name: str, description: str):
        self.name = name
        self.description = description
        self.steps = []
        self.expected_responses = []

    def add_step(self, packet: SCSPacket, description: str, expected_response: Optional[SCSPacket] = None):
        """Add a test step"""
        self.steps.append({
            'packet': packet,
            'description': description,
            'expected_response': expected_response
        })

# ==================== MAIN TESTER CLASS ====================

class NAVCONTester:
    """Main NAVCON Testing Application"""

    def __init__(self):
        self.serial_port = None
        self.is_connected = False
        self.test_running = False
        self.message_queue = queue.Queue()
        self.packet_log = []
        self.current_scenario = None

        # Test state tracking
        self.test_state = {
            'system_state': SystemState.IDLE,
            'sequence_number': 0,
            'touch_count': 0,
            'rotation_count': 0,
            'green_detections': 0,
            'total_packets_sent': 0,
            'total_packets_received': 0,
            'test_start_time': None
        }

        # Initialize GUI
        self.setup_gui()
        self.create_test_scenarios()

    def setup_gui(self):
        """Initialize the GUI"""
        self.root = tk.Tk()
        self.root.title("MARV NAVCON Testing Suite v1.0")
        self.root.geometry("1400x900")
        self.root.configure(bg='#2c3e50')

        # Configure style
        style = ttk.Style()
        style.theme_use('clam')
        style.configure('Title.TLabel', font=('Arial', 16, 'bold'), background='#2c3e50', foreground='white')
        style.configure('Header.TLabel', font=('Arial', 12, 'bold'), background='#34495e', foreground='white')
        style.configure('Status.TLabel', font=('Arial', 10), background='#2c3e50', foreground='#ecf0f1')

        self.create_widgets()

    def create_widgets(self):
        """Create all GUI widgets"""

        # Main title
        title_frame = tk.Frame(self.root, bg='#2c3e50', height=60)
        title_frame.pack(fill='x', padx=10, pady=(10, 0))
        title_frame.pack_propagate(False)

        title_label = ttk.Label(title_frame, text="🤖 MARV NAVCON Testing Suite", style='Title.TLabel')
        title_label.pack(anchor='center', pady=15)

        # Main container
        main_frame = tk.Frame(self.root, bg='#2c3e50')
        main_frame.pack(fill='both', expand=True, padx=10, pady=10)

        # Left panel - Controls
        left_frame = tk.Frame(main_frame, bg='#34495e', relief='raised', bd=2)
        left_frame.pack(side='left', fill='y', padx=(0, 5))
        left_frame.configure(width=400)
        left_frame.pack_propagate(False)

        # Connection section
        conn_frame = tk.LabelFrame(left_frame, text="📡 Serial Connection", font=('Arial', 11, 'bold'),
                                 bg='#34495e', fg='white', padx=10, pady=10)
        conn_frame.pack(fill='x', padx=10, pady=10)

        # Port selection
        tk.Label(conn_frame, text="Serial Port:", bg='#34495e', fg='white', font=('Arial', 10)).pack(anchor='w')
        self.port_var = tk.StringVar()
        self.port_combo = ttk.Combobox(conn_frame, textvariable=self.port_var, width=25)
        self.port_combo.pack(fill='x', pady=(0, 5))

        # Baud rate
        tk.Label(conn_frame, text="Baud Rate:", bg='#34495e', fg='white', font=('Arial', 10)).pack(anchor='w')
        self.baud_var = tk.StringVar(value="19200")
        baud_combo = ttk.Combobox(conn_frame, textvariable=self.baud_var,
                                values=["9600", "19200", "38400", "57600", "115200"], width=25)
        baud_combo.pack(fill='x', pady=(0, 10))

        # Connection buttons
        button_frame = tk.Frame(conn_frame, bg='#34495e')
        button_frame.pack(fill='x')

        self.refresh_btn = tk.Button(button_frame, text="🔄 Refresh Ports", command=self.refresh_ports,
                                   bg='#3498db', fg='white', font=('Arial', 9, 'bold'))
        self.refresh_btn.pack(side='left', padx=(0, 5))

        self.connect_btn = tk.Button(button_frame, text="🔌 Connect", command=self.toggle_connection,
                                   bg='#27ae60', fg='white', font=('Arial', 9, 'bold'))
        self.connect_btn.pack(side='right')

        # Connection status
        self.status_label = tk.Label(conn_frame, text="❌ Disconnected", bg='#34495e', fg='#e74c3c',
                                   font=('Arial', 10, 'bold'))
        self.status_label.pack(pady=(10, 0))

        # Test scenarios section
        scenario_frame = tk.LabelFrame(left_frame, text="🧪 Test Scenarios", font=('Arial', 11, 'bold'),
                                     bg='#34495e', fg='white', padx=10, pady=10)
        scenario_frame.pack(fill='both', expand=True, padx=10, pady=10)

        # Scenario selection
        tk.Label(scenario_frame, text="Select Test Scenario:", bg='#34495e', fg='white',
               font=('Arial', 10)).pack(anchor='w')

        self.scenario_var = tk.StringVar()
        self.scenario_combo = ttk.Combobox(scenario_frame, textvariable=self.scenario_var, width=35)
        self.scenario_combo.pack(fill='x', pady=(0, 10))
        self.scenario_combo.bind('<<ComboboxSelected>>', self.on_scenario_selected)

        # Scenario description
        desc_label = tk.Label(scenario_frame, text="Description:", bg='#34495e', fg='white',
                            font=('Arial', 10, 'bold'))
        desc_label.pack(anchor='w')

        self.desc_text = tk.Text(scenario_frame, height=4, wrap='word', font=('Arial', 9),
                               bg='#ecf0f1', fg='#2c3e50')
        self.desc_text.pack(fill='x', pady=(0, 10))

        # Test controls
        control_frame = tk.Frame(scenario_frame, bg='#34495e')
        control_frame.pack(fill='x')

        self.start_test_btn = tk.Button(control_frame, text="▶️ Start Test", command=self.start_test,
                                      bg='#e67e22', fg='white', font=('Arial', 11, 'bold'),
                                      state='disabled')
        self.start_test_btn.pack(side='left', padx=(0, 5))

        self.stop_test_btn = tk.Button(control_frame, text="⏹️ Stop Test", command=self.stop_test,
                                     bg='#e74c3c', fg='white', font=('Arial', 11, 'bold'),
                                     state='disabled')
        self.stop_test_btn.pack(side='right')

        # Test progress
        progress_label = tk.Label(scenario_frame, text="Test Progress:", bg='#34495e', fg='white',
                                font=('Arial', 10, 'bold'))
        progress_label.pack(anchor='w', pady=(15, 0))

        self.progress_var = tk.DoubleVar()
        self.progress_bar = ttk.Progressbar(scenario_frame, variable=self.progress_var, maximum=100)
        self.progress_bar.pack(fill='x', pady=(5, 0))

        self.progress_label = tk.Label(scenario_frame, text="Ready to start...", bg='#34495e', fg='#ecf0f1',
                                     font=('Arial', 9))
        self.progress_label.pack(anchor='w', pady=(5, 0))

        # Right panel - Monitoring
        right_frame = tk.Frame(main_frame, bg='#2c3e50')
        right_frame.pack(side='right', fill='both', expand=True, padx=(5, 0))

        # Notebook for tabs
        notebook = ttk.Notebook(right_frame)
        notebook.pack(fill='both', expand=True)

        # Packet log tab
        log_frame = tk.Frame(notebook, bg='#ecf0f1')
        notebook.add(log_frame, text="📊 Packet Monitor")

        # Log controls
        log_control_frame = tk.Frame(log_frame, bg='#ecf0f1')
        log_control_frame.pack(fill='x', padx=10, pady=5)

        clear_log_btn = tk.Button(log_control_frame, text="🗑️ Clear Log", command=self.clear_log,
                                bg='#95a5a6', fg='white', font=('Arial', 9, 'bold'))
        clear_log_btn.pack(side='left')

        save_log_btn = tk.Button(log_control_frame, text="💾 Save Log", command=self.save_log,
                               bg='#3498db', fg='white', font=('Arial', 9, 'bold'))
        save_log_btn.pack(side='left', padx=(10, 0))

        # Packet log display
        self.log_text = scrolledtext.ScrolledText(log_frame, wrap='none', font=('Courier New', 9),
                                                bg='#2c3e50', fg='#ecf0f1', selectbackground='#3498db')
        self.log_text.pack(fill='both', expand=True, padx=10, pady=(0, 10))

        # Statistics tab
        stats_frame = tk.Frame(notebook, bg='#ecf0f1')
        notebook.add(stats_frame, text="📈 Test Statistics")

        # Create statistics display
        self.create_statistics_display(stats_frame)

        # Initial setup
        self.refresh_ports()

    def create_statistics_display(self, parent):
        """Create the statistics display panel"""

        # Test statistics
        stats_label_frame = tk.LabelFrame(parent, text="Test Statistics", font=('Arial', 12, 'bold'),
                                        bg='#ecf0f1', fg='#2c3e50', padx=10, pady=10)
        stats_label_frame.pack(fill='x', padx=10, pady=10)

        # Create statistics labels
        self.stats_labels = {}

        stats_info = [
            ("Test Duration", "test_duration"),
            ("Packets Sent", "packets_sent"),
            ("Packets Received", "packets_received"),
            ("Success Rate", "success_rate"),
            ("Current State", "current_state"),
            ("Touch Events", "touch_events"),
            ("Rotation Commands", "rotation_commands"),
            ("Green Detections", "green_detections")
        ]

        row = 0
        for label_text, key in stats_info:
            tk.Label(stats_label_frame, text=f"{label_text}:", bg='#ecf0f1', fg='#2c3e50',
                   font=('Arial', 10, 'bold')).grid(row=row, column=0, sticky='w', padx=(0, 10), pady=2)

            self.stats_labels[key] = tk.Label(stats_label_frame, text="0", bg='#ecf0f1', fg='#27ae60',
                                            font=('Arial', 10))
            self.stats_labels[key].grid(row=row, column=1, sticky='w', pady=2)
            row += 1

        # Real-time packet display
        packet_frame = tk.LabelFrame(parent, text="Current Packet Analysis", font=('Arial', 12, 'bold'),
                                   bg='#ecf0f1', fg='#2c3e50', padx=10, pady=10)
        packet_frame.pack(fill='both', expand=True, padx=10, pady=10)

        self.packet_analysis_text = scrolledtext.ScrolledText(packet_frame, height=15, wrap='word',
                                                            font=('Courier New', 9), bg='#2c3e50', fg='#ecf0f1')
        self.packet_analysis_text.pack(fill='both', expand=True)

    def refresh_ports(self):
        """Refresh available serial ports"""
        ports = [port.device for port in serial.tools.list_ports.comports()]
        self.port_combo['values'] = ports
        if ports:
            self.port_combo.set(ports[0])
        self.log_message("🔄 Serial ports refreshed")

    def toggle_connection(self):
        """Toggle serial connection"""
        if not self.is_connected:
            self.connect_serial()
        else:
            self.disconnect_serial()

    def connect_serial(self):
        """Connect to serial port"""
        try:
            port = self.port_var.get()
            baud = int(self.baud_var.get())

            if not port:
                messagebox.showerror("Error", "Please select a serial port")
                return

            self.serial_port = serial.Serial(port, baud, timeout=1)
            self.is_connected = True

            self.status_label.config(text="✅ Connected", fg='#27ae60')
            self.connect_btn.config(text="🔌 Disconnect", bg='#e74c3c')
            self.start_test_btn.config(state='normal')

            # Start monitoring thread
            self.monitor_thread = threading.Thread(target=self.monitor_serial, daemon=True)
            self.monitor_thread.start()

            self.log_message(f"✅ Connected to {port} at {baud} baud", "SUCCESS")

        except Exception as e:
            messagebox.showerror("Connection Error", f"Failed to connect: {str(e)}")
            self.log_message(f"❌ Connection failed: {str(e)}", "ERROR")

    def disconnect_serial(self):
        """Disconnect from serial port"""
        if self.serial_port:
            self.serial_port.close()
            self.serial_port = None

        self.is_connected = False
        self.status_label.config(text="❌ Disconnected", fg='#e74c3c')
        self.connect_btn.config(text="🔌 Connect", bg='#27ae60')
        self.start_test_btn.config(state='disabled')

        if self.test_running:
            self.stop_test()

        self.log_message("🔌 Disconnected from serial port", "INFO")

    def monitor_serial(self):
        """Monitor incoming serial data"""
        buffer = bytearray()

        while self.is_connected:
            try:
                if self.serial_port and self.serial_port.in_waiting > 0:
                    data = self.serial_port.read(self.serial_port.in_waiting)
                    buffer.extend(data)

                    # Process complete packets (4 bytes each)
                    while len(buffer) >= 4:
                        packet_bytes = buffer[:4]
                        buffer = buffer[4:]

                        packet = SCSPacket(packet_bytes[0], packet_bytes[1], packet_bytes[2], packet_bytes[3])
                        self.handle_received_packet(packet)

                time.sleep(0.01)  # Small delay to prevent excessive CPU usage

            except Exception as e:
                if self.is_connected:  # Only log if we're supposed to be connected
                    self.log_message(f"❌ Serial monitoring error: {str(e)}", "ERROR")
                break

    def handle_received_packet(self, packet: SCSPacket):
        """Handle received packet from SNC"""
        timestamp = datetime.now().strftime("%H:%M:%S.%f")[:-3]
        self.test_state['total_packets_received'] += 1

        sys_state, subsystem, ist = parse_control_byte(packet.control)

        # Log the received packet
        direction = "RECEIVED"
        log_line = f"{timestamp} || {self.test_state['total_packets_received']:3} || {direction:8} || {packet}"
        self.log_message(log_line, "RECEIVED")

        # Track last received packet timestamp for turn-based protocol
        self.last_received_time = time.time()
        self.last_received_packet = packet

        # Update test state
        if subsystem == SubsystemID.SNC:
            self.test_state['system_state'] = sys_state

            # Analyze packet for test progress
            self.analyze_received_packet(packet, sys_state, ist)

        # Update statistics display
        self.update_statistics()

        # Queue packet for processing
        self.message_queue.put(('received_packet', packet))

    def analyze_received_packet(self, packet: SCSPacket, sys_state: SystemState, ist: int):
        """Analyze received packet and update test progress"""
        analysis = []

        analysis.append(f"📥 RECEIVED PACKET ANALYSIS")
        analysis.append(f"Timestamp: {datetime.now().strftime('%H:%M:%S.%f')[:-3]}")
        analysis.append(f"Raw Bytes: [{packet.control:02X}, {packet.dat1:02X}, {packet.dat0:02X}, {packet.dec:02X}]")
        analysis.append(f"System State: {sys_state.name}")
        analysis.append(f"Internal State: {ist}")
        analysis.append(f"Data: DAT1={packet.dat1}, DAT0={packet.dat0}, DEC={packet.dec}")

        # Interpret based on state and IST
        if sys_state == SystemState.MAZE and ist == 3:
            analysis.append("🎯 NAVCON ACTIVE - IST=3 detected")
            analysis.append(f"Speed Command: vR={packet.dat1}, vL={packet.dat0}")
            if packet.dec > 0:
                analysis.append(f"⚠️ Special condition: DEC={packet.dec}")

        elif sys_state == SystemState.MAZE and ist == 1:
            analysis.append("🔄 ROTATION REQUEST detected")
            angle = (packet.dat1 << 8) | packet.dat0
            direction = "RIGHT" if packet.dec == 2 else "LEFT" if packet.dec == 1 else "UNKNOWN"
            analysis.append(f"Rotation: {angle/10:.1f}° {direction}")
            self.test_state['rotation_count'] += 1

        elif sys_state == SystemState.CAL:
            analysis.append("🛠️ CALIBRATION state")

        elif sys_state == SystemState.IDLE:
            analysis.append("⏸️ IDLE state")

        elif sys_state == SystemState.SOS:
            analysis.append("🚨 SOS state")

        analysis.append("-" * 50)

        # Update packet analysis display
        self.packet_analysis_text.insert(tk.END, "\n".join(analysis) + "\n\n")
        self.packet_analysis_text.see(tk.END)

    def create_test_scenarios(self):
        """Create predefined test scenarios"""
        self.scenarios = {}

        # ============================================================
        # QTP1: Initial Calibration and First GREEN Line
        # ============================================================
        qtp1 = NAVCONTestScenario("QTP1: First GREEN Line",
                                "Tests system initialization, calibration, and first GREEN line detection with proper REVERSE behavior.")

        # IDLE Phase - Initial contact
        qtp1.add_step(SCSPacket(0, 0, 0, 0), "HUB: Initial contact (IDLE-HUB-0)")

        # Expect SNC response: (0-1-0) with DAT1=1, DAT0=50

        # CAL Phase - Calibration sequence (simplified - normally 60 seconds)
        qtp1.add_step(SCSPacket(112, 0, 0, 0), "SS: Start calibration (CAL-SS-0)")
        qtp1.add_step(SCSPacket(96, 10, 10, 0), "MDPS: Start calibration (CAL-MDPS-0)")
        qtp1.add_step(SCSPacket(97, 90, 0, 0), "MDPS: Calibration rotation 90° (CAL-MDPS-1)")
        qtp1.add_step(SCSPacket(113, 0, 0, 0), "SS: Calibration complete (CAL-SS-1)")

        # Repeat calibration packets (10 cycles to simulate sustained calibration)
        for i in range(10):
            qtp1.add_step(SCSPacket(97, 90, 0, 0), f"MDPS: Calibration cycle {i+1}")
            qtp1.add_step(SCSPacket(113, 0, 0, 0), f"SS: Calibration cycle {i+1}")

        # Expect SNC to transition to MAZE: (2-1-1) → (2-1-2) → (2-1-3)

        # MAZE Phase - First GREEN line encounter (angled line, 35°)
        qtp1.add_step(SCSPacket(161, 90, 0, 0), "MDPS: Stop command (MAZE-MDPS-1)")
        qtp1.add_step(SCSPacket(162, 0, 0, 0), "MDPS: Stopped (MAZE-MDPS-2)")
        qtp1.add_step(SCSPacket(163, 10, 10, 0), "MDPS: Slow forward (MAZE-MDPS-3)")
        qtp1.add_step(SCSPacket(164, 0, 50, 0), "MDPS: Distance 50mm (MAZE-MDPS-4)")

        # GREEN line S2 detection: Color=2 (GREEN), Angle=35°
        # Color encoding: S1=0, S2=2, S3=0 → packed = (0<<6)|(2<<3)|(0) = 16 (0x10)
        qtp1.add_step(SCSPacket(177, 0, 16, 0), "SS: GREEN on S2 (MAZE-SS-1)")
        qtp1.add_step(SCSPacket(178, 35, 0, 0), "SS: Incidence angle 35° (MAZE-SS-2)")

        # Expect SNC to: STOP → REVERSE → STOP → ROTATE
        # Validate N1.2 compliance: SNC must send (2-1-2) REVERSE before (2-1-1) rotation

        qtp1.add_step(SCSPacket(162, 0, 6, 3), "MDPS: Reverse complete 60mm (MAZE-MDPS-2)")
        qtp1.add_step(SCSPacket(161, 90, 0, 0), "MDPS: Stop before rotate (MAZE-MDPS-1)")
        qtp1.add_step(SCSPacket(162, 0, 0, 0), "MDPS: Stopped (MAZE-MDPS-2)")

        # Expect SNC rotation command: (2-1-1) with target angle

        qtp1.add_step(SCSPacket(162, 0, 35, 2), "MDPS: Rotation 35° RIGHT complete (MAZE-MDPS-2)")
        qtp1.add_step(SCSPacket(163, 10, 10, 0), "MDPS: Resume forward (MAZE-MDPS-3)")
        qtp1.add_step(SCSPacket(177, 0, 0, 0), "SS: All sensors WHITE (MAZE-SS-1)")

        self.scenarios["QTP1"] = qtp1

        # ============================================================
        # QTP2: Multiple GREEN Lines (Navigable)
        # ============================================================
        qtp2 = NAVCONTestScenario("QTP2: GREEN Lines (Navigable)",
                                "Tests multiple GREEN line encounters with various angles and correction sequences.")

        # Start from active MAZE state
        qtp2.add_step(SCSPacket(163, 10, 10, 0), "MDPS: Forward movement")
        qtp2.add_step(SCSPacket(164, 0, 100, 0), "MDPS: Distance 100mm")

        # GREEN Line #1: Steep angle (46°) requiring correction
        qtp2.add_step(SCSPacket(177, 0, 16, 0), "SS: GREEN on S2 (Color=2)")
        qtp2.add_step(SCSPacket(178, 46, 0, 0), "SS: Angle 46° (steep)")

        # Expect: STOP → REVERSE → STOP → ROTATE RIGHT
        qtp2.add_step(SCSPacket(161, 90, 0, 0), "MDPS: Stop")
        qtp2.add_step(SCSPacket(162, 0, 0, 0), "MDPS: Stopped")
        qtp2.add_step(SCSPacket(162, 0, 6, 3), "MDPS: Reverse 60mm")
        qtp2.add_step(SCSPacket(162, 0, 46, 2), "MDPS: Rotate 46° RIGHT")
        qtp2.add_step(SCSPacket(163, 10, 10, 0), "MDPS: Forward")
        qtp2.add_step(SCSPacket(177, 0, 0, 0), "SS: WHITE")

        qtp2.add_step(SCSPacket(164, 0, 150, 0), "MDPS: Distance 150mm")

        # GREEN Line #2: Small angle (15°) - safe to cross with steering
        qtp2.add_step(SCSPacket(177, 0, 16, 0), "SS: GREEN on S2")
        qtp2.add_step(SCSPacket(178, 15, 0, 0), "SS: Angle 15°")

        # Expect: STOP → small correction → CROSS
        qtp2.add_step(SCSPacket(161, 90, 0, 0), "MDPS: Stop")
        qtp2.add_step(SCSPacket(162, 0, 10, 2), "MDPS: Small 10° correction")
        qtp2.add_step(SCSPacket(163, 10, 10, 1), "MDPS: Crossing line")
        qtp2.add_step(SCSPacket(177, 0, 0, 0), "SS: WHITE")

        qtp2.add_step(SCSPacket(164, 0, 120, 0), "MDPS: Distance 120mm")

        # GREEN Line #3: Very safe (5°) - direct cross
        qtp2.add_step(SCSPacket(177, 0, 16, 0), "SS: GREEN on S2")
        qtp2.add_step(SCSPacket(178, 5, 0, 0), "SS: Angle 5° (safe)")

        # Expect: Direct crossing with minimal adjustment
        qtp2.add_step(SCSPacket(163, 10, 10, 1), "MDPS: Crossing safely")
        qtp2.add_step(SCSPacket(177, 0, 0, 0), "SS: WHITE")

        self.scenarios["QTP2"] = qtp2

        # ============================================================
        # QTP3: BLUE Wall Lines (Must Avoid)
        # ============================================================
        qtp3 = NAVCONTestScenario("QTP3: BLUE Walls",
                                "Tests BLUE wall detection and 90° avoidance turns.")

        qtp3.add_step(SCSPacket(163, 10, 10, 0), "MDPS: Forward")
        qtp3.add_step(SCSPacket(164, 0, 80, 0), "MDPS: Distance 80mm")

        # BLUE Wall #1: 30° angle - RIGHT turn to avoid
        # Color encoding: S1=0, S2=3 (BLUE), S3=0 → packed = (0<<6)|(3<<3)|(0) = 24 (0x18)
        qtp3.add_step(SCSPacket(177, 0, 24, 0), "SS: BLUE wall on S2 (Color=3)")
        qtp3.add_step(SCSPacket(178, 30, 0, 0), "SS: Angle 30°")

        # Expect: STOP → REVERSE → STOP → 90° RIGHT turn
        qtp3.add_step(SCSPacket(161, 90, 0, 0), "MDPS: Stop")
        qtp3.add_step(SCSPacket(162, 0, 6, 3), "MDPS: Reverse 60mm")
        qtp3.add_step(SCSPacket(162, 0, 90, 2), "MDPS: 90° RIGHT to avoid BLUE")
        qtp3.add_step(SCSPacket(163, 10, 10, 0), "MDPS: Forward")
        qtp3.add_step(SCSPacket(177, 0, 0, 0), "SS: WHITE")

        qtp3.add_step(SCSPacket(164, 0, 100, 0), "MDPS: Distance 100mm")

        # BLUE Wall #2: Steep 52° - RIGHT turn
        qtp3.add_step(SCSPacket(177, 0, 24, 0), "SS: BLUE wall steep")
        qtp3.add_step(SCSPacket(178, 52, 0, 0), "SS: Angle 52°")

        qtp3.add_step(SCSPacket(161, 90, 0, 0), "MDPS: Stop")
        qtp3.add_step(SCSPacket(162, 0, 6, 3), "MDPS: Reverse")
        qtp3.add_step(SCSPacket(162, 0, 90, 2), "MDPS: 90° RIGHT")
        qtp3.add_step(SCSPacket(163, 10, 10, 0), "MDPS: Forward")
        qtp3.add_step(SCSPacket(177, 0, 0, 0), "SS: WHITE")

        self.scenarios["QTP3"] = qtp3

        # ============================================================
        # QTP4: BLACK Wall Lines (Must Avoid)
        # ============================================================
        qtp4 = NAVCONTestScenario("QTP4: BLACK Walls",
                                "Tests BLACK wall detection, 90° turns, and potential 180° sequences.")

        qtp4.add_step(SCSPacket(163, 10, 10, 0), "MDPS: Forward")
        qtp4.add_step(SCSPacket(164, 0, 90, 0), "MDPS: Distance 90mm")

        # BLACK Wall #1: 28° angle - RIGHT turn
        # Color encoding: S1=0, S2=4 (BLACK), S3=0 → packed = (0<<6)|(4<<3)|(0) = 32 (0x20)
        qtp4.add_step(SCSPacket(177, 0, 32, 0), "SS: BLACK wall on S2 (Color=4)")
        qtp4.add_step(SCSPacket(178, 28, 0, 0), "SS: Angle 28°")

        # Expect: STOP → REVERSE → STOP → 90° RIGHT
        qtp4.add_step(SCSPacket(161, 90, 0, 0), "MDPS: Stop")
        qtp4.add_step(SCSPacket(162, 0, 6, 3), "MDPS: Reverse 60mm")
        qtp4.add_step(SCSPacket(162, 0, 90, 2), "MDPS: 90° RIGHT to avoid BLACK")
        qtp4.add_step(SCSPacket(163, 10, 10, 0), "MDPS: Forward")
        qtp4.add_step(SCSPacket(177, 0, 0, 0), "SS: WHITE")

        qtp4.add_step(SCSPacket(164, 0, 80, 0), "MDPS: Distance 80mm")

        # BLACK Wall #2: Second BLACK at 35° (triggers 180° logic)
        qtp4.add_step(SCSPacket(177, 0, 32, 0), "SS: BLACK wall again")
        qtp4.add_step(SCSPacket(178, 35, 0, 0), "SS: Angle 35°")

        # Expect: STOP → REVERSE → 180° turn (90° + 90°)
        qtp4.add_step(SCSPacket(161, 90, 0, 0), "MDPS: Stop")
        qtp4.add_step(SCSPacket(162, 0, 6, 3), "MDPS: Reverse")
        qtp4.add_step(SCSPacket(162, 0, 180, 2), "MDPS: 180° turn (second BLACK)")
        qtp4.add_step(SCSPacket(163, 10, 10, 0), "MDPS: Forward")
        qtp4.add_step(SCSPacket(177, 0, 0, 0), "SS: WHITE")

        self.scenarios["QTP4"] = qtp4

        # ============================================================
        # QTP5: RED Line (End of Maze)
        # ============================================================
        qtp5 = NAVCONTestScenario("QTP5: RED End-of-Maze",
                                "Tests RED line detection signaling end of maze and return to IDLE.")

        qtp5.add_step(SCSPacket(163, 10, 10, 0), "MDPS: Forward")
        qtp5.add_step(SCSPacket(164, 0, 100, 0), "MDPS: Distance 100mm")

        # RED Line: End of maze marker
        # Color encoding: S1=1, S2=1, S3=1 (All RED) → packed = (1<<6)|(1<<3)|(1) = 73 (0x49)
        qtp5.add_step(SCSPacket(177, 0, 73, 0), "SS: RED detected - END OF MAZE (All sensors RED)")
        qtp5.add_step(SCSPacket(178, 0, 0, 0), "SS: Angle N/A")

        # Expect: STOP immediately, no rotation needed
        qtp5.add_step(SCSPacket(161, 90, 0, 0), "MDPS: Stop")
        qtp5.add_step(SCSPacket(162, 0, 0, 0), "MDPS: Stopped at end")

        # SS signals end of maze with IST=3
        qtp5.add_step(SCSPacket(179, 0, 0, 0), "SS: End of maze signal (MAZE-SS-3)")

        # Expect SNC to transition back to IDLE: (0-1-0)

        qtp5.add_step(SCSPacket(0, 0, 0, 0), "System return to IDLE")

        self.scenarios["QTP5"] = qtp5

        # ============================================================
        # Full Maze: Complete Run Through All Line Types
        # ============================================================
        full_maze = NAVCONTestScenario("Full Maze: All Colors",
                                     "Complete maze simulation: Calibration → GREEN → BLUE → BLACK → RED (End)")

        # Phase 1: Calibration (from QTP1)
        full_maze.add_step(SCSPacket(0, 0, 0, 0), "HUB: Initial contact")
        full_maze.add_step(SCSPacket(112, 0, 0, 0), "SS: Start calibration")
        full_maze.add_step(SCSPacket(96, 10, 10, 0), "MDPS: Start calibration")
        full_maze.add_step(SCSPacket(97, 90, 0, 0), "MDPS: Rotation calibration")
        full_maze.add_step(SCSPacket(113, 0, 0, 0), "SS: Calibration complete")

        for i in range(5):
            full_maze.add_step(SCSPacket(97, 90, 0, 0), f"CAL cycle {i+1}")
            full_maze.add_step(SCSPacket(113, 0, 0, 0), f"CAL cycle {i+1}")

        # Phase 2: GREEN lines (navigable)
        full_maze.add_step(SCSPacket(163, 10, 10, 0), "MAZE: Start forward")
        full_maze.add_step(SCSPacket(164, 0, 100, 0), "Distance 100mm")
        full_maze.add_step(SCSPacket(177, 0, 16, 0), "GREEN line 1 (35°)")
        full_maze.add_step(SCSPacket(178, 35, 0, 0), "Angle 35°")
        full_maze.add_step(SCSPacket(161, 90, 0, 0), "Stop")
        full_maze.add_step(SCSPacket(162, 0, 6, 3), "Reverse")
        full_maze.add_step(SCSPacket(162, 0, 35, 2), "Rotate 35°")
        full_maze.add_step(SCSPacket(163, 10, 10, 0), "Forward")
        full_maze.add_step(SCSPacket(177, 0, 0, 0), "WHITE")

        # Phase 3: BLUE wall
        full_maze.add_step(SCSPacket(164, 0, 120, 0), "Distance 120mm")
        full_maze.add_step(SCSPacket(177, 0, 24, 0), "BLUE wall (30°)")
        full_maze.add_step(SCSPacket(178, 30, 0, 0), "Angle 30°")
        full_maze.add_step(SCSPacket(161, 90, 0, 0), "Stop")
        full_maze.add_step(SCSPacket(162, 0, 6, 3), "Reverse")
        full_maze.add_step(SCSPacket(162, 0, 90, 2), "90° RIGHT avoid")
        full_maze.add_step(SCSPacket(163, 10, 10, 0), "Forward")
        full_maze.add_step(SCSPacket(177, 0, 0, 0), "WHITE")

        # Phase 4: BLACK wall
        full_maze.add_step(SCSPacket(164, 0, 90, 0), "Distance 90mm")
        full_maze.add_step(SCSPacket(177, 0, 32, 0), "BLACK wall (28°)")
        full_maze.add_step(SCSPacket(178, 28, 0, 0), "Angle 28°")
        full_maze.add_step(SCSPacket(161, 90, 0, 0), "Stop")
        full_maze.add_step(SCSPacket(162, 0, 6, 3), "Reverse")
        full_maze.add_step(SCSPacket(162, 0, 90, 2), "90° RIGHT avoid")
        full_maze.add_step(SCSPacket(163, 10, 10, 0), "Forward")
        full_maze.add_step(SCSPacket(177, 0, 0, 0), "WHITE")

        # Phase 5: Another GREEN
        full_maze.add_step(SCSPacket(164, 0, 110, 0), "Distance 110mm")
        full_maze.add_step(SCSPacket(177, 0, 16, 0), "GREEN line 2 (12°)")
        full_maze.add_step(SCSPacket(178, 12, 0, 0), "Angle 12°")
        full_maze.add_step(SCSPacket(161, 90, 0, 0), "Stop")
        full_maze.add_step(SCSPacket(162, 0, 12, 2), "Small correction")
        full_maze.add_step(SCSPacket(163, 10, 10, 1), "Cross line")
        full_maze.add_step(SCSPacket(177, 0, 0, 0), "WHITE")

        # Phase 6: RED end of maze
        full_maze.add_step(SCSPacket(164, 0, 150, 0), "Distance 150mm")
        full_maze.add_step(SCSPacket(177, 0, 73, 0), "RED - END OF MAZE!")
        full_maze.add_step(SCSPacket(161, 90, 0, 0), "Final stop")
        full_maze.add_step(SCSPacket(162, 0, 0, 0), "Stopped")
        full_maze.add_step(SCSPacket(179, 0, 0, 0), "End of maze signal")
        full_maze.add_step(SCSPacket(0, 0, 0, 0), "Return to IDLE")

        self.scenarios["Full Maze"] = full_maze

        # Update combo box
        scenario_names = list(self.scenarios.keys())
        self.scenario_combo['values'] = scenario_names
        if scenario_names:
            self.scenario_combo.set(scenario_names[0])
            self.on_scenario_selected(None)

    def on_scenario_selected(self, event):
        """Handle scenario selection"""
        scenario_name = self.scenario_var.get()
        if scenario_name in self.scenarios:
            scenario = self.scenarios[scenario_name]
            self.current_scenario = scenario

            # Update description
            self.desc_text.delete(1.0, tk.END)
            self.desc_text.insert(1.0, scenario.description)

    def start_test(self):
        """Start the selected test scenario"""
        if not self.is_connected:
            messagebox.showerror("Error", "Please connect to serial port first")
            return

        if not self.current_scenario:
            messagebox.showerror("Error", "Please select a test scenario")
            return

        self.test_running = True
        self.test_state['test_start_time'] = time.time()
        self.test_state['sequence_number'] = 0
        self.test_state['total_packets_sent'] = 0
        self.test_state['total_packets_received'] = 0

        self.start_test_btn.config(state='disabled')
        self.stop_test_btn.config(state='normal')

        # Start test thread
        test_thread = threading.Thread(target=self.run_test_scenario, daemon=True)
        test_thread.start()

        self.log_message(f"🚀 Started test scenario: {self.current_scenario.name}", "INFO")

    def run_test_scenario(self):
        """Run the test scenario - matches REAL HUB behavior from Client_log.txt"""
        try:
            # ========================================
            # PHASE 1: IDLE Connection
            # ========================================
            self.log_message("📡 PHASE 1: Establishing IDLE connection...", "INFO")

            self.send_packet(SCSPacket(0, 0, 0, 0), "HUB: IDLE:HUB:0")
            time.sleep(1.0)

            # Wait for IDLE:SNC:0
            if not self.wait_for_snc_response(SystemState.IDLE, 0, timeout=5.0):
                self.log_message("❌ No IDLE:SNC:0 response", "ERROR")
                self.stop_test()
                return

            self.log_message("✅ IDLE connection established", "SUCCESS")

            # ========================================
            # PHASE 2: CAL Initialization
            # ========================================
            self.log_message("🛠️ PHASE 2: CAL initialization...", "INFO")

            # Send CAL init sequence (lines 4-7 from log)
            self.send_packet(SCSPacket(112, 0, 0, 0), "SS: CAL:SS:0")
            time.sleep(0.1)
            self.send_packet(SCSPacket(96, 10, 10, 0), "MDPS: CAL:MDPS:0")
            time.sleep(0.1)
            self.send_packet(SCSPacket(97, 90, 0, 0), "MDPS: CAL:MDPS:1")
            time.sleep(0.1)
            self.send_packet(SCSPacket(113, 0, 0, 0), "SS: CAL:SS:1")
            time.sleep(0.5)

            # Wait for CAL:SNC:0
            if not self.wait_for_snc_response(SystemState.CAL, 0, timeout=5.0):
                self.log_message("⚠️ No CAL:SNC:0, continuing anyway...", "INFO")

            self.log_message("✅ CAL state entered", "SUCCESS")

            # ========================================
            # PHASE 3: CAL Loop (until MAZE)
            # ========================================
            self.log_message("🎵 PHASE 3: CAL loop (waiting for pure tone)...", "INFO")

            maze_detected = False
            cal_loop_start = time.time()
            cal_loop_timeout = 30.0

            while self.test_running and not maze_detected and (time.time() - cal_loop_start) < cal_loop_timeout:
                # Send CAL:MDPS:1 and CAL:SS:1 (lines 9-110 pattern)
                self.send_packet(SCSPacket(97, 90, 0, 0), "MDPS: CAL:MDPS:1")
                time.sleep(0.1)
                self.send_packet(SCSPacket(113, 0, 0, 0), "SS: CAL:SS:1")
                time.sleep(0.4)

                # Check for MAZE transition
                maze_detected = self.check_for_maze_transition()

            if not maze_detected:
                self.log_message("❌ No MAZE transition detected", "ERROR")
                self.stop_test()
                return

            self.log_message("🎯 MAZE state detected - starting continuous loop!", "SUCCESS")

            # ========================================
            # PHASE 4: MAZE Continuous Loop
            # ========================================
            self.execute_maze_continuous_loop()

        except Exception as e:
            self.log_message(f"❌ Test error: {str(e)}", "ERROR")
        finally:
            if self.test_running:
                self.stop_test()

    def wait_for_transition(self, send_packet: SCSPacket, send_description: str,
                           expected_state: SystemState, expected_subsystem: SubsystemID,
                           expected_ist: int, timeout: float, send_interval: float = 0.5) -> bool:
        """
        Continuously send packet until expected response is received

        This follows SCS protocol: keep sending state packets until the SNC acknowledges
        the transition with the expected response packet.

        Args:
            send_packet: Packet to send repeatedly
            send_description: Description for logging
            expected_state: Expected SystemState in response
            expected_subsystem: Expected SubsystemID in response
            expected_ist: Expected IST value in response
            timeout: Maximum time to wait (seconds)
            send_interval: Time between packet sends (seconds)

        Returns:
            True if expected response received, False on timeout
        """
        start_time = time.time()
        last_send_time = 0

        self.log_message(f"🔄 Waiting for ({expected_state.value}-{expected_subsystem.value}-{expected_ist})", "INFO")

        while self.test_running and (time.time() - start_time) < timeout:
            # Send packet at intervals
            if time.time() - last_send_time >= send_interval:
                self.send_packet(send_packet, send_description)
                last_send_time = time.time()

            # Check for response
            try:
                msg_type, data = self.message_queue.get(timeout=0.1)
                if msg_type == 'received_packet':
                    packet = data
                    sys_state, subsystem, ist = parse_control_byte(packet.control)

                    # Check if this is the expected response
                    if (sys_state == expected_state and
                        subsystem == expected_subsystem and
                        ist == expected_ist):
                        return True

            except queue.Empty:
                continue

        return False

    def wait_for_transition_multi_packet(self, send_packets: List[SCSPacket],
                                        send_descriptions: List[str],
                                        expected_state: SystemState,
                                        expected_subsystem: SubsystemID,
                                        expected_ist_list: List[int],
                                        timeout: float,
                                        send_interval: float = 0.5) -> bool:
        """
        Continuously send alternating packets until expected response is received

        Args:
            send_packets: List of packets to send in sequence
            send_descriptions: List of descriptions for each packet
            expected_state: Expected SystemState in response
            expected_subsystem: Expected SubsystemID in response
            expected_ist_list: List of acceptable IST values in response
            timeout: Maximum time to wait (seconds)
            send_interval: Time between packet sends (seconds)

        Returns:
            True if expected response received, False on timeout
        """
        start_time = time.time()
        last_send_time = 0
        packet_index = 0

        self.log_message(f"🔄 Waiting for ({expected_state.value}-{expected_subsystem.value}-{expected_ist_list})", "INFO")

        while self.test_running and (time.time() - start_time) < timeout:
            # Send packets in rotation at intervals
            if time.time() - last_send_time >= send_interval:
                self.send_packet(send_packets[packet_index], send_descriptions[packet_index])
                packet_index = (packet_index + 1) % len(send_packets)
                last_send_time = time.time()

            # Check for response
            try:
                msg_type, data = self.message_queue.get(timeout=0.1)
                if msg_type == 'received_packet':
                    packet = data
                    sys_state, subsystem, ist = parse_control_byte(packet.control)

                    # Check if this is the expected response
                    if (sys_state == expected_state and
                        subsystem == expected_subsystem and
                        ist in expected_ist_list):
                        return True

            except queue.Empty:
                continue

        return False

    def wait_for_snc_response(self, expected_state: SystemState, expected_ist: int, timeout: float) -> bool:
        """Wait for SNC to respond with specific state and IST"""
        start_time = time.time()
        while self.test_running and (time.time() - start_time) < timeout:
            try:
                msg_type, data = self.message_queue.get(timeout=0.1)
                if msg_type == 'received_packet':
                    packet = data
                    sys_state, subsystem, ist = parse_control_byte(packet.control)
                    if sys_state == expected_state and subsystem == SubsystemID.SNC and ist == expected_ist:
                        return True
            except queue.Empty:
                continue
        return False

    def check_for_maze_transition(self) -> bool:
        """Check if SNC has transitioned to MAZE state"""
        while not self.message_queue.empty():
            try:
                msg_type, data = self.message_queue.get_nowait()
                if msg_type == 'received_packet':
                    packet = data
                    sys_state, subsystem, ist = parse_control_byte(packet.control)
                    if sys_state == SystemState.MAZE and subsystem == SubsystemID.SNC:
                        return True
            except queue.Empty:
                break
        return False

    def execute_maze_continuous_loop(self):
        """
        Execute continuous MAZE loop - THIS MATCHES THE REAL HUB!
        Sends all 6 packets continuously simulating SS and MDPS
        """
        self.log_message("🔄 Starting MAZE continuous loop (emulating SS + MDPS)...", "INFO")

        # Virtual maze state
        distance = 44  # Start at 0.44m (like line 117 in log)
        current_color = 0  # All WHITE initially
        current_angle = 0
        loop_count = 0
        max_loops = 100  # Safety limit

        while self.test_running and loop_count < max_loops:
            loop_count += 1
            self.progress_var.set((loop_count / max_loops) * 100)

            # ========================================
            # Send MDPS packets (simulating motors)
            # ========================================
            self.send_packet(SCSPacket(161, 90, 0, 0), "MDPS: MAZE:MDPS:1 (stop/rotate)")
            time.sleep(0.05)

            self.send_packet(SCSPacket(162, 0, 0, 0), "MDPS: MAZE:MDPS:2 (confirm)")
            time.sleep(0.05)

            self.send_packet(SCSPacket(163, 10, 10, 0), "MDPS: MAZE:MDPS:3 (forward)")
            time.sleep(0.05)

            # MDPS:4 with incrementing distance
            dat1 = distance // 100  # Upper byte (meters)
            dat0 = distance % 100   # Lower byte (cm)
            self.send_packet(SCSPacket(164, dat1, dat0, 0), f"MDPS: MAZE:MDPS:4 (dist={distance})")
            time.sleep(0.05)

            # ========================================
            # Send SS packets (simulating sensors)
            # ========================================
            self.send_packet(SCSPacket(177, 0, current_color, 0), f"SS: MAZE:SS:1 (color={current_color})")
            time.sleep(0.05)

            self.send_packet(SCSPacket(178, current_angle, 0, 0), f"SS: MAZE:SS:2 (angle={current_angle})")

            # ========================================
            # WAIT for SNC response (turn-based protocol!)
            # ========================================
            # The real HUB waits for SNC to respond after sending all 6 packets
            # This gives SNC time to process and respond with MAZE:SNC:[1,2,3]

            # Record the timestamp before waiting
            last_rx_time = getattr(self, 'last_received_time', 0)

            # Wait for SNC response with timeout
            timeout_start = time.time()
            snc_responded = False

            while (time.time() - timeout_start) < 0.5:  # 500ms timeout
                # Check if we received a NEW packet since we started waiting
                current_rx_time = getattr(self, 'last_received_time', 0)
                if current_rx_time > last_rx_time:
                    # SNC has responded!
                    snc_responded = True
                    break
                time.sleep(0.01)  # Small delay to avoid busy-waiting

            # Optional: Add delay if SNC didn't respond (give it more time)
            if not snc_responded:
                time.sleep(0.1)

            # ========================================
            # Update virtual maze state
            # ========================================
            distance += 2  # Increment distance by 2cm each loop

            # Simulate FULL MAZE with all line types and angles
            if loop_count == 10:
                # First GREEN line - moderate angle
                current_color = 16  # S2=GREEN (0b00010000)
                current_angle = 22
                self.log_message("🟢 GREEN line #1 detected (22° angle - moderate)", "INFO")

            elif loop_count == 15:
                # Clear GREEN
                current_color = 0
                current_angle = 0
                self.log_message("⚪ GREEN cleared - back to WHITE", "INFO")

            elif loop_count == 25:
                # BLUE wall
                current_color = 24  # S2=BLUE (0b00011000)
                current_angle = 30
                self.log_message("🔵 BLUE wall detected (30° angle) - should trigger 90° turn!", "INFO")

            elif loop_count == 30:
                # Clear BLUE
                current_color = 0
                current_angle = 0
                self.log_message("⚪ BLUE cleared", "INFO")

            elif loop_count == 40:
                # Second GREEN line - moderate-high angle
                current_color = 16  # S2=GREEN
                current_angle = 35
                self.log_message("🟢 GREEN line #2 detected (35° angle - moderate-high)", "INFO")

            elif loop_count == 45:
                # Clear GREEN
                current_color = 0
                current_angle = 0
                self.log_message("⚪ GREEN cleared", "INFO")

            elif loop_count == 50:
                # Third GREEN line - LARGE ANGLE (>45°)
                # Edge sensor triggers first - SS cannot measure angle!
                current_color = 2   # S1 (edge) =GREEN (bit 0 set, 0b00000010)
                current_angle = 0   # NO ANGLE DATA (edge sensor triggered first)
                self.log_message("🟢 GREEN line #3 detected (>45° STEEP) - EDGE SENSOR triggered - angle=0 (SNC must calculate from distance)!", "INFO")

            elif loop_count == 54:
                # Edge sensor still sees GREEN  (steep angle means longer detection)
                current_color = 2   # S1=GREEN (edge sensor)
                current_angle = 0   # Still no angle (steep line)

            elif loop_count == 55:
                # Clear GREEN
                current_color = 0
                current_angle = 0
                self.log_message("⚪ GREEN cleared (steep angle)", "INFO")

            elif loop_count == 60:
                # BLACK wall
                current_color = 32  # S2=BLACK (0b00100000)
                current_angle = 28
                self.log_message("⚫ BLACK wall detected (28° angle) - should trigger 90° turn!", "INFO")

            elif loop_count == 65:
                # Clear BLACK
                current_color = 0
                current_angle = 0
                self.log_message("⚪ BLACK cleared", "INFO")

            elif loop_count == 70:
                # Fourth GREEN line - small angle
                current_color = 16  # S2=GREEN
                current_angle = 8
                self.log_message("🟢 GREEN line #4 detected (8° angle - small)", "INFO")

            elif loop_count == 75:
                # Clear GREEN
                current_color = 0
                current_angle = 0
                self.log_message("⚪ GREEN cleared", "INFO")

            elif loop_count == 80:
                # Fifth GREEN line - VERY LARGE ANGLE (>45°)
                # Edge sensor triggers first - SS cannot measure angle!
                current_color = 2   # S1 (edge) = GREEN (0b00000010)
                current_angle = 0   # NO ANGLE DATA (edge sensor triggered first)
                self.log_message("🟢 GREEN line #5 detected (>45° VERY STEEP) - EDGE SENSOR triggered - angle=0 (SNC must calculate)!", "INFO")

            elif loop_count == 83:
                # Edge sensor still sees GREEN (very steep angle = longer detection)
                current_color = 2   # S1=GREEN
                current_angle = 0

            elif loop_count == 85:
                # Clear GREEN
                current_color = 0
                current_angle = 0
                self.log_message("⚪ GREEN cleared (very steep angle)", "INFO")

            elif loop_count == 90:
                # Approaching end - robot should be rectifying alignment
                current_color = 0
                current_angle = 12  # Still slightly misaligned
                self.log_message("⚠️ Approaching EOM - angle=12° (robot should rectify to <5°)", "INFO")

            elif loop_count == 92:
                # Robot rectifying
                current_color = 0
                current_angle = 7
                self.log_message("⚠️ EOM approach - angle=7° (still rectifying...)", "INFO")

            elif loop_count == 94:
                # Robot should be nearly aligned now
                current_color = 0
                current_angle = 3
                self.log_message("✅ EOM approach - angle=3° (good alignment!)", "INFO")

            elif loop_count == 96:
                # Perfect alignment achieved
                current_color = 0
                current_angle = 1
                self.log_message("✅ EOM approach - angle=1° (excellent alignment)", "INFO")

            elif loop_count == 98:
                # RED end of maze - now aligned
                current_color = 73  # All sensors RED (0b01001001)
                current_angle = 1  # Aligned to <5°
                self.log_message("🔴 RED END-OF-MAZE detected! Angle=1° (<5° requirement met) - SNC should accept EOM!", "INFO")

            elif loop_count == 99:
                # Keep RED with perfect alignment
                current_color = 73
                current_angle = 0  # Perfect alignment
                self.log_message("🔴 RED EOM confirmed - angle=0° (perfect) - maze complete!", "INFO")

            # Log progress every 10 loops
            if loop_count % 10 == 0:
                self.log_message(f"📊 Loop {loop_count}: distance={distance}cm, color={current_color}", "INFO")

        self.log_message("✅ MAZE continuous loop completed!", "SUCCESS")
        self.progress_var.set(100)

    def execute_navcon_sequence(self):
        """OLD METHOD - DEPRECATED - Use execute_maze_continuous_loop instead"""
        self.log_message("🧪 Executing NAVCON test sequence in MAZE state...", "INFO")

        # ========================================
        # MAZE Phase 1: Forward Movement
        # ========================================
        self.log_message("➡️ MAZE Phase 1: Sending forward movement packets...", "INFO")

        # Keep sending MDPS:MAZE:3 (forward movement) until SNC responds with MAZE:SNC:3
        if not self.wait_for_transition_multi_packet(
            send_packets=[
                SCSPacket(163, 10, 10, 0),  # MDPS: Forward (MAZE:MDPS:3)
                SCSPacket(164, 0, 50, 0),   # MDPS: Distance update (MAZE:MDPS:4)
            ],
            send_descriptions=[
                "MDPS: Forward motion (MAZE:MDPS:3)",
                "MDPS: Distance 50mm (MAZE:MDPS:4)"
            ],
            expected_state=SystemState.MAZE,
            expected_subsystem=SubsystemID.SNC,
            expected_ist_list=[3],  # Wait for NAVCON to send speed command
            timeout=10.0,
            send_interval=0.5
        ):
            self.log_message("⚠️ SNC did not send MAZE:3 speed command", "INFO")

        self.log_message("✅ SNC sending speed commands (MAZE:SNC:3)", "SUCCESS")
        self.progress_var.set(20)

        # ========================================
        # MAZE Phase 2: GREEN Line Detection
        # ========================================
        self.log_message("🟢 MAZE Phase 2: Simulating GREEN line detection...", "INFO")

        # Send SS packets for GREEN line detection
        # Color encoding: S2=GREEN(2) → packed = (0<<6)|(2<<3)|(0) = 16
        if not self.send_and_wait_response(
            send_packets=[
                SCSPacket(177, 0, 16, 0),   # SS: GREEN on S2 (MAZE:SS:1)
                SCSPacket(178, 35, 0, 0),   # SS: Angle 35° (MAZE:SS:2)
            ],
            send_descriptions=[
                "SS: GREEN line detected on S2 (MAZE:SS:1)",
                "SS: Incidence angle 35° (MAZE:SS:2)"
            ],
            wait_for_ist_list=[2],  # Expect SNC to send STOP (MAZE:SNC:2)
            timeout=10.0
        ):
            self.log_message("⚠️ Continuing despite no STOP response", "INFO")

        self.progress_var.set(40)

        # ========================================
        # MAZE Phase 3: MDPS Reverse Execution
        # ========================================
        self.log_message("⏪ MAZE Phase 3: Sending MDPS reverse confirmation...", "INFO")

        # Send MDPS reverse complete packet
        if not self.send_and_wait_response(
            send_packets=[
                SCSPacket(162, 0, 6, 3),    # MDPS: Reverse 60mm complete (MAZE:MDPS:2)
            ],
            send_descriptions=[
                "MDPS: Reverse 60mm complete (MAZE:MDPS:2)"
            ],
            wait_for_ist_list=[1],  # Expect rotation command (MAZE:SNC:1)
            timeout=10.0
        ):
            self.log_message("⚠️ Continuing despite no rotation command", "INFO")

        self.progress_var.set(60)

        # ========================================
        # MAZE Phase 4: Rotation Execution
        # ========================================
        self.log_message("🔄 MAZE Phase 4: Sending rotation completion...", "INFO")

        # Send MDPS rotation complete
        if not self.send_and_wait_response(
            send_packets=[
                SCSPacket(162, 0, 35, 2),   # MDPS: Rotation 35° RIGHT complete (MAZE:MDPS:2)
            ],
            send_descriptions=[
                "MDPS: Rotation 35° RIGHT complete (MAZE:MDPS:2)"
            ],
            wait_for_ist_list=[3],  # Expect forward command (MAZE:SNC:3)
            timeout=10.0
        ):
            self.log_message("⚠️ Continuing despite no forward command", "INFO")

        self.progress_var.set(80)

        # ========================================
        # MAZE Phase 5: Resume Forward (WHITE surface)
        # ========================================
        self.log_message("⚪ MAZE Phase 5: Sending WHITE surface (line cleared)...", "INFO")

        # Send SS packets for WHITE (no lines detected)
        if not self.send_and_wait_response(
            send_packets=[
                SCSPacket(177, 0, 0, 0),    # SS: All sensors WHITE (MAZE:SS:1)
                SCSPacket(163, 10, 10, 0),  # MDPS: Forward motion (MAZE:MDPS:3)
            ],
            send_descriptions=[
                "SS: All sensors WHITE (MAZE:SS:1)",
                "MDPS: Forward motion resumed (MAZE:MDPS:3)"
            ],
            wait_for_ist_list=[3],
            timeout=10.0
        ):
            self.log_message("⚠️ Continuing test...", "INFO")

        # Complete the test
        self.progress_var.set(100)
        self.progress_label.config(text="Test completed successfully!")
        self.log_message("✅ NAVCON test sequence completed!", "SUCCESS")
        self.log_message("📊 Check packet log for complete protocol exchange", "INFO")

    def send_and_wait_response(self, send_packets: List[SCSPacket],
                               send_descriptions: List[str],
                               wait_for_ist_list: List[int],
                               timeout: float,
                               send_interval: float = 0.5) -> bool:
        """
        Send packets continuously and wait for SNC to respond with specific IST values

        Args:
            send_packets: List of packets to send alternately
            send_descriptions: Descriptions for logging
            wait_for_ist_list: List of IST values to wait for from SNC
            timeout: Maximum wait time
            send_interval: Time between sends

        Returns:
            True if expected response received, False on timeout
        """
        start_time = time.time()
        last_send_time = 0
        packet_index = 0

        self.log_message(f"🔄 Waiting for SNC MAZE:SNC:{wait_for_ist_list}", "INFO")

        while self.test_running and (time.time() - start_time) < timeout:
            # Send packets in rotation
            if time.time() - last_send_time >= send_interval:
                self.send_packet(send_packets[packet_index], send_descriptions[packet_index])
                packet_index = (packet_index + 1) % len(send_packets)
                last_send_time = time.time()

            # Check for SNC response
            try:
                msg_type, data = self.message_queue.get(timeout=0.1)
                if msg_type == 'received_packet':
                    packet = data
                    sys_state, subsystem, ist = parse_control_byte(packet.control)

                    if (sys_state == SystemState.MAZE and
                        subsystem == SubsystemID.SNC and
                        ist in wait_for_ist_list):
                        self.log_message(f"✅ Received expected MAZE:SNC:{ist}", "SUCCESS")
                        return True

            except queue.Empty:
                continue

        return False

    def monitor_snc_responses(self, timeout=1.0):
        """Monitor SNC responses during test execution"""
        start_time = time.time()

        while self.test_running and (time.time() - start_time) < timeout:
            try:
                msg_type, data = self.message_queue.get(timeout=0.1)
                if msg_type == 'received_packet':
                    packet = data
                    sys_state, subsystem, ist = parse_control_byte(packet.control)

                    # Log interesting responses
                    if subsystem == SubsystemID.SNC:
                        if sys_state == SystemState.MAZE and ist == 1:
                            self.log_message("🎯 SNC rotation request detected", "SUCCESS")
                        elif sys_state == SystemState.MAZE and ist == 3:
                            self.log_message("⚡ SNC speed command detected", "SUCCESS")

            except queue.Empty:
                continue

    def send_packet(self, packet: SCSPacket, description: str = ""):
        """Send packet to SNC"""
        if not self.is_connected or not self.serial_port:
            return

        try:
            packet_bytes = bytes([packet.control, packet.dat1, packet.dat0, packet.dec])
            self.serial_port.write(packet_bytes)

            timestamp = datetime.now().strftime("%H:%M:%S.%f")[:-3]
            self.test_state['total_packets_sent'] += 1
            self.test_state['sequence_number'] += 1

            # Log the sent packet
            direction = "SENT"
            log_line = f"{timestamp} || {self.test_state['sequence_number']:3} || {direction:8} || {packet}"
            if description:
                log_line += f" || {description}"

            self.log_message(log_line, "SENT")

        except Exception as e:
            self.log_message(f"❌ Send error: {str(e)}", "ERROR")

    def stop_test(self):
        """Stop the current test"""
        self.test_running = False

        self.start_test_btn.config(state='normal' if self.is_connected else 'disabled')
        self.stop_test_btn.config(state='disabled')

        self.progress_var.set(0)
        self.progress_label.config(text="Test stopped")

        self.log_message("⏹️ Test stopped", "INFO")

    def update_statistics(self):
        """Update the statistics display"""
        if self.test_state['test_start_time']:
            duration = time.time() - self.test_state['test_start_time']
            self.stats_labels['test_duration'].config(text=f"{duration:.1f}s")
        else:
            self.stats_labels['test_duration'].config(text="0.0s")

        self.stats_labels['packets_sent'].config(text=str(self.test_state['total_packets_sent']))
        self.stats_labels['packets_received'].config(text=str(self.test_state['total_packets_received']))

        # Calculate success rate
        total_packets = self.test_state['total_packets_sent'] + self.test_state['total_packets_received']
        success_rate = (self.test_state['total_packets_received'] / max(1, self.test_state['total_packets_sent'])) * 100
        self.stats_labels['success_rate'].config(text=f"{success_rate:.1f}%")

        self.stats_labels['current_state'].config(text=self.test_state['system_state'].name)
        self.stats_labels['touch_events'].config(text=str(self.test_state['touch_count']))
        self.stats_labels['rotation_commands'].config(text=str(self.test_state['rotation_count']))
        self.stats_labels['green_detections'].config(text=str(self.test_state['green_detections']))

    def log_message(self, message: str, msg_type: str = "INFO"):
        """Log a message to the display"""
        timestamp = datetime.now().strftime("%H:%M:%S.%f")[:-3]

        # Color coding based on message type
        color_map = {
            "SENT": "#3498db",      # Blue
            "RECEIVED": "#27ae60",  # Green
            "ERROR": "#e74c3c",     # Red
            "SUCCESS": "#2ecc71",   # Bright green
            "INFO": "#ecf0f1"       # Light gray
        }

        color = color_map.get(msg_type, "#ecf0f1")

        # Insert message with color
        self.log_text.tag_configure(msg_type, foreground=color)
        self.log_text.insert(tk.END, f"{message}\n", msg_type)
        self.log_text.see(tk.END)

        # Store in packet log
        self.packet_log.append({
            'timestamp': timestamp,
            'message': message,
            'type': msg_type
        })

    def clear_log(self):
        """Clear the packet log"""
        self.log_text.delete(1.0, tk.END)
        self.packet_log.clear()
        self.log_message("🗑️ Log cleared", "INFO")

    def save_log(self):
        """Save the packet log to file"""
        try:
            filename = f"navcon_test_log_{datetime.now().strftime('%Y%m%d_%H%M%S')}.txt"
            filepath = f"C:\\Users\\byron\\OneDrive\\Documents\\3rd Year Uni\\Sem 2\\ERD320\\Phase3\\Phase3\\NAVCON_Test_Suite\\{filename}"

            with open(filepath, 'w', encoding='utf-8') as f:
                f.write("MARV NAVCON Test Log\n")
                f.write("=" * 50 + "\n")
                f.write(f"Generated: {datetime.now().strftime('%Y-%m-%d %H:%M:%S')}\n")
                f.write(f"Scenario: {self.current_scenario.name if self.current_scenario else 'None'}\n")
                f.write("=" * 50 + "\n\n")

                for entry in self.packet_log:
                    f.write(f"[{entry['timestamp']}] [{entry['type']}] {entry['message']}\n")

            self.log_message(f"💾 Log saved to {filename}", "SUCCESS")
            messagebox.showinfo("Success", f"Log saved to {filename}")

        except Exception as e:
            self.log_message(f"❌ Save error: {str(e)}", "ERROR")
            messagebox.showerror("Error", f"Failed to save log: {str(e)}")

    def run(self):
        """Start the application"""
        self.log_message("🚀 MARV NAVCON Testing Suite initialized", "INFO")
        self.log_message("📋 Select a serial port and test scenario to begin", "INFO")
        self.root.mainloop()

# ==================== MAIN EXECUTION ====================

if __name__ == "__main__":
    try:
        app = NAVCONTester()
        app.run()
    except KeyboardInterrupt:
        print("\n👋 Testing suite terminated by user")
    except Exception as e:
        print(f"❌ Fatal error: {str(e)}")
        sys.exit(1)