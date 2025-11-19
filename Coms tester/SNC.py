#!/usr/bin/env python3
"""
MARV A-Maze-Eng ESP32 Testing Interface
Simulates SS and MDPS subsystems for complete state machine testing
"""

import tkinter as tk
from tkinter import ttk, scrolledtext, messagebox
import serial
import serial.tools.list_ports
import threading
import time
import json
from datetime import datetime
from enum import Enum
import struct


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


def create_control_byte(sys_state, subsystem, ist):
    return ((sys_state.value & 0x03) << 6) | ((subsystem.value & 0x03) << 4) | (ist & 0x0F)


class StateTransition:
    def __init__(self, current_sys, current_sub, current_ist, next_sys, next_sub, next_ist, description):
        self.current_sys = current_sys
        self.current_sub = current_sub
        self.current_ist = current_ist
        self.next_sys = next_sys
        self.next_sub = next_sub
        self.next_ist = next_ist
        self.description = description


# Complete state transition table based on the state diagram
STATE_TRANSITIONS = [
    # IDLE STATE
    StateTransition(SystemState.IDLE, SubsystemID.SNC, 0, SystemState.CAL, SubsystemID.SS, 0,
                    "SNC Touch → SS Calibrate"),

    # CAL STATE - Initial Sequence
    StateTransition(SystemState.CAL, SubsystemID.SS, 0, SystemState.CAL, SubsystemID.SS, 1, "SS Calibrate → SS EoC"),
    StateTransition(SystemState.CAL, SubsystemID.SS, 1, SystemState.CAL, SubsystemID.MDPS, 0, "SS EoC → MDPS vop Cal"),
    StateTransition(SystemState.CAL, SubsystemID.MDPS, 0, SystemState.CAL, SubsystemID.MDPS, 1,
                    "MDPS vop → MDPS Battery"),

    # CAL STATE - Loop
    StateTransition(SystemState.CAL, SubsystemID.MDPS, 1, SystemState.CAL, SubsystemID.SS, 1,
                    "MDPS Battery → SS Colors"),
    StateTransition(SystemState.CAL, SubsystemID.SS, 1, SystemState.CAL, SubsystemID.SNC, 0,
                    "SS Colors → SNC Touch Check"),
    StateTransition(SystemState.CAL, SubsystemID.SNC, 0, SystemState.MAZE, SubsystemID.SNC, 1,
                    "SNC 2nd Touch → MAZE Pure Tone"),

    # MAZE STATE
    StateTransition(SystemState.MAZE, SubsystemID.SNC, 1, SystemState.MAZE, SubsystemID.SNC, 2,
                    "Pure Tone → Touch Detection"),
    StateTransition(SystemState.MAZE, SubsystemID.SNC, 2, SystemState.MAZE, SubsystemID.SNC, 3,
                    "Touch → Navigation Control"),
    StateTransition(SystemState.MAZE, SubsystemID.SNC, 3, SystemState.MAZE, SubsystemID.MDPS, 4,
                    "NAVCON → MDPS Distance"),
    StateTransition(SystemState.MAZE, SubsystemID.MDPS, 4, SystemState.MAZE, SubsystemID.MDPS, 3, "Distance → Speed"),
    StateTransition(SystemState.MAZE, SubsystemID.MDPS, 3, SystemState.MAZE, SubsystemID.MDPS, 2, "Speed → Rotation"),
    StateTransition(SystemState.MAZE, SubsystemID.MDPS, 2, SystemState.MAZE, SubsystemID.MDPS, 1, "Rotation → Battery"),
    StateTransition(SystemState.MAZE, SubsystemID.MDPS, 1, SystemState.MAZE, SubsystemID.SS, 1, "Battery → SS Colors"),
    StateTransition(SystemState.MAZE, SubsystemID.SS, 1, SystemState.MAZE, SubsystemID.SS, 2, "Colors → Incidence"),
    StateTransition(SystemState.MAZE, SubsystemID.SS, 2, SystemState.MAZE, SubsystemID.SNC, 1, "Incidence → Pure Tone"),
    StateTransition(SystemState.MAZE, SubsystemID.SS, 2, SystemState.MAZE, SubsystemID.SS, 3,
                    "Incidence → End of Maze"),

    # SOS STATE
    StateTransition(SystemState.SOS, SubsystemID.SNC, 0, SystemState.MAZE, SubsystemID.SNC, 1,
                    "SOS Pure Tone → MAZE Pure Tone"),
]


class MARVTestInterface:
    def __init__(self, root):
        self.root = root
        self.root.title("MARV ESP32 Testing Interface")
        self.root.geometry("1400x900")

        # Serial connection
        self.serial_port = None
        self.serial_thread = None
        self.running = False

        # State tracking
        self.current_state = SystemState.IDLE
        self.expected_next = None
        self.last_received_packet = None
        self.packet_history = []

        # Create UI
        self.create_widgets()

        # Start monitoring
        self.update_expected_next()

    def create_widgets(self):
        # Main frame
        main_frame = ttk.Frame(self.root, padding="10")
        main_frame.grid(row=0, column=0, sticky=(tk.W, tk.E, tk.N, tk.S))

        # Configure grid weights
        self.root.columnconfigure(0, weight=1)
        self.root.rowconfigure(0, weight=1)
        main_frame.columnconfigure(1, weight=1)
        main_frame.rowconfigure(2, weight=1)

        # Serial connection frame
        serial_frame = ttk.LabelFrame(main_frame, text="Serial Connection", padding="5")
        serial_frame.grid(row=0, column=0, columnspan=2, sticky=(tk.W, tk.E), pady=(0, 10))

        ttk.Label(serial_frame, text="Port:").grid(row=0, column=0, padx=(0, 5))
        self.port_var = tk.StringVar()
        self.port_combo = ttk.Combobox(serial_frame, textvariable=self.port_var, width=15)
        self.port_combo.grid(row=0, column=1, padx=(0, 10))

        ttk.Button(serial_frame, text="Refresh Ports", command=self.refresh_ports).grid(row=0, column=2, padx=(0, 10))
        self.connect_btn = ttk.Button(serial_frame, text="Connect", command=self.toggle_connection)
        self.connect_btn.grid(row=0, column=3)

        self.status_label = ttk.Label(serial_frame, text="Disconnected", foreground="red")
        self.status_label.grid(row=0, column=4, padx=(10, 0))

        # State monitoring frame
        state_frame = ttk.LabelFrame(main_frame, text="State Machine Status", padding="5")
        state_frame.grid(row=1, column=0, columnspan=2, sticky=(tk.W, tk.E), pady=(0, 10))
        state_frame.columnconfigure(1, weight=1)

        ttk.Label(state_frame, text="Current State:").grid(row=0, column=0, sticky=tk.W, padx=(0, 10))
        self.current_state_label = ttk.Label(state_frame, text="IDLE", font=("Arial", 12, "bold"))
        self.current_state_label.grid(row=0, column=1, sticky=tk.W)

        ttk.Label(state_frame, text="Expected Next:").grid(row=1, column=0, sticky=tk.W, padx=(0, 10))
        self.expected_label = ttk.Label(state_frame, text="SNC Touch Detection", font=("Arial", 10))
        self.expected_label.grid(row=1, column=1, sticky=tk.W)

        ttk.Label(state_frame, text="Last Received:").grid(row=2, column=0, sticky=tk.W, padx=(0, 10))
        self.last_packet_label = ttk.Label(state_frame, text="None", font=("Arial", 10))
        self.last_packet_label.grid(row=2, column=1, sticky=tk.W)

        # Control panels
        control_frame = ttk.Frame(main_frame)
        control_frame.grid(row=2, column=0, sticky=(tk.W, tk.E, tk.N, tk.S), padx=(0, 10))
        control_frame.rowconfigure(1, weight=1)

        # SS Subsystem controls
        ss_frame = ttk.LabelFrame(control_frame, text="SS (Sensor Subsystem)", padding="5")
        ss_frame.grid(row=0, column=0, sticky=(tk.W, tk.E), pady=(0, 10))

        ttk.Button(ss_frame, text="SS End of Calibration", command=lambda: self.send_ss_packet(1, 0)).grid(row=0,
                                                                                                           column=0,
                                                                                                           padx=2,
                                                                                                           pady=2)
        ttk.Button(ss_frame, text="SS Colors (CAL)", command=lambda: self.send_ss_packet(1, 1)).grid(row=0, column=1,
                                                                                                     padx=2, pady=2)
        ttk.Button(ss_frame, text="SS Colors (MAZE)", command=lambda: self.send_ss_packet(1, 1)).grid(row=1, column=0,
                                                                                                      padx=2, pady=2)
        ttk.Button(ss_frame, text="SS Incidence", command=lambda: self.send_ss_packet(2, 2)).grid(row=1, column=1,
                                                                                                  padx=2, pady=2)
        ttk.Button(ss_frame, text="SS End of Maze", command=lambda: self.send_ss_packet(3, 3)).grid(row=2, column=0,
                                                                                                    padx=2, pady=2)

        # MDPS Subsystem controls
        mdps_frame = ttk.LabelFrame(control_frame, text="MDPS (Motor Driver Power Supply)", padding="5")
        mdps_frame.grid(row=1, column=0, sticky=(tk.W, tk.E, tk.N, tk.S))

        ttk.Button(mdps_frame, text="MDPS vop Calibration", command=lambda: self.send_mdps_packet(0, 0)).grid(row=0,
                                                                                                              column=0,
                                                                                                              padx=2,
                                                                                                              pady=2)
        ttk.Button(mdps_frame, text="MDPS Battery Level", command=lambda: self.send_mdps_packet(1, 1)).grid(row=0,
                                                                                                            column=1,
                                                                                                            padx=2,
                                                                                                            pady=2)
        ttk.Button(mdps_frame, text="MDPS Rotation", command=lambda: self.send_mdps_packet(2, 2)).grid(row=1, column=0,
                                                                                                       padx=2, pady=2)
        ttk.Button(mdps_frame, text="MDPS Speed", command=lambda: self.send_mdps_packet(3, 3)).grid(row=1, column=1,
                                                                                                    padx=2, pady=2)
        ttk.Button(mdps_frame, text="MDPS Distance", command=lambda: self.send_mdps_packet(4, 4)).grid(row=2, column=0,
                                                                                                       padx=2, pady=2)

        # Log and analysis frame
        log_frame = ttk.LabelFrame(main_frame, text="Packet Log & Analysis", padding="5")
        log_frame.grid(row=2, column=1, sticky=(tk.W, tk.E, tk.N, tk.S))
        log_frame.columnconfigure(0, weight=1)
        log_frame.rowconfigure(0, weight=1)

        # Create notebook for tabs
        notebook = ttk.Notebook(log_frame)
        notebook.grid(row=0, column=0, sticky=(tk.W, tk.E, tk.N, tk.S))

        # Packet log tab
        log_tab = ttk.Frame(notebook)
        notebook.add(log_tab, text="Packet Log")

        self.log_text = scrolledtext.ScrolledText(log_tab, width=60, height=25, font=("Courier", 9))
        self.log_text.grid(row=0, column=0, sticky=(tk.W, tk.E, tk.N, tk.S))
        log_tab.columnconfigure(0, weight=1)
        log_tab.rowconfigure(0, weight=1)

        # Auto-response tab
        auto_tab = ttk.Frame(notebook)
        notebook.add(auto_tab, text="Auto Response")

        self.auto_response_var = tk.BooleanVar(value=False)
        ttk.Checkbutton(auto_tab, text="Enable Auto Response", variable=self.auto_response_var).grid(row=0, column=0,
                                                                                                     sticky=tk.W,
                                                                                                     pady=5)

        ttk.Label(auto_tab,
                  text="Auto response will automatically send the next expected packet\nafter receiving an SNC packet.").grid(
            row=1, column=0, sticky=tk.W, pady=10)

        # Manual packet sender
        manual_frame = ttk.LabelFrame(auto_tab, text="Manual Packet Sender", padding="5")
        manual_frame.grid(row=2, column=0, sticky=(tk.W, tk.E), pady=10)

        ttk.Label(manual_frame, text="Control:").grid(row=0, column=0)
        self.manual_control = tk.StringVar(value="0x00")
        ttk.Entry(manual_frame, textvariable=self.manual_control, width=8).grid(row=0, column=1, padx=5)

        ttk.Label(manual_frame, text="DAT1:").grid(row=0, column=2, padx=(10, 0))
        self.manual_dat1 = tk.StringVar(value="0")
        ttk.Entry(manual_frame, textvariable=self.manual_dat1, width=8).grid(row=0, column=3, padx=5)

        ttk.Label(manual_frame, text="DAT0:").grid(row=1, column=0)
        self.manual_dat0 = tk.StringVar(value="0")
        ttk.Entry(manual_frame, textvariable=self.manual_dat0, width=8).grid(row=1, column=1, padx=5)

        ttk.Label(manual_frame, text="DEC:").grid(row=1, column=2, padx=(10, 0))
        self.manual_dec = tk.StringVar(value="0")
        ttk.Entry(manual_frame, textvariable=self.manual_dec, width=8).grid(row=1, column=3, padx=5)

        ttk.Button(manual_frame, text="Send Manual Packet", command=self.send_manual_packet).grid(row=2, column=0,
                                                                                                  columnspan=4, pady=10)

        # Clear log button
        ttk.Button(log_frame, text="Clear Log", command=self.clear_log).grid(row=1, column=0, pady=5)

        # Initialize
        self.refresh_ports()
        self.log_message("MARV Testing Interface Started")
        self.log_message("Connect to ESP32 and start testing!")

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

            self.serial_port = serial.Serial(port, 19200, timeout=0.1)
            self.status_label.config(text="Connected", foreground="green")
            self.connect_btn.config(text="Disconnect")

            # Start reading thread
            self.running = True
            self.serial_thread = threading.Thread(target=self.read_serial, daemon=True)
            self.serial_thread.start()

            self.log_message(f"Connected to {port}")

        except Exception as e:
            messagebox.showerror("Connection Error", f"Failed to connect: {str(e)}")

    def disconnect(self):
        self.running = False
        if self.serial_port:
            self.serial_port.close()
        self.status_label.config(text="Disconnected", foreground="red")
        self.connect_btn.config(text="Connect")
        self.log_message("Disconnected")

    def read_serial(self):
        buffer = bytearray()

        while self.running:
            try:
                if self.serial_port and self.serial_port.is_open:
                    data = self.serial_port.read(1)
                    if data:
                        buffer.extend(data)

                        # Look for complete 4-byte packets
                        while len(buffer) >= 4:
                            # Try to find a valid packet
                            packet_found = False
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
                                buffer = buffer[1:]  # Remove one byte and try again

                        # Keep buffer size reasonable
                        if len(buffer) > 16:
                            buffer = buffer[-8:]

                time.sleep(0.01)

            except Exception as e:
                if self.running:
                    self.log_message(f"Serial read error: {str(e)}")
                    self.disconnect()
                break

    def is_valid_packet(self, packet):
        sys_state = packet.get_system_state()
        subsystem = packet.get_subsystem_id()
        ist = packet.get_internal_state()

        # Basic validation
        return (sys_state.value <= 3 and
                subsystem.value <= 3 and
                ist <= 15)

    def process_received_packet(self, packet):
        self.last_received_packet = packet
        timestamp = datetime.now().strftime("%H:%M:%S.%f")[:-3]

        # Check if packet matches expectation
        is_expected = self.check_packet_expectation(packet)
        status = "✓ EXPECTED" if is_expected else "⚠ UNEXPECTED"

        # Log packet
        log_msg = f"[{timestamp}] RX {status}: {packet}"
        self.log_message(log_msg)

        # Update UI in main thread
        self.root.after(0, self.update_ui_after_packet, packet, is_expected)

        # Auto-respond if enabled and packet is from SNC
        if (self.auto_response_var.get() and
                packet.get_subsystem_id() == SubsystemID.SNC):
            self.root.after(100, self.auto_respond, packet)  # Small delay

    def check_packet_expectation(self, packet):
        if not self.expected_next:
            return True  # No specific expectation

        exp_sys, exp_sub, exp_ist = self.expected_next
        return (packet.get_system_state() == exp_sys and
                packet.get_subsystem_id() == exp_sub and
                packet.get_internal_state() == exp_ist)

    def update_ui_after_packet(self, packet, is_expected):
        # Update current state
        self.current_state = packet.get_system_state()
        self.current_state_label.config(text=self.current_state.name)

        # Update last packet display
        status = "✓" if is_expected else "⚠"
        self.last_packet_label.config(text=f"{status} {packet}")

        # Update expected next
        self.update_expected_next()

    def update_expected_next(self):
        # Find what should come next based on current state
        next_options = []
        for transition in STATE_TRANSITIONS:
            if transition.current_sys == self.current_state:
                next_options.append(
                    (transition.next_sys, transition.next_sub, transition.next_ist, transition.description))

        if next_options:
            # Show the most likely next transition
            if len(next_options) == 1:
                next_sys, next_sub, next_ist, desc = next_options[0]
                self.expected_next = (next_sys, next_sub, next_ist)
                self.expected_label.config(text=f"{next_sub.name}:IST{next_ist} - {desc}")
            else:
                # Multiple options - show general expectation
                self.expected_label.config(text="Multiple possibilities - see state diagram")
                self.expected_next = None
        else:
            self.expected_label.config(text="Unknown state transition")
            self.expected_next = None

    def auto_respond(self, received_packet):
        if not self.auto_response_var.get():
            return

        # Find appropriate response based on the received SNC packet
        response_packet = self.generate_response_packet(received_packet)
        if response_packet:
            self.send_packet(response_packet)
            self.log_message(f"[AUTO] Sent response: {response_packet}")

    def generate_response_packet(self, snc_packet):
        sys_state = snc_packet.get_system_state()
        ist = snc_packet.get_internal_state()

        # Generate appropriate response based on state diagram
        if sys_state == SystemState.CAL and ist == 0:
            # SNC touch in CAL -> Send SS End of Calibration
            return SCSPacket(create_control_byte(SystemState.CAL, SubsystemID.SS, 1), 0, 0, 0)
        elif sys_state == SystemState.MAZE and ist == 1:
            # SNC pure tone in MAZE -> Could be various responses
            return SCSPacket(create_control_byte(SystemState.MAZE, SubsystemID.SS, 1), 0, 0, 0)

        return None

    def send_ss_packet(self, ist, sys_state_val=None):
        if sys_state_val is None:
            sys_state = self.current_state
        else:
            sys_state = SystemState(sys_state_val)

        packet = SCSPacket(create_control_byte(sys_state, SubsystemID.SS, ist), 0, 0, 0)
        self.send_packet(packet)

    def send_mdps_packet(self, ist, sys_state_val=None):
        if sys_state_val is None:
            sys_state = self.current_state
        else:
            sys_state = SystemState(sys_state_val)

        packet = SCSPacket(create_control_byte(sys_state, SubsystemID.MDPS, ist), 0, 0, 0)
        self.send_packet(packet)

    def send_manual_packet(self):
        try:
            control = int(self.manual_control.get(), 0)  # Support hex with 0x prefix
            dat1 = int(self.manual_dat1.get())
            dat0 = int(self.manual_dat0.get())
            dec = int(self.manual_dec.get())

            packet = SCSPacket(control, dat1, dat0, dec)
            self.send_packet(packet)

        except ValueError as e:
            messagebox.showerror("Input Error", f"Invalid packet data: {str(e)}")

    def send_packet(self, packet):
        if self.serial_port and self.serial_port.is_open:
            self.serial_port.write(packet.to_bytes())
            timestamp = datetime.now().strftime("%H:%M:%S.%f")[:-3]
            self.log_message(f"[{timestamp}] TX: {packet}")
        else:
            messagebox.showwarning("Not Connected", "Please connect to ESP32 first")

    def log_message(self, message):
        self.log_text.insert(tk.END, message + "\n")
        self.log_text.see(tk.END)

    def clear_log(self):
        self.log_text.delete(1.0, tk.END)


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