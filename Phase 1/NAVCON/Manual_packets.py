#!/usr/bin/env python3
"""
Manual NAVCON Test Harness with Parameter Selection
Allows manual control of MDPS and SS packet parameters
"""

import tkinter as tk
from tkinter import ttk, scrolledtext
import serial
import threading
import time
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
        packet.control = control
        return packet

    def __str__(self):
        sys_names = ["IDLE", "CAL", "MAZE", "SOS"]
        sub_names = ["HUB", "SNC", "MDPS", "SS"]
        sys_state = (self.control >> 6) & 0x03
        subsystem = (self.control >> 4) & 0x03
        ist = self.control & 0x0F

        return f"[{sys_names[sys_state]}:{sub_names[subsystem]}:IST{ist}] DAT1:{self.dat1} DAT0:{self.dat0} DEC:{self.dec}"


class ManualNavconTestHarness:
    def __init__(self):
        self.root = tk.Tk()
        self.root.title("Manual NAVCON Test Harness")
        self.root.geometry("1400x1000")

        # Serial connection
        self.serial_port = None
        self.running = False
        self.rx_thread = None

        self.setup_ui()

    def setup_ui(self):
        # Main frame
        main_frame = ttk.Frame(self.root, padding="10")
        main_frame.grid(row=0, column=0, sticky=(tk.W, tk.E, tk.N, tk.S))

        # Serial connection frame
        conn_frame = ttk.LabelFrame(main_frame, text="Serial Connection", padding="5")
        conn_frame.grid(row=0, column=0, columnspan=3, sticky=(tk.W, tk.E), pady=(0, 10))

        ttk.Label(conn_frame, text="COM Port:").grid(row=0, column=0, padx=(0, 5))
        self.port_var = tk.StringVar(value="COM6")
        port_entry = ttk.Entry(conn_frame, textvariable=self.port_var, width=10)
        port_entry.grid(row=0, column=1, padx=(0, 10))

        self.connect_btn = ttk.Button(conn_frame, text="Connect", command=self.toggle_connection)
        self.connect_btn.grid(row=0, column=2, padx=(0, 10))

        self.status_label = ttk.Label(conn_frame, text="Disconnected", foreground="red")
        self.status_label.grid(row=0, column=3)

        # MDPS Parameters Frame
        mdps_frame = ttk.LabelFrame(main_frame, text="MDPS Parameters", padding="5")
        mdps_frame.grid(row=1, column=0, sticky=(tk.W, tk.E, tk.N, tk.S), padx=(0, 5))

        # MDPS IST=1 (Battery)
        ttk.Label(mdps_frame, text="MDPS IST=1 (Battery):").grid(row=0, column=0, columnspan=2, sticky=tk.W)
        ttk.Label(mdps_frame, text="Always sends 0,0,0").grid(row=0, column=2, sticky=tk.W)

        # MDPS IST=2 (Rotation)
        ttk.Label(mdps_frame, text="MDPS IST=2 (Rotation):").grid(row=1, column=0, sticky=tk.W)
        ttk.Label(mdps_frame, text="Angle:").grid(row=2, column=0, sticky=tk.W)
        self.rotation_angle_var = tk.StringVar(value="90")
        rotation_entry = ttk.Entry(mdps_frame, textvariable=self.rotation_angle_var, width=10)
        rotation_entry.grid(row=2, column=1, padx=5)

        ttk.Label(mdps_frame, text="Direction:").grid(row=3, column=0, sticky=tk.W)
        self.rotation_dir_var = tk.StringVar(value="LEFT")
        rotation_combo = ttk.Combobox(mdps_frame, textvariable=self.rotation_dir_var,
                                      values=["LEFT", "RIGHT"], width=10)
        rotation_combo.grid(row=3, column=1, padx=5)

        # MDPS IST=3 (Speed)
        ttk.Label(mdps_frame, text="MDPS IST=3 (Speed):").grid(row=4, column=0, sticky=tk.W)
        ttk.Label(mdps_frame, text="Left Speed:").grid(row=5, column=0, sticky=tk.W)
        self.left_speed_var = tk.StringVar(value="80")
        left_speed_entry = ttk.Entry(mdps_frame, textvariable=self.left_speed_var, width=10)
        left_speed_entry.grid(row=5, column=1, padx=5)

        ttk.Label(mdps_frame, text="Right Speed:").grid(row=6, column=0, sticky=tk.W)
        self.right_speed_var = tk.StringVar(value="80")
        right_speed_entry = ttk.Entry(mdps_frame, textvariable=self.right_speed_var, width=10)
        right_speed_entry.grid(row=6, column=1, padx=5)

        # MDPS IST=4 (Distance)
        ttk.Label(mdps_frame, text="MDPS IST=4 (Distance):").grid(row=7, column=0, sticky=tk.W)
        ttk.Label(mdps_frame, text="Distance (mm):").grid(row=8, column=0, sticky=tk.W)
        self.distance_var = tk.StringVar(value="1000")
        distance_entry = ttk.Entry(mdps_frame, textvariable=self.distance_var, width=10)
        distance_entry.grid(row=8, column=1, padx=5)

        # SS Parameters Frame
        ss_frame = ttk.LabelFrame(main_frame, text="SS Parameters", padding="5")
        ss_frame.grid(row=1, column=1, sticky=(tk.W, tk.E, tk.N, tk.S), padx=5)

        # SS IST=1 (Colors)
        ttk.Label(ss_frame, text="SS IST=1 (Colors):").grid(row=0, column=0, columnspan=2, sticky=tk.W)

        ttk.Label(ss_frame, text="Sensor 1 (Left):").grid(row=1, column=0, sticky=tk.W)
        self.s1_color_var = tk.StringVar(value="WHITE")
        s1_combo = ttk.Combobox(ss_frame, textvariable=self.s1_color_var,
                                values=["WHITE", "RED", "GREEN", "BLUE", "BLACK"], width=10)
        s1_combo.grid(row=1, column=1, padx=5)

        ttk.Label(ss_frame, text="Sensor 2 (Middle):").grid(row=2, column=0, sticky=tk.W)
        self.s2_color_var = tk.StringVar(value="GREEN")
        s2_combo = ttk.Combobox(ss_frame, textvariable=self.s2_color_var,
                                values=["WHITE", "RED", "GREEN", "BLUE", "BLACK"], width=10)
        s2_combo.grid(row=2, column=1, padx=5)

        ttk.Label(ss_frame, text="Sensor 3 (Right):").grid(row=3, column=0, sticky=tk.W)
        self.s3_color_var = tk.StringVar(value="WHITE")
        s3_combo = ttk.Combobox(ss_frame, textvariable=self.s3_color_var,
                                values=["WHITE", "RED", "GREEN", "BLUE", "BLACK"], width=10)
        s3_combo.grid(row=3, column=1, padx=5)

        # SS IST=2 (Angle)
        ttk.Label(ss_frame, text="SS IST=2 (Angle):").grid(row=4, column=0, sticky=tk.W)
        ttk.Label(ss_frame, text="Angle (degrees):").grid(row=5, column=0, sticky=tk.W)
        self.angle_var = tk.StringVar(value="15")
        angle_entry = ttk.Entry(ss_frame, textvariable=self.angle_var, width=10)
        angle_entry.grid(row=5, column=1, padx=5)

        # Quick Test Presets Frame
        preset_frame = ttk.LabelFrame(main_frame, text="Quick Test Presets", padding="5")
        preset_frame.grid(row=2, column=0, columnspan=2, sticky=(tk.W, tk.E), pady=10)

        ttk.Button(preset_frame, text="QTP 1.A Preset (15°, S2=GREEN)",
                   command=self.set_qtp1a_preset, width=25).grid(row=0, column=0, padx=5)
        ttk.Button(preset_frame, text="QTP 1.3 Preset (3°, S2=GREEN)",
                   command=self.set_qtp13_preset, width=25).grid(row=0, column=1, padx=5)
        ttk.Button(preset_frame, text="QTP 2 Preset (S1=GREEN)",
                   command=self.set_qtp2_preset, width=25).grid(row=0, column=2, padx=5)
        ttk.Button(preset_frame, text="Wall Test (S2=BLUE)",
                   command=self.set_wall_preset, width=25).grid(row=1, column=0, padx=5)
        ttk.Button(preset_frame, text="All White",
                   command=self.set_white_preset, width=25).grid(row=1, column=1, padx=5)

        # Send Button Frame
        send_frame = ttk.LabelFrame(main_frame, text="Send Sequence", padding="5")
        send_frame.grid(row=3, column=0, columnspan=2, sticky=(tk.W, tk.E), pady=10)

        self.send_btn = ttk.Button(send_frame, text="Send MDPS(1,2,3,4) + SS(1,2) Sequence",
                                   command=self.send_complete_sequence, width=40)
        self.send_btn.grid(row=0, column=0, padx=10, pady=10)

        ttk.Label(send_frame,
                  text="Sends: MDPS Battery → MDPS Rotation → MDPS Speed → MDPS Distance → SS Colors → SS Angle",
                  foreground="gray").grid(row=1, column=0)

        # Log frame
        log_frame = ttk.LabelFrame(main_frame, text="Communication Log", padding="5")
        log_frame.grid(row=1, column=2, rowspan=3, sticky=(tk.W, tk.E, tk.N, tk.S))

        self.log_text = scrolledtext.ScrolledText(log_frame, width=50, height=40)
        self.log_text.grid(row=0, column=0, sticky=(tk.W, tk.E, tk.N, tk.S))

        # Configure grid weights
        main_frame.columnconfigure(2, weight=1)
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
        """Thread to receive and parse packets"""
        buffer = bytearray()

        while self.running and self.serial_port:
            try:
                if self.serial_port.in_waiting > 0:
                    data = self.serial_port.read(self.serial_port.in_waiting)
                    buffer.extend(data)

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
        """Process received packet"""
        sys_state = (packet.control >> 6) & 0x03
        subsystem = (packet.control >> 4) & 0x03
        ist = packet.control & 0x0F

        self.log(f"RX: {packet}")

        # Analyze NAVCON commands
        if subsystem == 1 and ist == 3:  # SNC NAVCON
            self.analyze_navcon_command(packet)

    def analyze_navcon_command(self, packet):
        """Analyze NAVCON command for QTP compliance"""
        action = packet.dec
        param1 = packet.dat1
        param2 = packet.dat0

        if action == 0:  # FORWARD
            self.log(f"NAVCON: FORWARD (L:{param1}, R:{param2})")

        elif action == 1:  # BACKWARD
            self.log(f"NAVCON: BACKWARD (L:{param1}, R:{param2})")

        elif action == 2:  # ROTATE_LEFT
            angle = (param1 << 8) | param2
            self.log(f"NAVCON: ROTATE LEFT {angle}°")

        elif action == 3:  # ROTATE_RIGHT
            angle = (param1 << 8) | param2
            self.log(f"NAVCON: ROTATE RIGHT {angle}°")

        elif action == 4:  # STOP
            self.log("NAVCON: STOP")

    def send_packet(self, packet):
        """Send packet"""
        if self.serial_port and self.running:
            try:
                self.serial_port.write(packet.to_bytes())
                self.log(f"TX: {packet}")
            except Exception as e:
                self.log(f"TX Error: {e}")

    def send_complete_sequence(self):
        """Send complete MDPS + SS sequence with user parameters"""
        if not self.serial_port:
            self.log("ERROR: Not connected")
            return

        self.log("=== SENDING COMPLETE MDPS + SS SEQUENCE ===")

        try:
            # MDPS IST=1 (Battery) - Always 0,0,0
            packet = SCSPacket(2, 2, 1, 0, 0, 0)
            self.send_packet(packet)
            time.sleep(0.2)

            # MDPS IST=2 (Rotation)
            angle = int(self.rotation_angle_var.get())
            direction = 2 if self.rotation_dir_var.get() == "LEFT" else 3
            packet = SCSPacket(2, 2, 2, (angle >> 8) & 0xFF, angle & 0xFF, direction)
            self.send_packet(packet)
            time.sleep(0.2)

            # MDPS IST=3 (Speed)
            left_speed = int(self.left_speed_var.get())
            right_speed = int(self.right_speed_var.get())
            packet = SCSPacket(2, 2, 3, right_speed, left_speed, 0)
            self.send_packet(packet)
            time.sleep(0.2)

            # MDPS IST=4 (Distance)
            distance = int(self.distance_var.get())
            packet = SCSPacket(2, 2, 4, (distance >> 8) & 0xFF, distance & 0xFF, 0)
            self.send_packet(packet)
            time.sleep(0.2)

            # SS IST=1 (Colors)
            color_map = {"WHITE": 0, "RED": 1, "GREEN": 2, "BLUE": 3, "BLACK": 4}
            s1_code = color_map[self.s1_color_var.get()]
            s2_code = color_map[self.s2_color_var.get()]
            s3_code = color_map[self.s3_color_var.get()]

            color_data = (s1_code << 6) | (s2_code << 3) | s3_code
            packet = SCSPacket(2, 3, 1, (color_data >> 8) & 0xFF, color_data & 0xFF, 0)
            self.send_packet(packet)
            time.sleep(0.2)

            # SS IST=2 (Angle)
            angle = int(self.angle_var.get())
            packet = SCSPacket(2, 3, 2, angle, 0, 0)
            self.send_packet(packet)

            self.log("=== SEQUENCE COMPLETE ===")

        except ValueError as e:
            self.log(f"ERROR: Invalid parameter value - {e}")

    def set_qtp1a_preset(self):
        """Set parameters for QTP 1.A test (5° < θᵢ ≤ 45°)"""
        self.s1_color_var.set("WHITE")
        self.s2_color_var.set("GREEN")
        self.s3_color_var.set("WHITE")
        self.angle_var.set("15")
        self.log("QTP 1.A preset loaded: S2=GREEN, 15° (should trigger corrections)")

    def set_qtp13_preset(self):
        """Set parameters for QTP 1.3 test (θᵢ ≤ 5°)"""
        self.s1_color_var.set("WHITE")
        self.s2_color_var.set("GREEN")
        self.s3_color_var.set("WHITE")
        self.angle_var.set("3")
        self.log("QTP 1.3 preset loaded: S2=GREEN, 3° (should cross directly)")

    def set_qtp2_preset(self):
        """Set parameters for QTP 2 test (edge sensor trigger)"""
        self.s1_color_var.set("GREEN")
        self.s2_color_var.set("WHITE")
        self.s3_color_var.set("WHITE")
        self.angle_var.set("50")
        self.log("QTP 2 preset loaded: S1=GREEN, 50° (edge sensor trigger)")

    def set_wall_preset(self):
        """Set parameters for wall navigation test"""
        self.s1_color_var.set("WHITE")
        self.s2_color_var.set("BLUE")
        self.s3_color_var.set("WHITE")
        self.angle_var.set("30")
        self.log("Wall test preset loaded: S2=BLUE, 30°")

    def set_white_preset(self):
        """Set all sensors to white"""
        self.s1_color_var.set("WHITE")
        self.s2_color_var.set("WHITE")
        self.s3_color_var.set("WHITE")
        self.angle_var.set("0")
        self.log("All white preset loaded")

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
    app = ManualNavconTestHarness()
    app.run()