#!/usr/bin/env python3
"""
Dual-Port Maze Tester
Advanced maze simulation using TWO serial ports for independent SS and MDPS emulation

This tester uses 2 serial ports:
- Port 1: SS (Sensor Subsystem) emulation
- Port 2: MDPS (Motor Drive & Power Supply) emulation

Author: ERD320 SNC Team
Date: 2025-01-18
"""

import sys
import os
sys.path.append(os.path.join(os.path.dirname(__file__), '../../Core'))

import tkinter as tk
from tkinter import ttk, scrolledtext, messagebox
import serial
import serial.tools.list_ports
import threading
import time
import queue
from datetime import datetime
from typing import Optional, Dict, List

from scs_protocol import *
from gui_framework import ColorScheme


class DualPortMazeTester:
 """Dual-port maze testing application"""

 def __init__(self):
 self.root = tk.Tk()
 self.root.title("Dual-Port Maze Tester - Independent SS & MDPS Emulation")
 self.root.geometry("1600x1000")
 self.root.configure(bg=ColorScheme.BACKGROUND)

 # Serial ports
 self.ss_port: Optional[serial.Serial] = None
 self.mdps_port: Optional[serial.Serial] = None
 self.ss_connected = False
 self.mdps_connected = False

 # Test state
 self.test_running = False
 self.message_queue = queue.Queue()
 self.packet_log = []

 # Virtual maze state
 self.maze_state = {
 'distance': 0, # cm
 'current_color': 0, # Color code
 'current_angle': 0, # degrees
 'loop_count': 0,
 'position_x': 0, # for visualization
 'position_y': 0,
 'heading': 90 # degrees (0=right, 90=up, 180=left, 270=down)
 }

 # Statistics
 self.stats = {
 'test_start_time': None,
 'ss_packets_sent': 0,
 'mdps_packets_sent': 0,
 'snc_packets_received': 0,
 'rotations_executed': 0,
 'distance_traveled': 0
 }

 self.setup_gui()

 def setup_gui(self):
 """Initialize the GUI"""
 # Configure style
 style = ttk.Style()
 style.theme_use('clam')

 # Title
 title_frame = tk.Frame(self.root, bg=ColorScheme.BACKGROUND, height=60)
 title_frame.pack(fill='x', padx=10, pady=(10, 0))
 title_frame.pack_propagate(False)

 title_label = tk.Label(title_frame, text=" Dual-Port Maze Tester - Independent SS & MDPS",
 font=('Arial', 16, 'bold'), bg=ColorScheme.BACKGROUND, fg='white')
 title_label.pack(anchor='center', pady=15)

 # Main container
 main_frame = tk.Frame(self.root, bg=ColorScheme.BACKGROUND)
 main_frame.pack(fill='both', expand=True, padx=10, pady=10)

 # Left panel - Dual serial connections
 left_frame = tk.Frame(main_frame, bg=ColorScheme.PANEL, relief='raised', bd=2)
 left_frame.pack(side='left', fill='y', padx=(0, 5))
 left_frame.configure(width=450)
 left_frame.pack_propagate(False)

 self.create_dual_connection_panel(left_frame)
 self.create_maze_control_panel(left_frame)
 self.create_statistics_panel(left_frame)

 # Right panel - Monitoring
 right_frame = tk.Frame(main_frame, bg=ColorScheme.BACKGROUND)
 right_frame.pack(side='right', fill='both', expand=True, padx=(5, 0))

 # Notebook for different views
 notebook = ttk.Notebook(right_frame)
 notebook.pack(fill='both', expand=True)

 # Packet log tab
 log_frame = tk.Frame(notebook, bg=ColorScheme.TEXT_LIGHT)
 notebook.add(log_frame, text=" Packet Monitor")
 self.create_log_panel(log_frame)

 # Maze visualization tab
 viz_frame = tk.Frame(notebook, bg=ColorScheme.TEXT_LIGHT)
 notebook.add(viz_frame, text=" Maze Visualization")
 self.create_visualization_panel(viz_frame)

 # Test progress tab
 progress_frame = tk.Frame(notebook, bg=ColorScheme.TEXT_LIGHT)
 notebook.add(progress_frame, text=" Test Progress")
 self.create_progress_panel(progress_frame)

 def create_dual_connection_panel(self, parent):
 """Create dual serial port connection panel"""
 conn_frame = tk.LabelFrame(parent, text=" Dual Serial Ports",
 font=('Arial', 11, 'bold'), bg=ColorScheme.PANEL,
 fg='white', padx=10, pady=10)
 conn_frame.pack(fill='x', padx=10, pady=10)

 # SS Port (Port 1)
 ss_label = tk.Label(conn_frame, text="SS Port (Sensors):",
 bg=ColorScheme.PANEL, fg='white',
 font=('Arial', 10, 'bold'))
 ss_label.pack(anchor='w')

 self.ss_port_var = tk.StringVar()
 self.ss_port_combo = ttk.Combobox(conn_frame, textvariable=self.ss_port_var, width=35)
 self.ss_port_combo.pack(fill='x', pady=(0, 5))

 ss_btn_frame = tk.Frame(conn_frame, bg=ColorScheme.PANEL)
 ss_btn_frame.pack(fill='x', pady=(0, 10))

 self.ss_connect_btn = tk.Button(ss_btn_frame, text="Connect SS",
 command=self.toggle_ss_connection,
 bg=ColorScheme.SUCCESS, fg='white',
 font=('Arial', 9, 'bold'))
 self.ss_connect_btn.pack(side='left', padx=(0, 5))

 self.ss_status_label = tk.Label(ss_btn_frame, text=" Disconnected",
 bg=ColorScheme.PANEL, fg=ColorScheme.ERROR,
 font=('Arial', 9))
 self.ss_status_label.pack(side='right')

 # Separator
 ttk.Separator(conn_frame, orient='horizontal').pack(fill='x', pady=10)

 # MDPS Port (Port 2)
 mdps_label = tk.Label(conn_frame, text="MDPS Port (Motors):",
 bg=ColorScheme.PANEL, fg='white',
 font=('Arial', 10, 'bold'))
 mdps_label.pack(anchor='w')

 self.mdps_port_var = tk.StringVar()
 self.mdps_port_combo = ttk.Combobox(conn_frame, textvariable=self.mdps_port_var, width=35)
 self.mdps_port_combo.pack(fill='x', pady=(0, 5))

 mdps_btn_frame = tk.Frame(conn_frame, bg=ColorScheme.PANEL)
 mdps_btn_frame.pack(fill='x', pady=(0, 10))

 self.mdps_connect_btn = tk.Button(mdps_btn_frame, text="Connect MDPS",
 command=self.toggle_mdps_connection,
 bg=ColorScheme.SUCCESS, fg='white',
 font=('Arial', 9, 'bold'))
 self.mdps_connect_btn.pack(side='left', padx=(0, 5))

 self.mdps_status_label = tk.Label(mdps_btn_frame, text=" Disconnected",
 bg=ColorScheme.PANEL, fg=ColorScheme.ERROR,
 font=('Arial', 9))
 self.mdps_status_label.pack(side='right')

 # Refresh button
 refresh_btn = tk.Button(conn_frame, text=" Refresh Ports",
 command=self.refresh_ports,
 bg=ColorScheme.INFO, fg='white',
 font=('Arial', 9, 'bold'))
 refresh_btn.pack(fill='x')

 # Initial refresh
 self.refresh_ports()

 def create_maze_control_panel(self, parent):
 """Create maze test control panel"""
 control_frame = tk.LabelFrame(parent, text=" Maze Test Control",
 font=('Arial', 11, 'bold'), bg=ColorScheme.PANEL,
 fg='white', padx=10, pady=10)
 control_frame.pack(fill='both', expand=True, padx=10, pady=10)

 # Test scenario selection
 tk.Label(control_frame, text="Maze Scenario:", bg=ColorScheme.PANEL,
 fg='white', font=('Arial', 10)).pack(anchor='w')

 self.scenario_var = tk.StringVar(value="Full Maze (All Colors)")
 scenarios = [
 "Full Maze (All Colors)",
 "GREEN Lines Only",
 "BLUE Walls Only",
 "BLACK Walls Only",
 "Mixed Obstacles",
 "Custom Scenario"
 ]
 scenario_combo = ttk.Combobox(control_frame, textvariable=self.scenario_var,
 values=scenarios, width=30)
 scenario_combo.pack(fill='x', pady=(0, 10))

 # Test controls
 self.start_test_btn = tk.Button(control_frame, text=" Start Maze Test",
 command=self.start_maze_test,
 bg=ColorScheme.WARNING, fg='white',
 font=('Arial', 11, 'bold'),
 state='disabled')
 self.start_test_btn.pack(fill='x', pady=(0, 5))

 self.stop_test_btn = tk.Button(control_frame, text="⏹ Stop Test",
 command=self.stop_test,
 bg=ColorScheme.ERROR, fg='white',
 font=('Arial', 11, 'bold'),
 state='disabled')
 self.stop_test_btn.pack(fill='x', pady=(0, 10))

 # Progress bar
 tk.Label(control_frame, text="Test Progress:", bg=ColorScheme.PANEL,
 fg='white', font=('Arial', 10)).pack(anchor='w')

 self.progress_var = tk.DoubleVar()
 self.progress_bar = ttk.Progressbar(control_frame, variable=self.progress_var,
 maximum=100)
 self.progress_bar.pack(fill='x', pady=(5, 0))

 self.progress_label = tk.Label(control_frame, text="Ready to start...",
 bg=ColorScheme.PANEL, fg=ColorScheme.TEXT_LIGHT,
 font=('Arial', 9))
 self.progress_label.pack(anchor='w', pady=(5, 0))

 def create_statistics_panel(self, parent):
 """Create statistics display"""
 stats_frame = tk.LabelFrame(parent, text=" Test Statistics",
 font=('Arial', 11, 'bold'), bg=ColorScheme.PANEL,
 fg='white', padx=10, pady=10)
 stats_frame.pack(fill='x', padx=10, pady=10)

 self.stats_labels = {}

 stats_info = [
 ("Test Duration", "duration", "0.0s"),
 ("SS Packets Sent", "ss_sent", "0"),
 ("MDPS Packets Sent", "mdps_sent", "0"),
 ("SNC Responses", "snc_received", "0"),
 ("Distance Traveled", "distance", "0.00m"),
 ("Rotations", "rotations", "0")
 ]

 row = 0
 for label_text, key, default in stats_info:
 tk.Label(stats_frame, text=f"{label_text}:", bg=ColorScheme.PANEL,
 fg='white', font=('Arial', 9)).grid(row=row, column=0,
 sticky='w', pady=2)

 self.stats_labels[key] = tk.Label(stats_frame, text=default,
 bg=ColorScheme.PANEL,
 fg=ColorScheme.SUCCESS_BG,
 font=('Arial', 9, 'bold'))
 self.stats_labels[key].grid(row=row, column=1, sticky='e', pady=2)
 row += 1

 def create_log_panel(self, parent):
 """Create packet log panel"""
 # Controls
 control_frame = tk.Frame(parent, bg=ColorScheme.TEXT_LIGHT)
 control_frame.pack(fill='x', padx=10, pady=5)

 clear_btn = tk.Button(control_frame, text=" Clear Log",
 command=self.clear_log, bg='#95a5a6',
 fg='white', font=('Arial', 9, 'bold'))
 clear_btn.pack(side='left')

 save_btn = tk.Button(control_frame, text=" Save Log",
 command=self.save_log, bg=ColorScheme.INFO,
 fg='white', font=('Arial', 9, 'bold'))
 save_btn.pack(side='left', padx=(10, 0))

 # Log display
 self.log_text = scrolledtext.ScrolledText(parent, wrap='none',
 font=('Courier New', 9),
 bg=ColorScheme.BACKGROUND,
 fg=ColorScheme.TEXT_LIGHT)
 self.log_text.pack(fill='both', expand=True, padx=10, pady=(0, 10))

 def create_visualization_panel(self, parent):
 """Create maze visualization"""
 viz_container = tk.Frame(parent, bg=ColorScheme.TEXT_LIGHT)
 viz_container.pack(fill='both', expand=True, padx=10, pady=10)

 # Maze canvas
 self.maze_canvas = tk.Canvas(viz_container, bg='white',
 width=800, height=600)
 self.maze_canvas.pack(fill='both', expand=True)

 # Draw initial grid
 self.draw_maze_grid()

 # Current state display
 state_frame = tk.Frame(viz_container, bg=ColorScheme.TEXT_LIGHT)
 state_frame.pack(fill='x', pady=(10, 0))

 self.state_text = tk.Text(state_frame, height=6, font=('Courier New', 10),
 bg=ColorScheme.BACKGROUND, fg=ColorScheme.TEXT_LIGHT)
 self.state_text.pack(fill='x')
 self.update_state_display()

 def create_progress_panel(self, parent):
 """Create test progress panel"""
 progress_container = tk.Frame(parent, bg=ColorScheme.TEXT_LIGHT)
 progress_container.pack(fill='both', expand=True, padx=10, pady=10)

 self.progress_text = scrolledtext.ScrolledText(progress_container,
 font=('Courier New', 10),
 bg=ColorScheme.BACKGROUND,
 fg=ColorScheme.TEXT_LIGHT)
 self.progress_text.pack(fill='both', expand=True)

 def refresh_ports(self):
 """Refresh available serial ports"""
 ports = [port.device for port in serial.tools.list_ports.comports()]

 self.ss_port_combo['values'] = ports
 self.mdps_port_combo['values'] = ports

 if len(ports) >= 2:
 self.ss_port_combo.set(ports[0])
 self.mdps_port_combo.set(ports[1])
 elif len(ports) == 1:
 self.ss_port_combo.set(ports[0])

 self.log_message(" Serial ports refreshed", "INFO")

 def toggle_ss_connection(self):
 """Toggle SS port connection"""
 if not self.ss_connected:
 self.connect_ss()
 else:
 self.disconnect_ss()

 def toggle_mdps_connection(self):
 """Toggle MDPS port connection"""
 if not self.mdps_connected:
 self.connect_mdps()
 else:
 self.disconnect_mdps()

 def connect_ss(self):
 """Connect to SS port"""
 try:
 port = self.ss_port_var.get()
 if not port:
 messagebox.showerror("Error", "Select SS port")
 return

 self.ss_port = serial.Serial(port, 19200, timeout=1)
 self.ss_connected = True

 self.ss_status_label.config(text=" Connected", fg=ColorScheme.SUCCESS)
 self.ss_connect_btn.config(text="Disconnect SS", bg=ColorScheme.ERROR)

 self.log_message(f" SS connected to {port}", "SUCCESS")
 self.check_both_connected()

 except Exception as e:
 messagebox.showerror("SS Connection Error", str(e))

 def connect_mdps(self):
 """Connect to MDPS port"""
 try:
 port = self.mdps_port_var.get()
 if not port:
 messagebox.showerror("Error", "Select MDPS port")
 return

 self.mdps_port = serial.Serial(port, 19200, timeout=1)
 self.mdps_connected = True

 self.mdps_status_label.config(text=" Connected", fg=ColorScheme.SUCCESS)
 self.mdps_connect_btn.config(text="Disconnect MDPS", bg=ColorScheme.ERROR)

 # Start monitoring thread for SNC responses
 threading.Thread(target=self.monitor_snc_responses, daemon=True).start()

 self.log_message(f" MDPS connected to {port}", "SUCCESS")
 self.check_both_connected()

 except Exception as e:
 messagebox.showerror("MDPS Connection Error", str(e))

 def disconnect_ss(self):
 """Disconnect SS port"""
 if self.ss_port:
 self.ss_port.close()
 self.ss_port = None

 self.ss_connected = False
 self.ss_status_label.config(text=" Disconnected", fg=ColorScheme.ERROR)
 self.ss_connect_btn.config(text="Connect SS", bg=ColorScheme.SUCCESS)
 self.start_test_btn.config(state='disabled')

 def disconnect_mdps(self):
 """Disconnect MDPS port"""
 if self.mdps_port:
 self.mdps_port.close()
 self.mdps_port = None

 self.mdps_connected = False
 self.mdps_status_label.config(text=" Disconnected", fg=ColorScheme.ERROR)
 self.mdps_connect_btn.config(text="Connect MDPS", bg=ColorScheme.SUCCESS)
 self.start_test_btn.config(state='disabled')

 def check_both_connected(self):
 """Check if both ports are connected and enable start button"""
 if self.ss_connected and self.mdps_connected:
 self.start_test_btn.config(state='normal')
 self.log_message(" Both ports connected - ready to test!", "SUCCESS")

 def monitor_snc_responses(self):
 """Monitor SNC responses from MDPS port"""
 buffer = bytearray()

 while self.mdps_connected:
 try:
 if self.mdps_port and self.mdps_port.in_waiting > 0:
 data = self.mdps_port.read(self.mdps_port.in_waiting)
 buffer.extend(data)

 while len(buffer) >= 4:
 packet_bytes = buffer[:4]
 buffer = buffer[4:]

 packet = SCSPacket.from_bytes(packet_bytes)
 self.handle_snc_response(packet)

 time.sleep(0.01)

 except Exception as e:
 if self.mdps_connected:
 self.log_message(f" Monitor error: {str(e)}", "ERROR")
 break

 def handle_snc_response(self, packet: SCSPacket):
 """Handle response from SNC"""
 timestamp = datetime.now().strftime("%H:%M:%S.%f")[:-3]
 self.stats['snc_packets_received'] += 1

 log_line = f"{timestamp} || {self.stats['snc_packets_received']:3} || RX-SNC || {packet}"
 self.log_message(log_line, "RECEIVED")

 # Update statistics
 self.update_statistics()

 # Process SNC commands
 sys_state, subsystem, ist = parse_control_byte(packet.control)

 if sys_state == SystemState.MAZE and subsystem == SubsystemID.SNC:
 if ist == 1: # Rotation request
 self.stats['rotations_executed'] += 1
 angle = (packet.dat1 << 8) | packet.dat0
 direction = "RIGHT" if packet.dec == 2 else "LEFT"
 self.log_progress(f" SNC requesting {angle/10:.1f}° {direction} rotation")

 def start_maze_test(self):
 """Start maze test"""
 if not (self.ss_connected and self.mdps_connected):
 messagebox.showerror("Error", "Connect both ports first")
 return

 self.test_running = True
 self.stats['test_start_time'] = time.time()
 self.stats['ss_packets_sent'] = 0
 self.stats['mdps_packets_sent'] = 0
 self.stats['snc_packets_received'] = 0

 self.start_test_btn.config(state='disabled')
 self.stop_test_btn.config(state='normal')

 self.log_message(" Starting dual-port maze test...", "INFO")
 self.log_progress("=== MAZE TEST STARTED ===")

 # Start test thread
 threading.Thread(target=self.run_maze_test, daemon=True).start()

 def run_maze_test(self):
 """Execute maze test scenario"""
 try:
 # Phase 1: IDLE
 self.log_progress("Phase 1: IDLE Connection")
 mdps_pkt = make_idle_hub_packet()
 self.send_mdps_packet(mdps_pkt, "HUB: Initial contact")
 time.sleep(1.0)

 # Phase 2: CAL
 self.log_progress("Phase 2: Calibration")
 self.send_ss_packet(make_cal_ss_packet(0), "SS: CAL start")
 time.sleep(0.1)
 self.send_mdps_packet(make_cal_mdps_packet(0), "MDPS: CAL start")
 time.sleep(0.1)
 self.send_mdps_packet(make_cal_mdps_packet(1), "MDPS: Rotation CAL")
 time.sleep(0.1)
 self.send_ss_packet(make_cal_ss_packet(1), "SS: CAL complete")
 time.sleep(2.0)

 # Phase 3: MAZE continuous loop
 self.log_progress("Phase 3: MAZE State - Continuous Navigation")
 self.execute_maze_navigation()

 except Exception as e:
 self.log_message(f" Test error: {str(e)}", "ERROR")
 finally:
 if self.test_running:
 self.stop_test()

 def execute_maze_navigation(self):
 """Execute maze navigation loop"""
 loop_count = 0
 max_loops = 100

 while self.test_running and loop_count < max_loops:
 loop_count += 1
 self.maze_state['loop_count'] = loop_count
 self.progress_var.set((loop_count / max_loops) * 100)

 # Send MDPS packets
 self.send_mdps_packet(make_maze_mdps_packet(1, 90, 0, 0), "MDPS:1 Stop/Rotate")
 time.sleep(0.05)
 self.send_mdps_packet(make_maze_mdps_packet(2, 0, 0, 0), "MDPS:2 Confirm")
 time.sleep(0.05)
 self.send_mdps_packet(make_maze_mdps_packet(3, 10, 10, 0), "MDPS:3 Forward")
 time.sleep(0.05)

 # Update distance
 self.maze_state['distance'] += 2
 dist_m = self.maze_state['distance'] // 100
 dist_cm = self.maze_state['distance'] % 100
 self.send_mdps_packet(make_maze_mdps_packet(4, dist_m, dist_cm, 0),
 f"MDPS:4 Distance={dist_m}.{dist_cm:02d}m")
 time.sleep(0.05)

 # Send SS packets with virtual maze events
 color, angle = self.get_virtual_maze_state(loop_count)
 self.maze_state['current_color'] = color
 self.maze_state['current_angle'] = angle

 self.send_ss_packet(make_maze_ss_color_packet(color), f"SS:1 Color={color}")
 time.sleep(0.05)
 self.send_ss_packet(make_maze_ss_angle_packet(angle), f"SS:2 Angle={angle}°")
 time.sleep(0.4) # Wait for SNC response

 # Update visualization
 self.update_maze_visualization()
 self.update_state_display()

 # Log events
 if loop_count % 10 == 0:
 self.log_progress(f"Loop {loop_count}: dist={self.maze_state['distance']}cm, color={color}")

 self.log_progress("=== MAZE TEST COMPLETED ===")
 self.progress_var.set(100)

 def get_virtual_maze_state(self, loop_count):
 """Get virtual maze color and angle for given loop"""
 # Simulate different maze events
 if loop_count == 10:
 return COLOR_S2_GREEN, 22 # Green line
 elif loop_count == 25:
 return COLOR_S2_BLUE, 30 # Blue wall
 elif loop_count == 40:
 return COLOR_S2_GREEN, 35 # Green line
 elif loop_count == 50:
 return COLOR_S1_GREEN, 0 # Steep green (edge sensor)
 elif loop_count == 60:
 return COLOR_S2_BLACK, 28 # Black wall
 elif loop_count == 70:
 return COLOR_S2_GREEN, 8 # Small angle green
 elif loop_count == 98:
 return COLOR_ALL_RED, 1 # End of maze
 else:
 return COLOR_ALL_WHITE, 0 # White surface

 def send_ss_packet(self, packet: SCSPacket, description: str):
 """Send packet via SS port"""
 if self.ss_port:
 self.ss_port.write(packet.to_bytes())
 self.stats['ss_packets_sent'] += 1
 timestamp = datetime.now().strftime("%H:%M:%S.%f")[:-3]
 log_line = f"{timestamp} || {self.stats['ss_packets_sent']:3} || TX-SS || {packet} || {description}"
 self.log_message(log_line, "SENT")

 def send_mdps_packet(self, packet: SCSPacket, description: str):
 """Send packet via MDPS port"""
 if self.mdps_port:
 self.mdps_port.write(packet.to_bytes())
 self.stats['mdps_packets_sent'] += 1
 timestamp = datetime.now().strftime("%H:%M:%S.%f")[:-3]
 log_line = f"{timestamp} || {self.stats['mdps_packets_sent']:3} || TX-MDPS || {packet} || {description}"
 self.log_message(log_line, "SENT")

 def stop_test(self):
 """Stop test"""
 self.test_running = False
 self.start_test_btn.config(state='normal' if (self.ss_connected and self.mdps_connected) else 'disabled')
 self.stop_test_btn.config(state='disabled')
 self.log_message("⏹ Test stopped", "INFO")

 def draw_maze_grid(self):
 """Draw maze grid on canvas"""
 self.maze_canvas.delete("all")

 # Draw grid
 for i in range(0, 800, 40):
 self.maze_canvas.create_line(i, 0, i, 600, fill='#ddd')
 for i in range(0, 600, 40):
 self.maze_canvas.create_line(0, i, 800, i, fill='#ddd')

 # Draw robot (initial position)
 x, y = 400, 300
 self.maze_canvas.create_oval(x-15, y-15, x+15, y+15, fill='blue', tags='robot')
 self.maze_canvas.create_line(x, y, x, y-20, fill='red', width=3, tags='robot')

 def update_maze_visualization(self):
 """Update maze visualization"""
 # For simplicity, just update robot position based on distance
 # In full implementation, this would track actual movement
 self.maze_canvas.delete('robot')

 x = 400 + (self.maze_state['distance'] % 400)
 y = 300
 self.maze_canvas.create_oval(x-15, y-15, x+15, y+15, fill='blue', tags='robot')
 self.maze_canvas.create_line(x, y, x, y-20, fill='red', width=3, tags='robot')

 def update_state_display(self):
 """Update state display"""
 self.state_text.delete(1.0, tk.END)
 state_info = f"""

 VIRTUAL MAZE STATE 

 Distance: {self.maze_state['distance']:6d} cm 
 Color Code: {self.maze_state['current_color']:6d} 
 Angle: {self.maze_state['current_angle']:6d}° 
 Loop Count: {self.maze_state['loop_count']:6d} 

"""
 self.state_text.insert(1.0, state_info)

 def update_statistics(self):
 """Update statistics display"""
 if self.stats['test_start_time']:
 duration = time.time() - self.stats['test_start_time']
 self.stats_labels['duration'].config(text=f"{duration:.1f}s")

 self.stats_labels['ss_sent'].config(text=str(self.stats['ss_packets_sent']))
 self.stats_labels['mdps_sent'].config(text=str(self.stats['mdps_packets_sent']))
 self.stats_labels['snc_received'].config(text=str(self.stats['snc_packets_received']))
 self.stats_labels['distance'].config(text=f"{self.maze_state['distance']/100:.2f}m")
 self.stats_labels['rotations'].config(text=str(self.stats['rotations_executed']))

 def log_message(self, message: str, msg_type: str = "INFO"):
 """Log message to packet log"""
 color_map = {
 "SENT": ColorScheme.SENT_PKT,
 "RECEIVED": ColorScheme.RECV_PKT,
 "ERROR": ColorScheme.ERROR,
 "SUCCESS": ColorScheme.SUCCESS_BG,
 "INFO": ColorScheme.TEXT_LIGHT
 }

 color = color_map.get(msg_type, ColorScheme.TEXT_LIGHT)
 self.log_text.tag_configure(msg_type, foreground=color)
 self.log_text.insert(tk.END, f"{message}\n", msg_type)
 self.log_text.see(tk.END)

 def log_progress(self, message: str):
 """Log progress message"""
 timestamp = datetime.now().strftime("%H:%M:%S")
 self.progress_text.insert(tk.END, f"[{timestamp}] {message}\n")
 self.progress_text.see(tk.END)

 def clear_log(self):
 """Clear packet log"""
 self.log_text.delete(1.0, tk.END)
 self.log_message(" Log cleared", "INFO")

 def save_log(self):
 """Save log to file"""
 try:
 filename = f"dual_port_maze_log_{datetime.now().strftime('%Y%m%d_%H%M%S')}.txt"
 with open(filename, 'w') as f:
 f.write(self.log_text.get(1.0, tk.END))
 self.log_message(f" Log saved to {filename}", "SUCCESS")
 messagebox.showinfo("Success", f"Log saved to {filename}")
 except Exception as e:
 self.log_message(f" Save error: {str(e)}", "ERROR")

 def run(self):
 """Start application"""
 self.log_message(" Dual-Port Maze Tester initialized", "INFO")
 self.log_message(" Connect SS port (sensors) and MDPS port (motors)", "INFO")
 self.root.mainloop()


if __name__ == "__main__":
 app = DualPortMazeTester()
 app.run()
