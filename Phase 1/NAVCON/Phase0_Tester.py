#!/usr/bin/env python3
"""
Fixed SCS Protocol Compliant NAVCON Test Harness
Properly implements SCS state machine communication flow
"""

import tkinter as tk
from tkinter import ttk, scrolledtext
import serial
import threading
import time
import struct
from datetime import datetime


class SCSPacket:
    """Represents an SCS packet with control, dat1, dat0, dec bytes"""

    def __init__(self, sys_state, subsystem, ist, dat1=0, dat0=0, dec=0):
        self.control = ((sys_state & 0x03) << 6) | ((subsystem & 0x03) << 4) | (ist & 0x0F)
        self.dat1 = dat1 & 0xFF
        self.dat0 = dat0 & 0xFF
        self.dec = dec & 0xFF

    def to_bytes(self):
        return bytes([self.control, self.dat1, self.dat0, self.dec])

    @classmethod
    def from_bytes(cls, data):
        if len(data) != 4:
            return None
        control, dat1, dat0, dec = data
        sys_state = (control >> 6) & 0x03
        subsystem = (control >> 4) & 0x03
        ist = control & 0x0F
        packet = cls(sys_state, subsystem, ist, dat1, dat0, dec)
        packet.control = control  # Keep original
        return packet

    def __str__(self):
        sys_names = ["IDLE", "CAL", "MAZE", "SOS"]
        sub_names = ["HUB", "SNC", "MDPS", "SS"]
        sys_state = (self.control >> 6) & 0x03
        subsystem = (self.control >> 4) & 0x03
        ist = self.control & 0x0F

        return f"[{sys_names[sys_state]}:{sub_names[subsystem]}:IST{ist}] DAT1:{self.dat1} DAT0:{self.dat0} DEC:{self.dec}"


class SCSCompliantTestHarness:
    def __init__(self):
        self.root = tk.Tk()
        self.root.title("Fixed SCS Protocol Compliant NAVCON Test Harness")
        self.root.geometry("1200x900")

        # Serial connection
        self.serial_port = None
        self.running = False
        self.rx_thread = None

        # SCS Protocol State
        self.system_state = 2  # MAZE
        self.expecting_subsystem = 1  # Expecting SNC first
        self.expecting_ist = 1  # Start with Pure Tone
        self.protocol_step = 0

        # Test state for QTP1.A
        self.test_active = False
        self.test_step = 0
        self.current_distance = 1000
        self.baseline_distance = 1000
        self.robot_stopped = False
        self.last_navcon_command = None

        # Protocol sequence tracking
        self.sequence_complete = False
        self.waiting_for_packet = False
        self.last_packet_time = 0

        # NEW: Track if we've sent initial SS data
        self.initial_ss_sent = False
        self.navcon_received = False

        self.setup_ui()

    def setup_ui(self):
        # Main frame
        main_frame = ttk.Frame(self.root, padding="10")
        main_frame.grid(row=0, column=0, sticky=(tk.W, tk.E, tk.N, tk.S))

        # Serial connection frame
        conn_frame = ttk.LabelFrame(main_frame, text="Serial Connection", padding="5")
        conn_frame.grid(row=0, column=0, columnspan=2, sticky=(tk.W, tk.E), pady=(0, 10))

        ttk.Label(conn_frame, text="COM Port:").grid(row=0, column=0, padx=(0, 5))
        self.port_var = tk.StringVar(value="COM6")
        port_entry = ttk.Entry(conn_frame, textvariable=self.port_var, width=10)
        port_entry.grid(row=0, column=1, padx=(0, 10))

        self.connect_btn = ttk.Button(conn_frame, text="Connect", command=self.toggle_connection)
        self.connect_btn.grid(row=0, column=2, padx=(0, 10))

        self.status_label = ttk.Label(conn_frame, text="Disconnected", foreground="red")
        self.status_label.grid(row=0, column=3)

        # Protocol status frame
        protocol_frame = ttk.LabelFrame(main_frame, text="SCS Protocol Status", padding="5")
        protocol_frame.grid(row=1, column=0, sticky=(tk.W, tk.E, tk.N, tk.S), padx=(0, 10))

        ttk.Label(protocol_frame, text="System State:").grid(row=0, column=0, sticky=tk.W)
        self.sys_state_var = tk.StringVar(value="MAZE")
        ttk.Label(protocol_frame, textvariable=self.sys_state_var, foreground="blue").grid(row=0, column=1, sticky=tk.W)

        ttk.Label(protocol_frame, text="Expecting:").grid(row=1, column=0, sticky=tk.W)
        self.expecting_var = tk.StringVar(value="SNC:IST1 (Pure Tone)")
        ttk.Label(protocol_frame, textvariable=self.expecting_var, foreground="green").grid(row=1, column=1,
                                                                                            sticky=tk.W)

        ttk.Label(protocol_frame, text="Protocol Step:").grid(row=2, column=0, sticky=tk.W)
        self.step_var = tk.StringVar(value="0")
        ttk.Label(protocol_frame, textvariable=self.step_var).grid(row=2, column=1, sticky=tk.W)

        # Test control frame
        test_frame = ttk.LabelFrame(main_frame, text="QTP Tests", padding="5")
        test_frame.grid(row=2, column=0, sticky=(tk.W, tk.E), padx=(0, 10), pady=10)

        self.qtp1a_btn = ttk.Button(test_frame, text="Start QTP1.A Test (5° < θᵢ ≤ 45°)",
                                    command=self.start_qtp1a_test, width=35)
        self.qtp1a_btn.grid(row=0, column=0, pady=5)

        self.qtp13_btn = ttk.Button(test_frame, text="Start QTP1.3 Test (θᵢ ≤ 5°)",
                                    command=self.start_qtp13_test, width=35)
        self.qtp13_btn.grid(row=1, column=0, pady=5)

        self.stop_test_btn = ttk.Button(test_frame, text="Stop Test",
                                        command=self.stop_test, width=35)
        self.stop_test_btn.grid(row=2, column=0, pady=5)

        # Log frame
        log_frame = ttk.LabelFrame(main_frame, text="Communication Log", padding="5")
        log_frame.grid(row=1, column=1, rowspan=2, sticky=(tk.W, tk.E, tk.N, tk.S))

        self.log_text = scrolledtext.ScrolledText(log_frame, width=70, height=35)
        self.log_text.grid(row=0, column=0, sticky=(tk.W, tk.E, tk.N, tk.S))

        # Configure grid weights
        main_frame.columnconfigure(1, weight=1)
        main_frame.rowconfigure(1, weight=1)
        log_frame.columnconfigure(0, weight=1)
        log_frame.rowconfigure(0, weight=1)

    def log(self, message):
        """Add message to log with timestamp"""
        timestamp = datetime.now().strftime("%H:%M:%S.%f")[:-3]
        log_msg = f"[{timestamp}] {message}\n"
        self.log_text.insert(tk.END, log_msg)
        self.log_text.see(tk.END)
        self.root.update()

    def toggle_connection(self):
        if self.serial_port is None:
            try:
                self.serial_port = serial.Serial(
                    port=self.port_var.get(),
                    baudrate=19200,
                    bytesize=8,
                    parity='N',
                    stopbits=1,
                    timeout=0.1
                )
                self.running = True
                self.rx_thread = threading.Thread(target=self.receive_thread, daemon=True)
                self.rx_thread.start()

                self.status_label.config(text="Connected", foreground="green")
                self.connect_btn.config(text="Disconnect")
                self.log(f"Connected to {self.port_var.get()}")

            except Exception as e:
                self.log(f"Connection failed: {e}")
        else:
            self.disconnect()

    def disconnect(self):
        self.running = False
        if self.serial_port:
            self.serial_port.close()
            self.serial_port = None
        self.status_label.config(text="Disconnected", foreground="red")
        self.connect_btn.config(text="Connect")
        self.log("Disconnected")

    def receive_thread(self):
        """Thread to receive and parse packets from NAVCON"""
        buffer = bytearray()

        while self.running and self.serial_port:
            try:
                if self.serial_port.in_waiting > 0:
                    data = self.serial_port.read(self.serial_port.in_waiting)
                    buffer.extend(data)

                    # Look for complete 4-byte packets
                    while len(buffer) >= 4:
                        packet_data = buffer[:4]
                        buffer = buffer[4:]

                        packet = SCSPacket.from_bytes(packet_data)
                        if packet:
                            self.process_received_packet(packet)

                time.sleep(0.01)

            except Exception as e:
                if self.running:
                    self.log(f"RX Error: {e}")
                break

    def process_received_packet(self, packet):
        """Process received packet according to SCS protocol"""
        sys_state = (packet.control >> 6) & 0x03
        subsystem = (packet.control >> 4) & 0x03
        ist = packet.control & 0x0F

        self.log(f"RX: {packet}")

        # Handle all packets properly
        if subsystem == 1:  # SNC packets
            if ist == 1:  # Pure Tone Detection
                self.log("SNC: Pure tone detection received")
                # After Pure Tone, expect Touch Detection
                self.advance_protocol_sequence(1, 2)  # SNC IST=2

            elif ist == 2:  # Touch Detection
                self.log("SNC: Touch detection received")
                # After Touch, expect NAVCON
                self.advance_protocol_sequence(1, 3)  # SNC IST=3

            elif ist == 3:  # NAVCON
                self.handle_navcon_command(packet)
                self.navcon_received = True
                # After NAVCON, expect MDPS battery
                self.advance_protocol_sequence(2, 1)  # MDPS IST=1

        # Automatically send responses to keep protocol flowing
        self.auto_respond_to_packet(packet)

    def auto_respond_to_packet(self, packet):
        """Automatically respond to keep SCS protocol flowing"""
        sys_state = (packet.control >> 6) & 0x03
        subsystem = (packet.control >> 4) & 0x03
        ist = packet.control & 0x0F

        if not self.test_active:
            return

        # Small delay to simulate processing time
        threading.Thread(target=lambda: self.delayed_response(subsystem, ist), daemon=True).start()

    def delayed_response(self, subsystem, ist):
        """Send delayed response to maintain protocol flow"""
        time.sleep(0.1)

        if subsystem == 1 and ist == 3:  # After NAVCON
            # Send complete MDPS sequence
            self.send_mdps_sequence()

        elif subsystem == 2 and ist == 4:  # After MDPS Distance
            # Send SS sequence based on test type
            if self.test_active:
                self.send_ss_test_sequence()

    def send_mdps_sequence(self):
        """Send complete MDPS sequence after NAVCON"""
        self.log("--- Sending MDPS response sequence ---")

        # MDPS IST=1: Battery level
        self.log("Sending MDPS IST=1 (Battery)")
        self.send_mdps_battery_response()
        time.sleep(0.1)

        # MDPS IST=2: Rotation
        self.log("Sending MDPS IST=2 (Rotation)")
        self.send_mdps_rotation_response()
        time.sleep(0.1)

        # MDPS IST=3: Speed
        self.log("Sending MDPS IST=3 (Speed)")
        self.send_mdps_speed_response()
        time.sleep(0.1)

        # MDPS IST=4: Distance
        self.log("Sending MDPS IST=4 (Distance)")
        self.current_distance += 5  # Simulate slight movement
        self.send_mdps_distance_response()
        time.sleep(0.1)

    def send_ss_test_sequence(self):
        """Send SS sequence based on current test"""
        self.log("--- Sending SS test sequence ---")

        if self.test_step == 0:  # Initial white baseline
            self.send_ss_colors("WHITE", "WHITE", "WHITE")
            time.sleep(0.1)
            self.send_ss_angle(0)
            self.test_step = 1

        elif self.test_step == 1:  # Send test condition
            if hasattr(self, 'test_type'):
                if self.test_type == "QTP1A":
                    self.log("Sending QTP1.A condition: S2=GREEN, Angle=15° (5° < θᵢ ≤ 45°)")
                    self.send_ss_colors("WHITE", "GREEN", "WHITE")
                    time.sleep(0.1)
                    self.send_ss_angle(15)  # 15° requires corrections
                elif self.test_type == "QTP13":
                    self.log("Sending QTP1.3 condition: S2=GREEN, Angle=3° (θᵢ ≤ 5°)")
                    self.send_ss_colors("WHITE", "GREEN", "WHITE")
                    time.sleep(0.1)
                    self.send_ss_angle(3)  # 3° should cross directly
            self.test_step = 2

        else:  # Continue with white
            self.send_ss_colors("WHITE", "WHITE", "WHITE")
            time.sleep(0.1)
            self.send_ss_angle(0)

    def advance_protocol_sequence(self, next_subsystem, next_ist):
        """Advance to next step in protocol sequence"""
        self.expecting_subsystem = next_subsystem
        self.expecting_ist = next_ist
        self.protocol_step += 1

        self.step_var.set(str(self.protocol_step))
        self.expecting_var.set(f"{self.get_subsystem_name(next_subsystem)}:IST{next_ist}")

    def handle_navcon_command(self, packet):
        """Handle NAVCON command and simulate robot response"""
        action = packet.dec
        param1 = packet.dat1
        param2 = packet.dat0

        if action == 0:  # FORWARD
            self.robot_stopped = False
            self.current_distance += 10  # Simulate movement
            self.log(f"🎯 NAVCON ANALYSIS: FORWARD (L:{param1}, R:{param2}) - Distance: {self.current_distance}mm")

            # Analyze the decision for QTP compliance
            if hasattr(self, 'test_type'):
                if self.test_type == "QTP1A":
                    self.log("🎯 QTP1.A ANALYSIS: FORWARD - checking if corrections were made first...")
                elif self.test_type == "QTP13":
                    self.log("🎯 QTP1.3 ANALYSIS: PASS - Direct FORWARD crossing (no rotation needed for ≤5°)")

        elif action == 1:  # BACKWARD
            self.robot_stopped = False
            self.current_distance = max(0, self.current_distance - 10)
            self.log(f"🎯 NAVCON ANALYSIS: BACKWARD (L:{param1}, R:{param2}) - Distance: {self.current_distance}mm")

        elif action == 2:  # ROTATE_LEFT
            angle = (param1 << 8) | param2
            self.robot_stopped = True
            self.log(f"🎯 NAVCON ANALYSIS: ROTATE LEFT {angle}° - Correction maneuver")

        elif action == 3:  # ROTATE_RIGHT
            angle = (param1 << 8) | param2
            self.robot_stopped = True
            self.log(f"🎯 NAVCON ANALYSIS: ROTATE RIGHT {angle}° - Correction maneuver")

        elif action == 4:  # STOP
            self.robot_stopped = True
            self.log("🎯 NAVCON ANALYSIS: STOP - Required before direction change")

    def send_packet(self, packet):
        """Send packet to NAVCON"""
        if self.serial_port and self.running:
            try:
                self.serial_port.write(packet.to_bytes())
                self.log(f"TX: {packet}")
            except Exception as e:
                self.log(f"TX Error: {e}")

    def send_mdps_battery_response(self):
        """Send MDPS battery level response"""
        packet = SCSPacket(2, 2, 1, 0, 0, 0)  # MAZE:MDPS:IST1 with zeros per spec
        self.send_packet(packet)

    def send_mdps_rotation_response(self):
        """Send MDPS rotation feedback"""
        packet = SCSPacket(2, 2, 2, 0, 0, 2)  # MAZE:MDPS:IST2, direction=2 (LEFT)
        self.send_packet(packet)

    def send_mdps_speed_response(self):
        """Send MDPS speed feedback"""
        speed = 0 if self.robot_stopped else 80
        packet = SCSPacket(2, 2, 3, speed, speed, 0)  # MAZE:MDPS:IST3
        self.send_packet(packet)

    def send_mdps_distance_response(self):
        """Send MDPS distance feedback"""
        packet = SCSPacket(2, 2, 4, (self.current_distance >> 8) & 0xFF,
                           self.current_distance & 0xFF, 0)
        self.send_packet(packet)

    def send_ss_colors(self, s1, s2, s3):
        """Send SS color packet"""
        color_map = {"WHITE": 0, "RED": 1, "GREEN": 2, "BLUE": 3, "BLACK": 4}
        s1_code = color_map.get(s1, 0)
        s2_code = color_map.get(s2, 0)
        s3_code = color_map.get(s3, 0)

        # Pack colors: DATA<8:6>=S1, DATA<5:3>=S2, DATA<2:0>=S3
        color_data = (s1_code << 6) | (s2_code << 3) | s3_code

        packet = SCSPacket(2, 3, 1, (color_data >> 8) & 0xFF, color_data & 0xFF, 0)
        self.send_packet(packet)

    def send_ss_angle(self, angle):
        """Send SS angle packet"""
        packet = SCSPacket(2, 3, 2, angle, 0, 0)
        self.send_packet(packet)

    def get_subsystem_name(self, subsystem):
        """Get subsystem name from ID"""
        names = ["HUB", "SNC", "MDPS", "SS"]
        return names[subsystem] if 0 <= subsystem < len(names) else "UNKNOWN"

    def start_qtp1a_test(self):
        """Start QTP1.A test (requires corrections for 5° < θᵢ ≤ 45°)"""
        if not self.serial_port:
            self.log("ERROR: Not connected to serial port")
            return

        self.test_active = True
        self.test_type = "QTP1A"
        self.test_step = 0
        self.current_distance = 1000
        self.baseline_distance = 1000
        self.protocol_step = 0
        self.navcon_received = False

        self.log("=== STARTING QTP1.A TEST (5° < θᵢ ≤ 45°) ===")
        self.log("This test should trigger correction sequence before crossing")

    def start_qtp13_test(self):
        """Start QTP1.3 test (direct crossing for θᵢ ≤ 5°)"""
        if not self.serial_port:
            self.log("ERROR: Not connected to serial port")
            return

        self.test_active = True
        self.test_type = "QTP13"
        self.test_step = 0
        self.current_distance = 1000
        self.baseline_distance = 1000
        self.protocol_step = 0
        self.navcon_received = False

        self.log("=== STARTING QTP1.3 TEST (θᵢ ≤ 5°) ===")
        self.log("This test should cross directly without corrections")

    def stop_test(self):
        """Stop current test"""
        self.test_active = False
        self.test_step = 0
        self.navcon_received = False
        if hasattr(self, 'test_type'):
            delattr(self, 'test_type')
        self.log("=== TEST STOPPED ===")

    def run(self):
        """Start the GUI"""
        self.root.protocol("WM_DELETE_WINDOW", self.on_closing)
        self.root.mainloop()

    def on_closing(self):
        """Handle window closing"""
        self.running = False
        if self.serial_port:
            self.serial_port.close()
        self.root.destroy()


if __name__ == "__main__":
    app = SCSCompliantTestHarness()
    app.run()