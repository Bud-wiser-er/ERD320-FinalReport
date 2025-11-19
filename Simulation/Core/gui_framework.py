#!/usr/bin/env python3
"""
GUI Framework Module
Provides consistent GUI components for all SNC test scripts

Author: ERD320 SNC Team
Date: 2025-01-18
"""

import tkinter as tk
from tkinter import ttk, scrolledtext, messagebox
import serial
import serial.tools.list_ports
from datetime import datetime
from typing import Callable, Optional, List, Dict
import threading
import queue

from scs_protocol import SCSPacket, parse_control_byte, SystemState, SubsystemID


# ==================== COLOR SCHEME ====================

class ColorScheme:
    """Consistent color scheme for all test GUIs"""
    BACKGROUND = '#2c3e50'
    PANEL = '#34495e'
    SUCCESS = '#27ae60'
    ERROR = '#e74c3c'
    WARNING = '#e67e22'
    INFO = '#3498db'
    TEXT_LIGHT = '#ecf0f1'
    TEXT_DARK = '#2c3e50'
    SENT_PKT = '#3498db'
    RECV_PKT = '#27ae60'
    SUCCESS_BG = '#2ecc71'


# ==================== BASE TEST WINDOW ====================

class BaseTestWindow:
    """
    Base class for all test GUI windows
    Provides common serial connection, logging, and statistics functionality
    """

    def __init__(self, title: str, window_size: str = "1400x900"):
        self.root = tk.Tk()
        self.root.title(title)
        self.root.geometry(window_size)
        self.root.configure(bg=ColorScheme.BACKGROUND)

        # Serial connection state
        self.serial_port: Optional[serial.Serial] = None
        self.is_connected = False
        self.monitor_thread: Optional[threading.Thread] = None

        # Test state
        self.test_running = False
        self.message_queue = queue.Queue()
        self.packet_log: List[Dict] = []
        self.stats = {
            'packets_sent': 0,
            'packets_received': 0,
            'test_duration': 0,
            'test_start_time': None
        }

        # Callbacks (to be overridden by subclasses)
        self.on_packet_received: Optional[Callable[[SCSPacket], None]] = None
        self.on_test_start: Optional[Callable[[], None]] = None
        self.on_test_stop: Optional[Callable[[], None]] = None

        # Configure ttk style
        self._configure_style()

    def _configure_style(self):
        """Configure ttk widget styles"""
        style = ttk.Style()
        style.theme_use('clam')
        style.configure('Title.TLabel', font=('Arial', 16, 'bold'),
                       background=ColorScheme.BACKGROUND, foreground='white')
        style.configure('Header.TLabel', font=('Arial', 12, 'bold'),
                       background=ColorScheme.PANEL, foreground='white')
        style.configure('Status.TLabel', font=('Arial', 10),
                       background=ColorScheme.BACKGROUND, foreground=ColorScheme.TEXT_LIGHT)

    def create_title(self, parent, text: str, icon: str = ""):
        """Create title label"""
        title_frame = tk.Frame(parent, bg=ColorScheme.BACKGROUND, height=60)
        title_frame.pack(fill='x', padx=10, pady=(10, 0))
        title_frame.pack_propagate(False)

        title_text = f"{icon} {text}" if icon else text
        title_label = ttk.Label(title_frame, text=title_text, style='Title.TLabel')
        title_label.pack(anchor='center', pady=15)
        return title_frame

    def create_serial_connection_panel(self, parent) -> tk.LabelFrame:
        """Create serial connection panel with port selection and connect button"""
        conn_frame = tk.LabelFrame(parent, text="Serial Connection",
                                  font=('Arial', 11, 'bold'), bg=ColorScheme.PANEL,
                                  fg='white', padx=10, pady=10)

        # Port selection
        tk.Label(conn_frame, text="Serial Port:", bg=ColorScheme.PANEL,
                fg='white', font=('Arial', 10)).pack(anchor='w')

        self.port_var = tk.StringVar()
        self.port_combo = ttk.Combobox(conn_frame, textvariable=self.port_var, width=25)
        self.port_combo.pack(fill='x', pady=(0, 5))

        # Baud rate
        tk.Label(conn_frame, text="Baud Rate:", bg=ColorScheme.PANEL,
                fg='white', font=('Arial', 10)).pack(anchor='w')

        self.baud_var = tk.StringVar(value="19200")
        baud_combo = ttk.Combobox(conn_frame, textvariable=self.baud_var,
                                  values=["9600", "19200", "38400", "57600", "115200"], width=25)
        baud_combo.pack(fill='x', pady=(0, 10))

        # Connection buttons
        button_frame = tk.Frame(conn_frame, bg=ColorScheme.PANEL)
        button_frame.pack(fill='x')

        self.refresh_btn = tk.Button(button_frame, text="Refresh Ports",
                                     command=self.refresh_ports, bg=ColorScheme.INFO,
                                     fg='white', font=('Arial', 9, 'bold'))
        self.refresh_btn.pack(side='left', padx=(0, 5))

        self.connect_btn = tk.Button(button_frame, text="Connect",
                                     command=self.toggle_connection, bg=ColorScheme.SUCCESS,
                                     fg='white', font=('Arial', 9, 'bold'))
        self.connect_btn.pack(side='right')

        # Connection status
        self.status_label = tk.Label(conn_frame, text="Disconnected",
                                     bg=ColorScheme.PANEL, fg=ColorScheme.ERROR,
                                     font=('Arial', 10, 'bold'))
        self.status_label.pack(pady=(10, 0))

        # Initial port refresh
        self.refresh_ports()

        return conn_frame

    def create_packet_log_panel(self, parent) -> tk.Frame:
        """Create packet monitoring log panel"""
        log_frame = tk.Frame(parent, bg=ColorScheme.TEXT_LIGHT)

        # Log controls
        log_control_frame = tk.Frame(log_frame, bg=ColorScheme.TEXT_LIGHT)
        log_control_frame.pack(fill='x', padx=10, pady=5)

        clear_log_btn = tk.Button(log_control_frame, text="Clear Log",
                                  command=self.clear_log, bg='#95a5a6',
                                  fg='white', font=('Arial', 9, 'bold'))
        clear_log_btn.pack(side='left')

        save_log_btn = tk.Button(log_control_frame, text="Save Log",
                                command=self.save_log, bg=ColorScheme.INFO,
                                fg='white', font=('Arial', 9, 'bold'))
        save_log_btn.pack(side='left', padx=(10, 0))

        # Packet log display
        self.log_text = scrolledtext.ScrolledText(log_frame, wrap='none',
                                                  font=('Courier New', 9),
                                                  bg=ColorScheme.BACKGROUND,
                                                  fg=ColorScheme.TEXT_LIGHT,
                                                  selectbackground=ColorScheme.INFO)
        self.log_text.pack(fill='both', expand=True, padx=10, pady=(0, 10))

        return log_frame

    def create_statistics_panel(self, parent) -> tk.LabelFrame:
        """Create statistics display panel"""
        stats_frame = tk.LabelFrame(parent, text="Test Statistics",
                                   font=('Arial', 12, 'bold'),
                                   bg=ColorScheme.TEXT_LIGHT,
                                   fg=ColorScheme.TEXT_DARK, padx=10, pady=10)

        self.stats_labels = {}

        stats_info = [
            ("Test Duration", "test_duration", "0.0s"),
            ("Packets Sent", "packets_sent", "0"),
            ("Packets Received", "packets_received", "0"),
            ("Success Rate", "success_rate", "0.0%")
        ]

        row = 0
        for label_text, key, default in stats_info:
            tk.Label(stats_frame, text=f"{label_text}:", bg=ColorScheme.TEXT_LIGHT,
                    fg=ColorScheme.TEXT_DARK, font=('Arial', 10, 'bold')).grid(
                        row=row, column=0, sticky='w', padx=(0, 10), pady=2)

            self.stats_labels[key] = tk.Label(stats_frame, text=default,
                                             bg=ColorScheme.TEXT_LIGHT,
                                             fg=ColorScheme.SUCCESS,
                                             font=('Arial', 10))
            self.stats_labels[key].grid(row=row, column=1, sticky='w', pady=2)
            row += 1

        return stats_frame

    def refresh_ports(self):
        """Refresh available serial ports"""
        ports = [port.device for port in serial.tools.list_ports.comports()]
        self.port_combo['values'] = ports
        if ports:
            self.port_combo.set(ports[0])
        self.log_message("Serial ports refreshed", "INFO")

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

            self.status_label.config(text="Connected", fg=ColorScheme.SUCCESS)
            self.connect_btn.config(text="Disconnect", bg=ColorScheme.ERROR)

            # Start monitoring thread
            self.monitor_thread = threading.Thread(target=self.monitor_serial, daemon=True)
            self.monitor_thread.start()

            self.log_message(f"Connected to {port} at {baud} baud", "SUCCESS")

        except Exception as e:
            messagebox.showerror("Connection Error", f"Failed to connect: {str(e)}")
            self.log_message(f"Connection failed: {str(e)}", "ERROR")

    def disconnect_serial(self):
        """Disconnect from serial port"""
        if self.serial_port:
            self.serial_port.close()
            self.serial_port = None

        self.is_connected = False
        self.status_label.config(text="Disconnected", fg=ColorScheme.ERROR)
        self.connect_btn.config(text="Connect", bg=ColorScheme.SUCCESS)

        if self.test_running:
            self.stop_test()

        self.log_message("Disconnected from serial port", "INFO")

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

                        packet = SCSPacket.from_bytes(packet_bytes)
                        self.handle_received_packet(packet)

                import time
                time.sleep(0.01)  # Small delay to prevent excessive CPU usage

            except Exception as e:
                if self.is_connected:
                    self.log_message(f"Serial monitoring error: {str(e)}", "ERROR")
                break

    def handle_received_packet(self, packet: SCSPacket):
        """Handle received packet from SNC"""
        timestamp = datetime.now().strftime("%H:%M:%S.%f")[:-3]
        self.stats['packets_received'] += 1

        # Log the received packet
        log_line = f"{timestamp} || {self.stats['packets_received']:3} || RECEIVED || {packet}"
        self.log_message(log_line, "RECEIVED")

        # Update statistics
        self.update_statistics()

        # Call custom callback if defined
        if self.on_packet_received:
            self.on_packet_received(packet)

        # Queue packet for processing
        self.message_queue.put(('received_packet', packet))

    def send_packet(self, packet: SCSPacket, description: str = ""):
        """Send packet to SNC"""
        if not self.is_connected or not self.serial_port:
            self.log_message("Cannot send: not connected", "ERROR")
            return False

        try:
            packet_bytes = packet.to_bytes()
            self.serial_port.write(packet_bytes)

            timestamp = datetime.now().strftime("%H:%M:%S.%f")[:-3]
            self.stats['packets_sent'] += 1

            # Log the sent packet
            log_line = f"{timestamp} || {self.stats['packets_sent']:3} || SENT     || {packet}"
            if description:
                log_line += f" || {description}"

            self.log_message(log_line, "SENT")
            self.update_statistics()

            return True

        except Exception as e:
            self.log_message(f"Send error: {str(e)}", "ERROR")
            return False

    def log_message(self, message: str, msg_type: str = "INFO"):
        """Log a message to the display"""
        timestamp = datetime.now().strftime("%H:%M:%S.%f")[:-3]

        # Color coding based on message type
        color_map = {
            "SENT": ColorScheme.SENT_PKT,
            "RECEIVED": ColorScheme.RECV_PKT,
            "ERROR": ColorScheme.ERROR,
            "SUCCESS": ColorScheme.SUCCESS_BG,
            "INFO": ColorScheme.TEXT_LIGHT,
            "WARNING": ColorScheme.WARNING
        }

        color = color_map.get(msg_type, ColorScheme.TEXT_LIGHT)

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
        self.log_message("Log cleared", "INFO")

    def save_log(self):
        """Save the packet log to file"""
        try:
            filename = f"test_log_{datetime.now().strftime('%Y%m%d_%H%M%S')}.txt"

            with open(filename, 'w', encoding='utf-8') as f:
                f.write(f"{self.root.title()}\n")
                f.write("=" * 50 + "\n")
                f.write(f"Generated: {datetime.now().strftime('%Y-%m-%d %H:%M:%S')}\n")
                f.write("=" * 50 + "\n\n")

                for entry in self.packet_log:
                    f.write(f"[{entry['timestamp']}] [{entry['type']}] {entry['message']}\n")

            self.log_message(f"Log saved to {filename}", "SUCCESS")
            messagebox.showinfo("Success", f"Log saved to {filename}")

        except Exception as e:
            self.log_message(f"Save error: {str(e)}", "ERROR")
            messagebox.showerror("Error", f"Failed to save log: {str(e)}")

    def update_statistics(self):
        """Update the statistics display"""
        import time

        if self.stats['test_start_time']:
            duration = time.time() - self.stats['test_start_time']
            self.stats_labels['test_duration'].config(text=f"{duration:.1f}s")
        else:
            self.stats_labels['test_duration'].config(text="0.0s")

        self.stats_labels['packets_sent'].config(text=str(self.stats['packets_sent']))
        self.stats_labels['packets_received'].config(text=str(self.stats['packets_received']))

        # Calculate success rate
        if self.stats['packets_sent'] > 0:
            success_rate = (self.stats['packets_received'] / self.stats['packets_sent']) * 100
            self.stats_labels['success_rate'].config(text=f"{success_rate:.1f}%")
        else:
            self.stats_labels['success_rate'].config(text="0.0%")

    def start_test(self):
        """Start test execution (override in subclass)"""
        if not self.is_connected:
            messagebox.showerror("Error", "Please connect to serial port first")
            return

        self.test_running = True
        import time
        self.stats['test_start_time'] = time.time()
        self.stats['packets_sent'] = 0
        self.stats['packets_received'] = 0

        self.log_message("Test started", "INFO")

        if self.on_test_start:
            self.on_test_start()

    def stop_test(self):
        """Stop test execution"""
        self.test_running = False
        self.stats['test_start_time'] = None

        self.log_message("Test stopped", "INFO")

        if self.on_test_stop:
            self.on_test_stop()

    def run(self):
        """Start the application"""
        self.log_message(f"{self.root.title()} initialized", "INFO")
        self.root.mainloop()


if __name__ == "__main__":
    # Test the base window
    window = BaseTestWindow("GUI Framework Test")
    window.create_title(window.root, "Test Window")

    # Create main container
    main_frame = tk.Frame(window.root, bg=ColorScheme.BACKGROUND)
    main_frame.pack(fill='both', expand=True, padx=10, pady=10)

    # Left panel
    left_frame = tk.Frame(main_frame, bg=ColorScheme.PANEL, relief='raised', bd=2)
    left_frame.pack(side='left', fill='y', padx=(0, 5))
    left_frame.configure(width=400)
    left_frame.pack_propagate(False)

    conn_panel = window.create_serial_connection_panel(left_frame)
    conn_panel.pack(fill='x', padx=10, pady=10)

    stats_panel = window.create_statistics_panel(left_frame)
    stats_panel.pack(fill='x', padx=10, pady=10)

    # Right panel
    right_frame = tk.Frame(main_frame, bg=ColorScheme.BACKGROUND)
    right_frame.pack(side='right', fill='both', expand=True, padx=(5, 0))

    log_panel = window.create_packet_log_panel(right_frame)
    log_panel.pack(fill='both', expand=True)

    window.run()
