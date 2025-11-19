#!/usr/bin/env python3
"""
MARV ESP32 Testing Interface - Fixed for Phase0.ino compatibility
Shows received packets and processes them correctly
"""

import tkinter as tk
from tkinter import ttk, scrolledtext, messagebox
import serial
import serial.tools.list_ports
import threading
import time
from datetime import datetime
from enum import Enum


class SystemState(Enum):
    IDLE = 0
    CAL = 1
    MAZE = 2
    SOS = 3


class SubsystemID(Enum):
    HUB = 0
    SNC = 1
    MDPS = 2
    SS = 3


class SCSPacket:
    def __init__(self, control=0, dat1=0, dat0=0, dec=0):
        self.control = control
        self.dat1 = dat1
        self.dat0 = dat0
        self.dec = dec

    def get_system_state(self):
        return SystemState((self.control >> 6) & 0x03)

    def get_subsystem_id(self):
        return SubsystemID((self.control >> 4) & 0x03)

    def get_internal_state(self):
        return self.control & 0x0F

    def to_bytes(self):
        return bytes([self.control, self.dat1, self.dat0, self.dec])

    def from_bytes(self, data):
        if len(data) >= 4:
            self.control = data[0]
            self.dat1 = data[1]
            self.dat0 = data[2]
            self.dec = data[3]
            return True
        return False

    def __str__(self):
        sys_state = self.get_system_state().name
        subsystem = self.get_subsystem_id().name
        ist = self.get_internal_state()
        return f"[{sys_state}:{subsystem}:IST{ist}] 0x{self.control:02X} {self.dat1} {self.dat0} {self.dec}"

    def get_detailed_description(self):
        """Get detailed description based on actual ESP32 implementation"""
        sys_state = self.get_system_state()
        subsystem = self.get_subsystem_id()
        ist = self.get_internal_state()

        desc = f"{sys_state.name}:{subsystem.name}:IST{ist}"

        if subsystem == SubsystemID.SNC:
            if sys_state == SystemState.IDLE and ist == 0:
                # SNC_TOUCH_IDLE from Phase0.ino
                desc += f" - Touch Detection (IDLE): {'DETECTED' if self.dat1 else 'NOT DETECTED'}"
                if self.dat0 > 0:
                    desc += f", vop speed: {self.dat0}mm/s"
            elif sys_state == SystemState.CAL and ist == 0:
                # SNC_TOUCH_CAL from Phase0.ino
                desc += f" - Touch Detection (CAL): {'DETECTED' if self.dat1 else 'NOT DETECTED'}"
            elif sys_state == SystemState.MAZE and ist == 1:
                # SNC_PURETONE_MAZE from Phase0.ino
                desc += f" - Pure Tone Detection (MAZE): {'DETECTED' if self.dat1 else 'NOT DETECTED'}"
            elif sys_state == SystemState.SOS and ist == 0:
                # SNC_PURETONE_SOS from Phase0.ino
                desc += f" - Pure Tone Detection (SOS): {'DETECTED' if self.dat1 else 'NOT DETECTED'}"

        elif subsystem == SubsystemID.MDPS:
            # Based on your ESP32 forwarding behavior
            desc += " - FORWARDED from other subsystem"

        elif subsystem == SubsystemID.SS:
            # Based on your ESP32 forwarding behavior
            desc += " - FORWARDED from other subsystem"

        return desc


def create_control_byte(sys_state, subsystem, ist):
    return ((sys_state.value & 0x03) << 6) | ((subsystem.value & 0x03) << 4) | (ist & 0x0F)


class MARVTestInterface:
    def __init__(self, root):
        self.root = root
        self.root.title("MARV ESP32 Testing Interface - Phase 0 Compatible")
        self.root.geometry("1400x900")  # Reduced from 1600x1000 for better small screen compatibility
        self.root.resizable(True, True)

        # Serial connection
        self.serial_port = None
        self.serial_thread = None
        self.running = False

        # Packet tracking
        self.received_packet_count = 0
        self.transmitted_packet_count = 0
        self.packet_history = []

        # Create UI
        self.create_widgets()

    def create_widgets(self):
        # Main frame with scrollable canvas for small screens
        main_canvas = tk.Canvas(self.root)
        scrollbar = ttk.Scrollbar(self.root, orient="vertical", command=main_canvas.yview)
        scrollable_frame = ttk.Frame(main_canvas)

        scrollable_frame.bind(
            "<Configure>",
            lambda e: main_canvas.configure(scrollregion=main_canvas.bbox("all"))
        )

        main_canvas.create_window((0, 0), window=scrollable_frame, anchor="nw")
        main_canvas.configure(yscrollcommand=scrollbar.set)

        main_canvas.pack(side="left", fill="both", expand=True)
        scrollbar.pack(side="right", fill="y")

        # Bind mousewheel to canvas
        def _on_mousewheel(event):
            main_canvas.yview_scroll(int(-1*(event.delta/120)), "units")
        main_canvas.bind_all("<MouseWheel>", _on_mousewheel)

        # Main frame inside scrollable area
        main_frame = ttk.Frame(scrollable_frame, padding="10")
        main_frame.pack(fill="both", expand=True)

        # Configure grid weights
        main_frame.columnconfigure(1, weight=1)
        main_frame.rowconfigure(2, weight=1)

        # Serial connection frame
        serial_frame = ttk.LabelFrame(main_frame, text="Serial Connection", padding="5")
        serial_frame.pack(fill="x", pady=(0, 10))

        ttk.Label(serial_frame, text="Port:").grid(row=0, column=0, padx=(0, 5))
        self.port_var = tk.StringVar()
        self.port_combo = ttk.Combobox(serial_frame, textvariable=self.port_var, width=15)
        self.port_combo.grid(row=0, column=1, padx=(0, 10))

        ttk.Button(serial_frame, text="Refresh Ports", command=self.refresh_ports).grid(row=0, column=2, padx=(0, 10))
        self.connect_btn = ttk.Button(serial_frame, text="Connect", command=self.toggle_connection)
        self.connect_btn.grid(row=0, column=3)

        self.status_label = ttk.Label(serial_frame, text="Disconnected", foreground="red")
        self.status_label.grid(row=0, column=4, padx=(10, 0))

        # Statistics frame
        stats_frame = ttk.LabelFrame(main_frame, text="Communication Statistics", padding="5")
        stats_frame.pack(fill="x", pady=(0, 10))

        ttk.Label(stats_frame, text="Packets Received:").grid(row=0, column=0, padx=(0, 5))
        self.rx_count_label = ttk.Label(stats_frame, text="0", font=("Arial", 12, "bold"), foreground="blue")
        self.rx_count_label.grid(row=0, column=1, padx=(0, 20))

        ttk.Label(stats_frame, text="Packets Sent:").grid(row=0, column=2, padx=(0, 5))
        self.tx_count_label = ttk.Label(stats_frame, text="0", font=("Arial", 12, "bold"), foreground="green")
        self.tx_count_label.grid(row=0, column=3, padx=(0, 20))

        ttk.Button(stats_frame, text="Reset Counters", command=self.reset_counters).grid(row=0, column=4, padx=(20, 0))

        # Main content frame - using horizontal layout
        content_frame = ttk.Frame(main_frame)
        content_frame.pack(fill="both", expand=True)
        content_frame.columnconfigure(1, weight=1)
        content_frame.rowconfigure(0, weight=1)

        # Control panels - wrapped in a scrollable frame
        control_canvas = tk.Canvas(content_frame, width=450, height=600)
        control_scrollbar = ttk.Scrollbar(content_frame, orient="vertical", command=control_canvas.yview)
        control_scrollable = ttk.Frame(control_canvas)

        control_scrollable.bind(
            "<Configure>",
            lambda e: control_canvas.configure(scrollregion=control_canvas.bbox("all"))
        )

        control_canvas.create_window((0, 0), window=control_scrollable, anchor="nw")
        control_canvas.configure(yscrollcommand=control_scrollbar.set)

        control_canvas.grid(row=0, column=0, sticky="nsew", padx=(0, 10))
        control_scrollbar.grid(row=0, column=0, sticky="nse", padx=(440, 10))

        # SNC Subsystem controls
        snc_frame = ttk.LabelFrame(control_scrollable, text="SNC (State & Navigation Control)", padding="5")
        snc_frame.pack(fill="x", pady=(0, 10))

        # IDLE state SNC packets
        ttk.Label(snc_frame, text="IDLE State:", font=("Arial", 9, "bold")).grid(row=0, column=0, columnspan=2, sticky=tk.W)
        ttk.Button(snc_frame, text="SNC Touch (No Touch)",
                   command=lambda: self.send_snc_packet(0, 0, 0, 0, 50)).grid(row=1, column=0, padx=2, pady=1)
        ttk.Button(snc_frame, text="SNC Touch (DETECTED)",
                   command=lambda: self.send_snc_packet(0, 0, 1, 0, 50)).grid(row=1, column=1, padx=2, pady=1)

        # CAL state SNC packets
        ttk.Label(snc_frame, text="CAL State:", font=("Arial", 9, "bold")).grid(row=2, column=0, columnspan=2, sticky=tk.W, pady=(10, 0))
        ttk.Button(snc_frame, text="SNC Touch Check (No)",
                   command=lambda: self.send_snc_packet(1, 0, 0, 0, 0)).grid(row=3, column=0, padx=2, pady=1)
        ttk.Button(snc_frame, text="SNC Touch Check (YES)",
                   command=lambda: self.send_snc_packet(1, 0, 1, 0, 0)).grid(row=3, column=1, padx=2, pady=1)

        # MAZE state SNC packets
        ttk.Label(snc_frame, text="MAZE State:", font=("Arial", 9, "bold")).grid(row=4, column=0, columnspan=2, sticky=tk.W, pady=(10, 0))
        ttk.Button(snc_frame, text="SNC Pure Tone (No)",
                   command=lambda: self.send_snc_packet(2, 1, 0, 0, 0)).grid(row=5, column=0, padx=2, pady=1)
        ttk.Button(snc_frame, text="SNC Pure Tone (YES)",
                   command=lambda: self.send_snc_packet(2, 1, 1, 0, 0)).grid(row=5, column=1, padx=2, pady=1)
        ttk.Button(snc_frame, text="SNC Touch (MAZE)",
                   command=lambda: self.send_snc_packet(2, 2, 1, 0, 0)).grid(row=6, column=0, padx=2, pady=1)
        ttk.Button(snc_frame, text="SNC Navigation (Forward)",
                   command=lambda: self.send_snc_navigation(50, 50, 0)).grid(row=6, column=1, padx=2, pady=1)

        # SOS state SNC packets
        ttk.Label(snc_frame, text="SOS State:", font=("Arial", 9, "bold")).grid(row=7, column=0, columnspan=2, sticky=tk.W, pady=(10, 0))
        ttk.Button(snc_frame, text="SNC Pure Tone (No)",
                   command=lambda: self.send_snc_packet(3, 0, 0, 0, 0)).grid(row=8, column=0, padx=2, pady=1)
        ttk.Button(snc_frame, text="SNC Pure Tone (YES)",
                   command=lambda: self.send_snc_packet(3, 0, 1, 0, 0)).grid(row=8, column=1, padx=2, pady=1)

        # Navigation controls
        nav_frame = ttk.LabelFrame(snc_frame, text="Navigation Commands", padding="3")
        nav_frame.grid(row=9, column=0, columnspan=2, sticky=(tk.W, tk.E), pady=(5, 0))

        ttk.Button(nav_frame, text="Turn Right 90°",
                   command=lambda: self.send_snc_rotation(90, 3)).grid(row=0, column=0, padx=1, pady=1)
        ttk.Button(nav_frame, text="Turn Left 90°",
                   command=lambda: self.send_snc_rotation(90, 2)).grid(row=0, column=1, padx=1, pady=1)
        ttk.Button(nav_frame, text="Turn 180°",
                   command=lambda: self.send_snc_rotation(180, 3)).grid(row=1, column=0, padx=1, pady=1)
        ttk.Button(nav_frame, text="Stop",
                   command=lambda: self.send_snc_navigation(0, 0, 0)).grid(row=1, column=1, padx=1, pady=1)

        # SS Subsystem controls
        ss_frame = ttk.LabelFrame(control_scrollable, text="SS (Sensor Subsystem)", padding="5")
        ss_frame.pack(fill="x", pady=(0, 10))

        # CAL state SS packets
        ttk.Label(ss_frame, text="CAL State:", font=("Arial", 9, "bold")).grid(row=0, column=0, columnspan=2, sticky=tk.W)
        ttk.Button(ss_frame, text="SS End of Calibration",
                   command=lambda: self.send_ss_packet(0, 1)).grid(row=1, column=0, padx=2, pady=2)
        ttk.Button(ss_frame, text="SS Colors (CAL)",
                   command=lambda: self.send_ss_packet(1, 1)).grid(row=1, column=1, padx=2, pady=2)

        # MAZE state SS packets
        ttk.Label(ss_frame, text="MAZE State:", font=("Arial", 9, "bold")).grid(row=2, column=0, columnspan=2, sticky=tk.W, pady=(10, 0))
        ttk.Button(ss_frame, text="SS Colors (MAZE)",
                   command=lambda: self.send_ss_packet(1, 2)).grid(row=3, column=0, padx=2, pady=2)
        ttk.Button(ss_frame, text="SS Incidence 15°",
                   command=lambda: self.send_ss_packet(2, 2, angle=15)).grid(row=3, column=1, padx=2, pady=2)
        ttk.Button(ss_frame, text="SS Incidence 30°",
                   command=lambda: self.send_ss_packet(2, 2, angle=30)).grid(row=4, column=0, padx=2, pady=2)
        ttk.Button(ss_frame, text="SS End of Maze",
                   command=lambda: self.send_ss_packet(3, 2)).grid(row=4, column=1, padx=2, pady=2)

        # Color simulation
        color_frame = ttk.LabelFrame(ss_frame, text="Color Simulation", padding="3")
        color_frame.grid(row=5, column=0, columnspan=2, sticky=(tk.W, tk.E), pady=(5, 0))

        ttk.Button(color_frame, text="All White",
                   command=lambda: self.send_ss_colors([0, 0, 0])).grid(row=0, column=0, padx=1, pady=1)
        ttk.Button(color_frame, text="Red-White-Blue",
                   command=lambda: self.send_ss_colors([1, 0, 3])).grid(row=0, column=1, padx=1, pady=1)
        ttk.Button(color_frame, text="All Black",
                   command=lambda: self.send_ss_colors([4, 4, 4])).grid(row=1, column=0, padx=1, pady=1)
        ttk.Button(color_frame, text="Green Line",
                   command=lambda: self.send_ss_colors([0, 2, 0])).grid(row=1, column=1, padx=1, pady=1)

        # MDPS Subsystem controls - FIXED with SOS state
        mdps_frame = ttk.LabelFrame(control_scrollable, text="MDPS (Motor Driver Power Supply)", padding="5")
        mdps_frame.pack(fill="x", pady=(0, 10))

        # CAL state MDPS packets
        ttk.Label(mdps_frame, text="CAL State:", font=("Arial", 9, "bold")).grid(row=0, column=0, columnspan=2, sticky=tk.W)
        ttk.Button(mdps_frame, text="MDPS vop Calibration",
                   command=lambda: self.send_mdps_packet(0, 1)).grid(row=1, column=0, padx=2, pady=2)
        ttk.Button(mdps_frame, text="MDPS Battery Level",
                   command=lambda: self.send_mdps_packet(1, 1)).grid(row=1, column=1, padx=2, pady=2)

        # MAZE state MDPS packets
        ttk.Label(mdps_frame, text="MAZE State:", font=("Arial", 9, "bold")).grid(row=2, column=0, columnspan=2, sticky=tk.W, pady=(10, 0))
        ttk.Button(mdps_frame, text="MDPS Battery",
                   command=lambda: self.send_mdps_packet(1, 2)).grid(row=3, column=0, padx=2, pady=2)
        ttk.Button(mdps_frame, text="MDPS Rotation 90°",
                   command=lambda: self.send_mdps_packet(2, 2, angle=90)).grid(row=3, column=1, padx=2, pady=2)
        ttk.Button(mdps_frame, text="MDPS Speed",
                   command=lambda: self.send_mdps_packet(3, 2)).grid(row=4, column=0, padx=2, pady=2)
        ttk.Button(mdps_frame, text="MDPS Distance 200mm",
                   command=lambda: self.send_mdps_packet(4, 2, distance=200)).grid(row=4, column=1, padx=2, pady=2)

        # SOS state MDPS packets - NEW ADDITION
        ttk.Label(mdps_frame, text="SOS State:", font=("Arial", 9, "bold"), foreground="red").grid(row=5, column=0, columnspan=2, sticky=tk.W, pady=(10, 0))
        ttk.Button(mdps_frame, text="MDPS Pure Tone Response",
                   command=lambda: self.send_mdps_packet(4, 3),
                   style="Accent.TButton").grid(row=6, column=0, columnspan=2, padx=2, pady=2, sticky="ew")

        # Quick test scenarios
        scenario_frame = ttk.LabelFrame(control_scrollable, text="Quick Test Scenarios", padding="5")
        scenario_frame.pack(fill="x", pady=(0, 10))

        ttk.Button(scenario_frame, text="🚀 Start System (Touch)",
                   command=self.scenario_start_system, width=25).grid(row=0, column=0, pady=3)
        ttk.Button(scenario_frame, text="🔧 Complete CAL Sequence",
                   command=self.scenario_complete_cal, width=25).grid(row=1, column=0, pady=3)
        ttk.Button(scenario_frame, text="🎯 Enter MAZE State",
                   command=self.scenario_enter_maze, width=25).grid(row=2, column=0, pady=3)
        ttk.Button(scenario_frame, text="🔄 MAZE Loop Test",
                   command=self.scenario_maze_loop, width=25).grid(row=3, column=0, pady=3)
        ttk.Button(scenario_frame, text="🚨 Emergency SOS",
                   command=self.scenario_sos, width=25).grid(row=4, column=0, pady=3)

        # Manual packet sender
        manual_frame = ttk.LabelFrame(control_scrollable, text="Manual Packet Builder", padding="5")
        manual_frame.pack(fill="x", pady=(10, 0))

        ttk.Label(manual_frame, text="Control (hex):").grid(row=0, column=0, sticky=tk.W)
        self.manual_control = tk.StringVar(value="0x10")
        ttk.Entry(manual_frame, textvariable=self.manual_control, width=10).grid(row=0, column=1, sticky=tk.W)

        ttk.Label(manual_frame, text="DAT1:").grid(row=1, column=0, sticky=tk.W)
        self.manual_dat1 = tk.StringVar(value="1")
        ttk.Entry(manual_frame, textvariable=self.manual_dat1, width=10).grid(row=1, column=1, sticky=tk.W)

        ttk.Label(manual_frame, text="DAT0:").grid(row=2, column=0, sticky=tk.W)
        self.manual_dat0 = tk.StringVar(value="50")
        ttk.Entry(manual_frame, textvariable=self.manual_dat0, width=10).grid(row=2, column=1, sticky=tk.W)

        ttk.Label(manual_frame, text="DEC:").grid(row=3, column=0, sticky=tk.W)
        self.manual_dec = tk.StringVar(value="0")
        ttk.Entry(manual_frame, textvariable=self.manual_dec, width=10).grid(row=3, column=1, sticky=tk.W)

        ttk.Button(manual_frame, text="Send Manual Packet",
                   command=self.send_manual_packet).grid(row=4, column=0, columnspan=2, pady=5)

        # Log display
        log_frame = ttk.LabelFrame(content_frame, text="Packet Log & Analysis", padding="5")
        log_frame.grid(row=0, column=1, sticky=(tk.W, tk.E, tk.N, tk.S))
        log_frame.columnconfigure(0, weight=1)
        log_frame.rowconfigure(0, weight=1)

        # Create notebook for different views
        notebook = ttk.Notebook(log_frame)
        notebook.grid(row=0, column=0, sticky=(tk.W, tk.E, tk.N, tk.S))

        # All packets log
        all_tab = ttk.Frame(notebook)
        notebook.add(all_tab, text="All Packets")
        self.all_log = scrolledtext.ScrolledText(all_tab, width=60, height=25, font=("Courier", 9))
        self.all_log.grid(row=0, column=0, sticky=(tk.W, tk.E, tk.N, tk.S))
        all_tab.columnconfigure(0, weight=1)
        all_tab.rowconfigure(0, weight=1)

        # Received packets only
        rx_tab = ttk.Frame(notebook)
        notebook.add(rx_tab, text="Received Only")
        self.rx_log = scrolledtext.ScrolledText(rx_tab, width=60, height=25, font=("Courier", 9))
        self.rx_log.grid(row=0, column=0, sticky=(tk.W, tk.E, tk.N, tk.S))
        rx_tab.columnconfigure(0, weight=1)
        rx_tab.rowconfigure(0, weight=1)

        # Sent packets only
        tx_tab = ttk.Frame(notebook)
        notebook.add(tx_tab, text="Sent Only")
        self.tx_log = scrolledtext.ScrolledText(tx_tab, width=60, height=25, font=("Courier", 9))
        self.tx_log.grid(row=0, column=0, sticky=(tk.W, tk.E, tk.N, tk.S))
        tx_tab.columnconfigure(0, weight=1)
        tx_tab.rowconfigure(0, weight=1)

        # Control buttons
        btn_frame = ttk.Frame(log_frame)
        btn_frame.grid(row=1, column=0, pady=5)
        ttk.Button(btn_frame, text="Clear Logs", command=self.clear_logs).grid(row=0, column=0, padx=5)
        ttk.Button(btn_frame, text="Export", command=self.export_logs).grid(row=0, column=1, padx=5)

        # Initialize
        self.refresh_ports()
        self.log_all("🚀 MARV Phase 0 Testing Interface Ready")
        self.log_all("📋 Instructions:")
        self.log_all("  1. Upload Phase0.ino to your ESP32")
        self.log_all("  2. Connect to the correct COM port")
        self.log_all("  3. Use GPIO command buttons to test")
        self.log_all("  4. Watch for packet reception/forwarding")

    def refresh_ports(self):
        ports = [port.device for port in serial.tools.list_ports.comports()]
        self.port_combo['values'] = ports
        if ports:
            self.port_combo.set(ports[0])

    def toggle_connection(self):
        if self.serial_port and self.serial_port.is_open:
            self.disconnect()
        else:
            self.connect()

    def connect(self):
        try:
            port = self.port_var.get()
            if not port:
                messagebox.showerror("Error", "Please select a port")
                return

            # Try to connect
            self.serial_port = serial.Serial(port, 19200, timeout=0.1)
            time.sleep(2)  # Give ESP32 time to reset

            self.status_label.config(text="Connected", foreground="green")
            self.connect_btn.config(text="Disconnect")

            # Start reading thread
            self.running = True
            self.serial_thread = threading.Thread(target=self.read_serial, daemon=True)
            self.serial_thread.start()

            self.log_all(f"✅ Connected to {port} at 19200 baud")
            self.log_all("📡 Listening for packets...")

        except Exception as e:
            self.log_all(f"❌ Connection failed: {str(e)}")
            messagebox.showerror("Connection Error", f"Failed to connect to {port}:\n{str(e)}")

    def disconnect(self):
        self.running = False
        if self.serial_port:
            self.serial_port.close()
        self.status_label.config(text="Disconnected", foreground="red")
        self.connect_btn.config(text="Connect")
        self.log_all("🔌 Disconnected")

    def read_serial(self):
        buffer = bytearray()
        last_activity = time.time()

        while self.running:
            try:
                if self.serial_port and self.serial_port.is_open:
                    data = self.serial_port.read(10)  # Read up to 10 bytes
                    if data:
                        buffer.extend(data)
                        last_activity = time.time()

                        # Look for complete 4-byte packets
                        while len(buffer) >= 4:
                            packet_found = False

                            # Try each position in buffer for valid packet start
                            for i in range(len(buffer) - 3):
                                test_packet = SCSPacket()
                                if test_packet.from_bytes(buffer[i:i + 4]):
                                    if self.is_valid_packet(test_packet):
                                        # Found valid packet
                                        self.process_received_packet(test_packet)
                                        buffer = buffer[i + 4:]  # Remove processed bytes
                                        packet_found = True
                                        break

                            if not packet_found:
                                # No valid packet found, remove first byte
                                if len(buffer) > 0:
                                    self.log_all(f"🗑️ Discarding invalid byte: 0x{buffer[0]:02X}")
                                    buffer = buffer[1:]

                        # Prevent buffer overflow
                        if len(buffer) > 20:
                            self.log_all("⚠️ Buffer overflow, clearing")
                            buffer = bytearray()

                    # Show activity indicator
                    if time.time() - last_activity > 5:  # 5 seconds of no activity
                        # Could add periodic "listening..." message here if needed
                        last_activity = time.time()

                time.sleep(0.01)  # Small delay to prevent CPU overload

            except Exception as e:
                if self.running:
                    self.log_all(f"💥 Serial read error: {str(e)}")
                    self.root.after(0, self.disconnect)
                break

    def is_valid_packet(self, packet):
        """Validate packet structure"""
        try:
            sys_state = packet.get_system_state()
            subsystem = packet.get_subsystem_id()
            ist = packet.get_internal_state()

            # Basic range checks
            return (0 <= sys_state.value <= 3 and
                    0 <= subsystem.value <= 3 and
                    0 <= ist <= 15)
        except:
            return False

    def process_received_packet(self, packet):
        """Process a received packet"""
        self.received_packet_count += 1
        timestamp = datetime.now().strftime("%H:%M:%S.%f")[:-3]

        # Create detailed description
        detailed_desc = packet.get_detailed_description()

        # Log the received packet
        rx_msg = f"[{timestamp}] 📥 RX: {packet}"
        detail_msg = f"[{timestamp}] 📋 Details: {detailed_desc}"
        hex_msg = f"[{timestamp}] 🔍 Raw: {packet.control:02X} {packet.dat1:02X} {packet.dat0:02X} {packet.dec:02X}"

        # Update logs
        self.log_all(rx_msg)
        self.log_all(detail_msg)
        self.log_all(hex_msg)
        self.log_all("─" * 60)

        self.log_rx(rx_msg)
        self.log_rx(detail_msg)
        self.log_rx(hex_msg)
        self.log_rx("─" * 60)

        # Update UI counters
        self.root.after(0, lambda: self.rx_count_label.config(text=str(self.received_packet_count)))

    def send_snc_packet(self, sys_state, ist, dat1, dat0, dec):
        """Send an SNC packet with specified parameters"""
        packet = SCSPacket(create_control_byte(SystemState(sys_state), SubsystemID.SNC, ist), dat1, dat0, dec)
        self.send_packet(packet)

    def send_snc_navigation(self, right_speed, left_speed, direction):
        """Send SNC Navigation Control packet"""
        # direction: 0=forward, 1=backward
        packet = SCSPacket(create_control_byte(SystemState.MAZE, SubsystemID.SNC, 3), right_speed, left_speed,
                           direction)
        self.send_packet(packet)

    def send_snc_rotation(self, angle, direction):
        """Send SNC Rotation packet"""
        # direction: 2=left, 3=right
        dat1 = (angle >> 8) & 0xFF  # Upper byte
        dat0 = angle & 0xFF  # Lower byte
        packet = SCSPacket(create_control_byte(SystemState.MAZE, SubsystemID.SNC, 3), dat1, dat0, direction)
        self.send_packet(packet)

    def send_ss_packet(self, ist, sys_state_val, angle=None):
        """Send an SS packet with appropriate data"""
        sys_state = SystemState(sys_state_val)

        # Generate appropriate test data
        dat1, dat0, dec = 0, 0, 0

        if ist == 1:  # Colors
            # Default: Red, White, Blue pattern
            dat1 = 0
            dat0 = (1 << 6) | (0 << 3) | (3)  # S1=Red(1), S2=White(0), S3=Blue(3)
            dec = 0
        elif ist == 2:  # Incidence angle
            dat1 = angle if angle else 15  # Default 15 degrees
            dat0 = 0
            dec = 0

        packet = SCSPacket(create_control_byte(sys_state, SubsystemID.SS, ist), dat1, dat0, dec)
        self.send_packet(packet)

    def send_ss_colors(self, colors):
        """Send SS Colors packet with specific color pattern"""
        # colors = [sensor1, sensor2, sensor3] where 0=W, 1=R, 2=G, 3=B, 4=K
        dat1 = 0
        dat0 = (colors[0] << 6) | (colors[1] << 3) | colors[2]

        # Determine which state to use (try CAL first, then MAZE)
        current_state = SystemState.CAL  # Default to CAL for color testing
        packet = SCSPacket(create_control_byte(current_state, SubsystemID.SS, 1), dat1, dat0, 0)
        self.send_packet(packet)

    def send_mdps_packet(self, ist, sys_state_val, angle=None, distance=None):
        """Send an MDPS packet with appropriate data"""
        sys_state = SystemState(sys_state_val)

        # Generate appropriate test data
        dat1, dat0, dec = 0, 0, 0

        if ist == 0:  # vop calibration
            dat1 = 50  # Right wheel 50 mm/s
            dat0 = 50  # Left wheel 50 mm/s
            dec = 0
        elif ist == 1:  # Battery (removed in 2022, send zeros)
            dat1 = 0
            dat0 = 0
            dec = 0
        elif ist == 2:  # Rotation
            angle_val = angle if angle else 90
            dat1 = (angle_val >> 8) & 0xFF  # Upper byte
            dat0 = angle_val & 0xFF  # Lower byte
            dec = 3  # Right turn
        elif ist == 3:  # Speed
            dat1 = 45  # Right wheel 45 mm/s
            dat0 = 50  # Left wheel 50 mm/s
            dec = 0
        elif ist == 4:  # Distance or Pure Tone Response
            if sys_state == SystemState.SOS:
                # SOS state: Pure Tone Response - motor reduces speed to zero
                dat1 = 0  # Right wheel speed = 0
                dat0 = 0  # Left wheel speed = 0
                dec = 0
            else:
                # MAZE state: Distance measurement
                dist_val = distance if distance else 150
                dat1 = (dist_val >> 8) & 0xFF  # Upper byte
                dat0 = dist_val & 0xFF  # Lower byte
                dec = 0

        packet = SCSPacket(create_control_byte(sys_state, SubsystemID.MDPS, ist), dat1, dat0, dec)
        self.send_packet(packet)

    def scenario_start_system(self):
        """Scenario: Start the system with touch detection"""
        self.log_all("🚀 SCENARIO: Starting system...")
        self.send_snc_packet(0, 0, 1, 50, 0)  # SNC Touch detected in IDLE
        self.log_all("   → Sent SNC Touch (IDLE) - should transition to CAL")

    def scenario_complete_cal(self):
        """Scenario: Complete the entire CAL sequence"""
        self.log_all("🔧 SCENARIO: Complete CAL sequence...")

        # CAL sequence from state diagram
        packets = [
            ("SS End of Calibration", lambda: self.send_ss_packet(0, 1)),
            ("SS Colors (CAL)", lambda: self.send_ss_packet(1, 1)),
            ("MDPS vop Calibration", lambda: self.send_mdps_packet(0, 1)),
            ("MDPS Battery Level", lambda: self.send_mdps_packet(1, 1)),
            ("SS Colors (CAL loop)", lambda: self.send_ss_packet(1, 1)),
        ]

        def send_next_packet(index):
            if index < len(packets):
                name, func = packets[index]
                self.log_all(f"   → Sending {name}")
                func()
                # Schedule next packet after 1 second
                self.root.after(1000, lambda: send_next_packet(index + 1))
            else:
                self.log_all("   ✅ CAL sequence complete - ready for 2nd touch")

        send_next_packet(0)

    def scenario_enter_maze(self):
        """Scenario: Send second touch to enter MAZE"""
        self.log_all("🎯 SCENARIO: Entering MAZE state...")
        self.send_snc_packet(1, 0, 1, 0, 0)  # SNC Touch detected in CAL
        self.log_all("   → Sent SNC Touch (CAL) - should transition to MAZE")

    def scenario_maze_loop(self):
        """Scenario: Test a complete MAZE loop"""
        self.log_all("🔄 SCENARIO: MAZE loop test...")

        maze_packets = [
            ("SNC Pure Tone Detection", lambda: self.send_snc_packet(2, 1, 1, 0, 0)),
            ("SNC Touch Detection", lambda: self.send_snc_packet(2, 2, 1, 0, 0)),
            ("SNC Navigation Control", lambda: self.send_snc_navigation(50, 50, 0)),
            ("MDPS Distance", lambda: self.send_mdps_packet(4, 2, distance=100)),
            ("MDPS Speed", lambda: self.send_mdps_packet(3, 2)),
            ("MDPS Rotation", lambda: self.send_mdps_packet(2, 2, angle=90)),
            ("MDPS Battery", lambda: self.send_mdps_packet(1, 2)),
            ("SS Colors (MAZE)", lambda: self.send_ss_packet(1, 2)),
            ("SS Incidence Angle", lambda: self.send_ss_packet(2, 2, angle=15)),
        ]

        def send_next_maze_packet(index):
            if index < len(maze_packets):
                name, func = maze_packets[index]
                self.log_all(f"   → Sending {name}")
                func()
                # Schedule next packet after 800ms
                self.root.after(800, lambda: send_next_maze_packet(index + 1))
            else:
                self.log_all("   ✅ MAZE loop complete")

        send_next_maze_packet(0)

    def scenario_sos(self):
        """Scenario: Emergency SOS test"""
        self.log_all("🚨 SCENARIO: Emergency SOS...")
        self.send_snc_packet(2, 1, 1, 0, 0)  # Pure tone in MAZE (should trigger SOS)
        self.log_all("   → Sent Pure Tone (MAZE) - should transition to SOS")

        # After 2 seconds, send MDPS Pure Tone Response
        def send_mdps_response():
            self.log_all("   → Sending MDPS Pure Tone Response (motor stop)")
            self.send_mdps_packet(4, 3)  # MDPS Pure Tone Response in SOS

        # After 4 seconds, send second pure tone to return to MAZE
        def return_to_maze():
            self.log_all("   → Sending second Pure Tone to return to MAZE")
            self.send_snc_packet(3, 0, 1, 0, 0)  # Pure tone in SOS

        self.root.after(2000, send_mdps_response)
        self.root.after(4000, return_to_maze)

    def send_gpio_command(self, command):
        """Send a GPIO command simulation"""
        # These simulate what happens when GPIO pins are activated on ESP32
        if command == "touch":
            self.log_all("🔴 Simulating TOUCH command (GPIO 4)")
            self.log_all("   → ESP32 should detect touch and generate SNC packet")

        elif command == "tone":
            self.log_all("🎵 Simulating PURE TONE command (GPIO 2)")
            self.log_all("   → ESP32 should detect pure tone and generate SNC packet")

        elif command == "send":
            self.log_all("📤 Simulating SEND PACKET command (GPIO 15)")
            self.log_all("   → ESP32 should send current SNC packet")

    def send_test_packet(self, sys_state, subsystem, ist):
        """Send a basic test packet (legacy function)"""
        dat1, dat0, dec = 0, 0, 0

        if subsystem == SubsystemID.SS and ist == 1:  # Colors
            dat1 = 0
            dat0 = 0b001000011  # S1=Red(1), S2=White(0), S3=Blue(3)
            dec = 0
        elif subsystem == SubsystemID.MDPS and ist == 0:  # vop calibration
            dat1 = 50  # Right wheel
            dat0 = 50  # Left wheel
            dec = 0

        packet = SCSPacket(create_control_byte(sys_state, subsystem, ist), dat1, dat0, dec)
        self.send_packet(packet)
        self.send_packet(packet)

    def send_manual_packet(self):
        """Send a manually crafted packet"""
        try:
            control = int(self.manual_control.get(), 0)  # Support hex
            dat1 = int(self.manual_dat1.get())
            dat0 = int(self.manual_dat0.get())
            dec = int(self.manual_dec.get())

            packet = SCSPacket(control, dat1, dat0, dec)
            self.send_packet(packet)

        except ValueError as e:
            messagebox.showerror("Input Error", f"Invalid packet data: {str(e)}")

    def send_packet(self, packet):
        """Send a packet to the ESP32"""
        if not (self.serial_port and self.serial_port.is_open):
            messagebox.showwarning("Not Connected", "Please connect to ESP32 first")
            return

        try:
            # Send the packet
            packet_bytes = packet.to_bytes()
            self.serial_port.write(packet_bytes)
            self.serial_port.flush()

            self.transmitted_packet_count += 1
            timestamp = datetime.now().strftime("%H:%M:%S.%f")[:-3]

            # Log the transmission
            tx_msg = f"[{timestamp}] 📤 TX: {packet}"
            detail_msg = f"[{timestamp}] 📋 Details: {packet.get_detailed_description()}"
            hex_msg = f"[{timestamp}] 🔍 Sent: {packet.control:02X} {packet.dat1:02X} {packet.dat0:02X} {packet.dec:02X}"

            self.log_all(tx_msg)
            self.log_all(detail_msg)
            self.log_all(hex_msg)
            self.log_all("─" * 60)

            self.log_tx(tx_msg)
            self.log_tx(detail_msg)
            self.log_tx(hex_msg)
            self.log_tx("─" * 60)

            # Update counter
            self.tx_count_label.config(text=str(self.transmitted_packet_count))

        except Exception as e:
            self.log_all(f"💥 Send failed: {str(e)}")
            messagebox.showerror("Send Error", f"Failed to send packet: {str(e)}")

    def log_all(self, message):
        """Log to all packets view"""
        self.all_log.insert(tk.END, message + "\n")
        self.all_log.see(tk.END)

    def log_rx(self, message):
        """Log to received packets view"""
        self.rx_log.insert(tk.END, message + "\n")
        self.rx_log.see(tk.END)

    def log_tx(self, message):
        """Log to sent packets view"""
        self.tx_log.insert(tk.END, message + "\n")
        self.tx_log.see(tk.END)

    def clear_logs(self):
        """Clear all log windows"""
        self.all_log.delete(1.0, tk.END)
        self.rx_log.delete(1.0, tk.END)
        self.tx_log.delete(1.0, tk.END)
        self.log_all("🧹 Logs cleared")

    def reset_counters(self):
        """Reset packet counters"""
        self.received_packet_count = 0
        self.transmitted_packet_count = 0
        self.rx_count_label.config(text="0")
        self.tx_count_label.config(text="0")
        self.log_all("🔄 Counters reset")

    def export_logs(self):
        """Export logs to file"""
        try:
            timestamp = datetime.now().strftime("%Y%m%d_%H%M%S")
            filename = f"marv_test_log_{timestamp}.txt"

            with open(filename, 'w') as f:
                f.write("MARV ESP32 Test Log\n")
                f.write(f"Generated: {datetime.now()}\n")
                f.write("=" * 50 + "\n\n")
                f.write("ALL PACKETS:\n")
                f.write(self.all_log.get(1.0, tk.END))
                f.write("\n" + "=" * 50 + "\n")
                f.write("RECEIVED PACKETS:\n")
                f.write(self.rx_log.get(1.0, tk.END))
                f.write("\n" + "=" * 50 + "\n")
                f.write("SENT PACKETS:\n")
                f.write(self.tx_log.get(1.0, tk.END))

            self.log_all(f"💾 Log exported to {filename}")
            messagebox.showinfo("Export Complete", f"Log exported to {filename}")

        except Exception as e:
            self.log_all(f"💥 Export failed: {str(e)}")
            messagebox.showerror("Export Error", f"Failed to export: {str(e)}")


def main():
    root = tk.Tk()
    app = MARVTestInterface(root)

    def on_closing():
        app.disconnect()
        root.destroy()

    root.protocol("WM_DELETE_WINDOW", on_closing)
    root.mainloop()


if __name__ == "__main__":
    main()