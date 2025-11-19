#!/usr/bin/env python3
"""
NAVCON GUI Testing Suite
========================
User-friendly GUI application for testing NAVCON Arduino implementation.
Features port selection, clickable test buttons, and real-time results.
"""

import tkinter as tk
from tkinter import ttk, scrolledtext, messagebox
import serial
import serial.tools.list_ports
import time
import struct
import threading
from enum import IntEnum
from dataclasses import dataclass
from typing import Optional, List, Tuple
import queue


class SystemState(IntEnum):
    SYS_IDLE = 0
    SYS_CAL = 1
    SYS_MAZE = 2
    SYS_SOS = 3


class SubsystemID(IntEnum):
    SUB_HUB = 0
    SUB_SNC = 1
    SUB_MDPS = 2
    SUB_SS = 3


class ColorType(IntEnum):
    WHITE = 0
    RED = 1
    GREEN = 2
    BLUE = 3
    BLACK = 4


class NAVCONAction(IntEnum):
    ACT_STOP = 0
    ACT_FORWARD = 1
    ACT_BACKWARD = 2
    ACT_TURN_LEFT = 3
    ACT_TURN_RIGHT = 4
    ACT_TURN_180_LEFT = 5
    ACT_TURN_180_RIGHT = 6
    ACT_TURN_360_LEFT = 7
    ACT_DIFFERENTIAL_STEER = 8
    ACT_WALL_FOLLOW_TURN = 9
    ACT_STEERING_CORRECTION = 10
    ACT_ERROR_STOP = 11


@dataclass
class SCSPacket:
    control: int
    dat1: int
    dat0: int
    dec: int

    @property
    def system_state(self) -> SystemState:
        return SystemState((self.control >> 6) & 0x03)

    @property
    def subsystem_id(self) -> SubsystemID:
        return SubsystemID((self.control >> 4) & 0x03)

    @property
    def internal_state(self) -> int:
        return self.control & 0x0F

    def __str__(self):
        return f"SCS[ctrl=0x{self.control:02X}, dat1={self.dat1}, dat0={self.dat0}, dec={self.dec}]"


@dataclass
class TestScenario:
    name: str
    description: str
    setup_packets: List[SCSPacket]
    trigger_packet: SCSPacket
    expected_action: NAVCONAction
    expected_data: Optional[Tuple[int, int, int]] = None


class NAVCONGUITester:
    def __init__(self):
        self.root = tk.Tk()
        self.root.title("NAVCON Arduino Testing Suite")
        self.root.geometry("1000x700")
        self.root.resizable(True, True)

        # Serial connection
        self.serial_conn = None
        self.is_connected = False

        # Threading
        self.test_queue = queue.Queue()
        self.result_queue = queue.Queue()

        # Test scenarios
        self.test_scenarios = []
        self.create_test_scenarios()

        # GUI setup
        self.setup_gui()
        self.refresh_ports()

        # Start background worker
        self.root.after(100, self.check_results)

    def setup_gui(self):
        """Create the GUI layout"""
        # Main frame
        main_frame = ttk.Frame(self.root, padding="10")
        main_frame.grid(row=0, column=0, sticky=(tk.W, tk.E, tk.N, tk.S))

        # Configure grid weights
        self.root.columnconfigure(0, weight=1)
        self.root.rowconfigure(0, weight=1)
        main_frame.columnconfigure(1, weight=1)
        main_frame.rowconfigure(2, weight=1)

        # Connection Frame
        conn_frame = ttk.LabelFrame(main_frame, text="🔌 Connection", padding="10")
        conn_frame.grid(row=0, column=0, columnspan=2, sticky=(tk.W, tk.E), pady=(0, 10))
        conn_frame.columnconfigure(1, weight=1)

        # Port selection
        ttk.Label(conn_frame, text="Serial Port:").grid(row=0, column=0, sticky=tk.W, padx=(0, 5))
        self.port_var = tk.StringVar()
        self.port_combo = ttk.Combobox(conn_frame, textvariable=self.port_var, state="readonly", width=20)
        self.port_combo.grid(row=0, column=1, sticky=(tk.W, tk.E), padx=(0, 5))

        self.refresh_btn = ttk.Button(conn_frame, text="🔄 Refresh", command=self.refresh_ports)
        self.refresh_btn.grid(row=0, column=2, padx=(5, 0))

        # Baud rate
        ttk.Label(conn_frame, text="Baud Rate:").grid(row=0, column=3, sticky=tk.W, padx=(20, 5))
        self.baud_var = tk.StringVar(value="115200")
        baud_combo = ttk.Combobox(conn_frame, textvariable=self.baud_var, width=10,
                                  values=["9600", "57600", "115200", "230400"])
        baud_combo.grid(row=0, column=4, padx=(0, 5))

        # Connect button
        self.connect_btn = ttk.Button(conn_frame, text="🔌 Connect", command=self.toggle_connection)
        self.connect_btn.grid(row=0, column=5, padx=(20, 0))

        # Status label
        self.status_var = tk.StringVar(value="❌ Disconnected")
        self.status_label = ttk.Label(conn_frame, textvariable=self.status_var, foreground="red")
        self.status_label.grid(row=1, column=0, columnspan=6, sticky=tk.W, pady=(5, 0))

        # Test Control Frame
        test_frame = ttk.LabelFrame(main_frame, text="🧪 Test Scenarios", padding="10")
        test_frame.grid(row=1, column=0, sticky=(tk.W, tk.E, tk.N, tk.S), pady=(0, 10))

        # Individual test buttons
        for i, scenario in enumerate(self.test_scenarios):
            btn = ttk.Button(test_frame, text=f"{i + 1}. {scenario.name}",
                             command=lambda idx=i: self.run_single_test(idx),
                             width=25)
            btn.grid(row=i, column=0, sticky=(tk.W, tk.E), pady=2)

        # Bulk action buttons
        bulk_frame = ttk.Frame(test_frame)
        bulk_frame.grid(row=len(self.test_scenarios), column=0, sticky=(tk.W, tk.E), pady=(20, 0))

        self.run_all_btn = ttk.Button(bulk_frame, text="🚀 Run All Tests",
                                      command=self.run_all_tests, style="Accent.TButton")
        self.run_all_btn.pack(side=tk.LEFT, padx=(0, 10))

        self.clear_btn = ttk.Button(bulk_frame, text="🗑️ Clear Results", command=self.clear_results)
        self.clear_btn.pack(side=tk.LEFT, padx=(0, 10))

        self.reset_btn = ttk.Button(bulk_frame, text="🔄 Reset Arduino", command=self.reset_arduino)
        self.reset_btn.pack(side=tk.LEFT)

        # Results Frame
        results_frame = ttk.LabelFrame(main_frame, text="📊 Test Results", padding="10")
        results_frame.grid(row=2, column=0, columnspan=2, sticky=(tk.W, tk.E, tk.N, tk.S))
        results_frame.columnconfigure(0, weight=1)
        results_frame.rowconfigure(0, weight=1)

        # Results text area with scrollbar
        self.results_text = scrolledtext.ScrolledText(results_frame, wrap=tk.WORD,
                                                      font=("Consolas", 10), state=tk.DISABLED)
        self.results_text.grid(row=0, column=0, sticky=(tk.W, tk.E, tk.N, tk.S))

        # Configure text tags for colored output
        self.results_text.tag_configure("header", foreground="blue", font=("Consolas", 12, "bold"))
        self.results_text.tag_configure("pass", foreground="green", font=("Consolas", 10, "bold"))
        self.results_text.tag_configure("fail", foreground="red", font=("Consolas", 10, "bold"))
        self.results_text.tag_configure("info", foreground="blue")
        self.results_text.tag_configure("warning", foreground="orange")
        self.results_text.tag_configure("sent", foreground="purple")
        self.results_text.tag_configure("received", foreground="darkgreen")

        # Progress Frame
        progress_frame = ttk.Frame(main_frame)
        progress_frame.grid(row=3, column=0, columnspan=2, sticky=(tk.W, tk.E), pady=(10, 0))
        progress_frame.columnconfigure(0, weight=1)

        self.progress_var = tk.StringVar(value="Ready")
        self.progress_label = ttk.Label(progress_frame, textvariable=self.progress_var)
        self.progress_label.grid(row=0, column=0, sticky=tk.W)

        self.progress = ttk.Progressbar(progress_frame, mode='determinate')
        self.progress.grid(row=1, column=0, sticky=(tk.W, tk.E), pady=(5, 0))

    def refresh_ports(self):
        """Refresh available serial ports"""
        ports = [port.device for port in serial.tools.list_ports.comports()]
        self.port_combo['values'] = ports
        if ports and not self.port_var.get():
            self.port_var.set(ports[0])

    def toggle_connection(self):
        """Connect or disconnect from Arduino"""
        if not self.is_connected:
            self.connect_to_arduino()
        else:
            self.disconnect_from_arduino()

    def connect_to_arduino(self):
        """Connect to the selected Arduino port"""
        port = self.port_var.get()
        baud = int(self.baud_var.get())

        if not port:
            messagebox.showerror("Error", "Please select a serial port")
            return

        try:
            self.serial_conn = serial.Serial(port, baud, timeout=1)
            time.sleep(2)  # Arduino reset delay

            self.is_connected = True
            self.status_var.set(f"✅ Connected to {port} @ {baud} baud")
            self.status_label.configure(foreground="green")
            self.connect_btn.configure(text="🔌 Disconnect")
            self.log_message("🔌 Connected to Arduino successfully!", "info")

        except Exception as e:
            messagebox.showerror("Connection Error", f"Failed to connect to {port}:\n{str(e)}")

    def disconnect_from_arduino(self):
        """Disconnect from Arduino"""
        if self.serial_conn:
            self.serial_conn.close()
            self.serial_conn = None

        self.is_connected = False
        self.status_var.set("❌ Disconnected")
        self.status_label.configure(foreground="red")
        self.connect_btn.configure(text="🔌 Connect")
        self.log_message("🔌 Disconnected from Arduino", "warning")

    def log_message(self, message: str, tag: str = "info"):
        """Add message to results display"""
        self.results_text.configure(state=tk.NORMAL)
        timestamp = time.strftime("%H:%M:%S")
        self.results_text.insert(tk.END, f"[{timestamp}] {message}\n", tag)
        self.results_text.configure(state=tk.DISABLED)
        self.results_text.see(tk.END)

    def clear_results(self):
        """Clear the results display"""
        self.results_text.configure(state=tk.NORMAL)
        self.results_text.delete(1.0, tk.END)
        self.results_text.configure(state=tk.DISABLED)
        self.progress['value'] = 0
        self.progress_var.set("Ready")

    def reset_arduino(self):
        """Send reset command to Arduino"""
        if not self.is_connected:
            messagebox.showwarning("Warning", "Not connected to Arduino")
            return

        try:
            self.serial_conn.write(b'R')
            self.log_message("🔄 Reset command sent to Arduino", "info")
        except Exception as e:
            self.log_message(f"❌ Failed to send reset: {e}", "fail")

    def make_control_byte(self, sys_state: SystemState, subsystem: SubsystemID, internal_state: int) -> int:
        """Create control byte from components"""
        return ((sys_state & 0x03) << 6) | ((subsystem & 0x03) << 4) | (internal_state & 0x0F)

    def create_color_packet(self, s1: ColorType, s2: ColorType, s3: ColorType) -> SCSPacket:
        """Create SS color detection packet"""
        # Color encoding: S1 in bits 6-8, S2 in bits 3-5, S3 in bits 0-2
        color_data = (s1 << 6) | (s2 << 3) | s3
        control = self.make_control_byte(SystemState.SYS_MAZE, SubsystemID.SUB_SS, 1)

        # Debug: Log what we're encoding
        print(f"🔍 Color encoding: S1={s1.name}({s1}) S2={s2.name}({s2}) S3={s3.name}({s3}) → raw=0x{color_data:04X}")

        return SCSPacket(control, (color_data >> 8) & 0xFF, color_data & 0xFF, 0)

    def create_angle_packet(self, angle: int) -> SCSPacket:
        """Create SS angle packet"""
        control = self.make_control_byte(SystemState.SYS_MAZE, SubsystemID.SUB_SS, 2)
        angle_byte = angle if angle >= 0 else (256 + angle)
        return SCSPacket(control, angle_byte, 0, 0)

    def create_calibration_packet(self, vL: int, vR: int) -> SCSPacket:
        """Create MDPS calibration packet"""
        control = self.make_control_byte(SystemState.SYS_CAL, SubsystemID.SUB_MDPS, 0)
        return SCSPacket(control, vR, vL, 0)

    def send_packet(self, packet: SCSPacket) -> bool:
        """Send SCS packet to Arduino"""
        if not self.is_connected:
            return False

        try:
            data = bytes([packet.control, packet.dat1, packet.dat0, packet.dec])
            self.serial_conn.write(data)
            self.serial_conn.flush()
            return True
        except Exception as e:
            self.log_message(f"❌ Send error: {e}", "fail")
            return False

    def read_packet(self, timeout: float = 3.0) -> Optional[SCSPacket]:
        """Read SCS packet from Arduino with timeout - looks for NAVCON response"""
        if not self.is_connected:
            return None

        start_time = time.time()
        attempts = 0
        max_attempts = 10  # Try multiple packets if needed

        while time.time() - start_time < timeout and attempts < max_attempts:
            if self.serial_conn.in_waiting >= 4:
                try:
                    # Clear any partial data first
                    if self.serial_conn.in_waiting % 4 != 0:
                        self.serial_conn.read(self.serial_conn.in_waiting % 4)

                    data = self.serial_conn.read(4)
                    if len(data) == 4:
                        control, dat1, dat0, dec = struct.unpack('BBBB', data)
                        packet = SCSPacket(control, dat1, dat0, dec)
                        attempts += 1

                        # Log all received packets for debugging
                        self.log_message(f"🔍 Debug: Raw packet ctrl=0x{control:02X} dat1={dat1} dat0={dat0} dec={dec}",
                                         "info")

                        # Check if this is a NAVCON response (SUB_SNC, IST=3)
                        if packet.subsystem_id == SubsystemID.SUB_SNC and packet.internal_state == 3:
                            return packet

                        # If it's a startup packet (IST=1 or IST=2), keep reading for the real response
                        if packet.subsystem_id == SubsystemID.SUB_SNC and (
                                packet.internal_state == 1 or packet.internal_state == 2):
                            continue  # Skip startup packets, wait for the real NAVCON response

                        # For any other packet type, return it
                        return packet

                except Exception as e:
                    self.log_message(f"❌ Read error: {e}", "fail")
                    attempts += 1
            time.sleep(0.01)

        self.log_message(f"⚠️ Timeout after {attempts} attempts", "warning")
        return None

    def decode_navcon_response(self, packet: SCSPacket) -> Tuple[NAVCONAction, str]:
        """Decode NAVCON response packet"""
        if packet.subsystem_id != SubsystemID.SUB_SNC or packet.internal_state != 3:
            return NAVCONAction.ACT_ERROR_STOP, "Invalid NAVCON packet format"

        if packet.dec == 0:
            if packet.dat1 == 0 and packet.dat0 == 0:
                return NAVCONAction.ACT_STOP, "Stop"
            else:
                return NAVCONAction.ACT_FORWARD, f"Forward L={packet.dat0} R={packet.dat1}"
        elif packet.dec == 1:
            return NAVCONAction.ACT_BACKWARD, f"Backward L={packet.dat0} R={packet.dat1}"
        elif packet.dec == 2:
            rotation = (packet.dat1 << 8) | packet.dat0
            if rotation == 180:
                return NAVCONAction.ACT_TURN_180_LEFT, "180° Left Turn"
            elif rotation == 360:
                return NAVCONAction.ACT_TURN_360_LEFT, "360° Left Turn"
            else:
                # Small angles (≤ 10°) are likely steering corrections
                if rotation <= 10:
                    return NAVCONAction.ACT_STEERING_CORRECTION, f"Steering Correction {rotation}° Left"
                else:
                    return NAVCONAction.ACT_TURN_LEFT, f"{rotation}° Left Turn"
        elif packet.dec == 3:
            rotation = (packet.dat1 << 8) | packet.dat0
            if rotation == 180:
                return NAVCONAction.ACT_TURN_180_RIGHT, "180° Right Turn"
            else:
                # Small angles (≤ 10°) are likely steering corrections
                if rotation <= 10:
                    return NAVCONAction.ACT_STEERING_CORRECTION, f"Steering Correction {rotation}° Right"
                else:
                    return NAVCONAction.ACT_TURN_RIGHT, f"{rotation}° Right Turn"
        else:
            return NAVCONAction.ACT_ERROR_STOP, f"Unknown dec={packet.dec}"

    def describe_sent_packet(self, packet: SCSPacket) -> str:
        """Describe what a sent packet represents"""
        if packet.subsystem_id == SubsystemID.SUB_SS:
            if packet.internal_state == 1:
                color_data = (packet.dat1 << 8) | packet.dat0
                s1 = ColorType((color_data >> 6) & 0x07)
                s2 = ColorType((color_data >> 3) & 0x07)
                s3 = ColorType(color_data & 0x07)
                return f"Colors: S1={s1.name} S2={s2.name} S3={s3.name}"
            elif packet.internal_state == 2:
                angle = packet.dat1 if packet.dat1 < 128 else packet.dat1 - 256
                return f"Angle: {angle}°"
        elif packet.subsystem_id == SubsystemID.SUB_MDPS and packet.internal_state == 0:
            return f"Calibration: vL={packet.dat0} vR={packet.dat1}"

        return f"{packet.system_state.name}:{packet.subsystem_id.name}:IST{packet.internal_state}"

    def run_single_test(self, test_index: int):
        """Run a single test scenario"""
        if not self.is_connected:
            messagebox.showwarning("Warning", "Please connect to Arduino first")
            return

        # Clear serial buffer before starting test
        self.serial_conn.reset_input_buffer()
        self.serial_conn.reset_output_buffer()
        time.sleep(0.1)  # Brief pause after clearing

        scenario = self.test_scenarios[test_index]
        self.log_message(f"\n🎯 Running Test: {scenario.name}", "header")
        self.log_message(f"📝 {scenario.description}", "info")

        # Send setup packets
        for i, packet in enumerate(scenario.setup_packets):
            self.log_message(f"📤 Setup {i + 1}: {packet} - {self.describe_sent_packet(packet)}", "sent")
            if not self.send_packet(packet):
                return
            time.sleep(0.2)  # Increased delay between packets

        # Send trigger packet
        self.log_message(f"📤 Trigger: {scenario.trigger_packet} - {self.describe_sent_packet(scenario.trigger_packet)}",
                         "sent")
        if not self.send_packet(scenario.trigger_packet):
            return

        # Wait a bit for Arduino to process and respond
        time.sleep(0.5)  # Give Arduino time to process and send response

        # Wait for response
        self.progress_var.set("Waiting for response...")
        response = self.read_packet(timeout=3.0)

        if response:
            action, description = self.decode_navcon_response(response)
            self.log_message(f"📥 Received: {response} - {description}", "received")

            if action == scenario.expected_action:
                self.log_message(f"✅ PASS: Expected {scenario.expected_action.name}, got {action.name}", "pass")
            else:
                self.log_message(f"❌ FAIL: Expected {scenario.expected_action.name}, got {action.name}", "fail")
        else:
            self.log_message(f"📥 No response received (timeout)", "fail")
            self.log_message(f"❌ FAIL: Expected {scenario.expected_action.name}, got no response", "fail")

        self.progress_var.set("Test complete")

        # Clear buffer again after test
        self.serial_conn.reset_input_buffer()
        time.sleep(0.2)  # Pause between tests

    def run_all_tests(self):
        """Run all test scenarios"""
        if not self.is_connected:
            messagebox.showwarning("Warning", "Please connect to Arduino first")
            return

        self.clear_results()
        self.log_message("🚀 Starting comprehensive NAVCON test suite...", "header")

        total_tests = len(self.test_scenarios)
        passed = 0

        for i, scenario in enumerate(self.test_scenarios):
            self.progress['value'] = (i / total_tests) * 100
            self.progress_var.set(f"Running test {i + 1}/{total_tests}: {scenario.name}")
            self.root.update()

            # Run the test
            start_time = time.time()
            self.run_single_test(i)

            # Brief pause between tests
            time.sleep(0.5)

        self.progress['value'] = 100
        self.progress_var.set("All tests complete!")

        # Show summary (would need to track results properly)
        self.log_message(f"\n📊 Test Summary: {total_tests} tests completed", "header")

    def create_test_scenarios(self):
        """Create comprehensive test scenarios"""
        cal_packet = self.create_calibration_packet(50, 50)

        self.test_scenarios = [
            TestScenario(
                name="Clear Path Navigation",
                description="All white sensors, straight ahead (0°)",
                setup_packets=[cal_packet, self.create_color_packet(ColorType.WHITE, ColorType.WHITE, ColorType.WHITE)],
                trigger_packet=self.create_angle_packet(0),
                expected_action=NAVCONAction.ACT_FORWARD
            ),
            TestScenario(
                name="Green Line Crossing",
                description="Green line detected at low angle (3°)",
                setup_packets=[cal_packet, self.create_color_packet(ColorType.WHITE, ColorType.GREEN, ColorType.WHITE)],
                trigger_packet=self.create_angle_packet(3),
                expected_action=NAVCONAction.ACT_FORWARD
            ),
            TestScenario(
                name="Red Line High Angle",
                description="Red line at high angle (50°) - clamped to 45° triggers steering correction",
                setup_packets=[cal_packet, self.create_color_packet(ColorType.WHITE, ColorType.RED, ColorType.WHITE)],
                trigger_packet=self.create_angle_packet(50),
                expected_action=NAVCONAction.ACT_STEERING_CORRECTION
            ),
            TestScenario(
                name="Red Line Moderate Angle",
                description="Red line at moderate angle (30°) - should reverse and re-approach",
                setup_packets=[cal_packet, self.create_color_packet(ColorType.WHITE, ColorType.RED, ColorType.WHITE)],
                trigger_packet=self.create_angle_packet(30),
                expected_action=NAVCONAction.ACT_BACKWARD
            ),
            TestScenario(
                name="Obstacle Wall Following",
                description="Black obstacle detected - initiate wall following",
                setup_packets=[cal_packet, self.create_color_packet(ColorType.WHITE, ColorType.BLACK, ColorType.WHITE)],
                trigger_packet=self.create_angle_packet(25),
                expected_action=NAVCONAction.ACT_BACKWARD
            ),
            TestScenario(
                name="Emergency 180° Turn",
                description="Obstacles on both sides - emergency maneuver",
                setup_packets=[cal_packet, self.create_color_packet(ColorType.BLACK, ColorType.BLACK, ColorType.BLACK)],
                trigger_packet=self.create_angle_packet(15),
                expected_action=NAVCONAction.ACT_TURN_180_RIGHT
            ),
            TestScenario(
                name="End of Maze Celebration",
                description="All red sensors - 360° victory spin",
                setup_packets=[cal_packet, self.create_color_packet(ColorType.RED, ColorType.RED, ColorType.RED)],
                trigger_packet=self.create_angle_packet(0),
                expected_action=NAVCONAction.ACT_TURN_360_LEFT
            ),
            TestScenario(
                name="Negative Angle Green Line",
                description="Green line at moderate negative angle (-40°) - reverse and re-approach",
                setup_packets=[cal_packet, self.create_color_packet(ColorType.WHITE, ColorType.GREEN, ColorType.WHITE)],
                trigger_packet=self.create_angle_packet(-40),
                expected_action=NAVCONAction.ACT_BACKWARD
            ),
            TestScenario(
                name="High Angle Green Line Steering",
                description="Green line at high negative angle (-45°) - steer toward line",
                setup_packets=[cal_packet, self.create_color_packet(ColorType.WHITE, ColorType.GREEN, ColorType.WHITE)],
                trigger_packet=self.create_angle_packet(-45),
                expected_action=NAVCONAction.ACT_STEERING_CORRECTION
            )
        ]

    def check_results(self):
        """Check for background task results"""
        try:
            while True:
                result = self.result_queue.get_nowait()
                # Process any background results here
        except queue.Empty:
            pass

        # Schedule next check
        self.root.after(100, self.check_results)

    def run(self):
        """Start the GUI application"""
        try:
            self.root.mainloop()
        finally:
            if self.is_connected:
                self.disconnect_from_arduino()


def main():
    app = NAVCONGUITester()
    app.run()


if __name__ == "__main__":
    main()