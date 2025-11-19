#!/usr/bin/env python3
"""
MAZE-SS Command Test Script
Comprehensive testing for MAZE state SS (Sensor Subsystem) commands

Commands Tested:
- MAZE:SS:1 - Color data packets (ALL color combinations)
- MAZE:SS:2 - Angle data packets (0-90°)
- MAZE:SS:3 - End-of-maze signal

Tests all 3 sensors × 5 colors = comprehensive coverage
Validates color encoding, angle measurements, and EOM detection

Author: ERD320 SNC Team
Date: 2025-01-18
"""

import sys
import os
sys.path.append(os.path.join(os.path.dirname(__file__), '../../Core'))

import tkinter as tk
from tkinter import ttk, scrolledtext, messagebox
import threading
import time
from datetime import datetime

from gui_framework import BaseTestWindow, ColorScheme
from scs_protocol import *


class MAZESSCommandTester(BaseTestWindow):
 """MAZE-SS command testing GUI"""

 def __init__(self):
 super().__init__("MAZE-SS Command Tester", "1600x950")

 # Color test matrix
 self.all_color_tests = {
 'WHITE': (COLOR_ALL_WHITE, "All sensors WHITE (normal surface)"),
 'S2_RED': (COLOR_S2_RED, "S2 = RED (center sensor)"),
 'S2_GREEN': (COLOR_S2_GREEN, "S2 = GREEN (center sensor)"),
 'S2_BLUE': (COLOR_S2_BLUE, "S2 = BLUE wall (center)"),
 'S2_BLACK': (COLOR_S2_BLACK, "S2 = BLACK wall (center)"),
 'S1_GREEN': (COLOR_S1_GREEN, "S1 = GREEN (left edge)"),
 'S3_GREEN': (COLOR_S3_GREEN, "S3 = GREEN (right edge)"),
 'ALL_RED': (COLOR_ALL_RED, "All sensors RED (end-of-maze)"),
 }

 # Test completion tracking
 self.tests_completed = set()

 self.setup_test_gui()

 def setup_test_gui(self):
 """Setup the test GUI"""
 # Title
 self.create_title(self.root, "MAZE-SS Command Tester - Sensor Data Validation", "")

 # Main container
 main_frame = tk.Frame(self.root, bg=ColorScheme.BACKGROUND)
 main_frame.pack(fill='both', expand=True, padx=10, pady=10)

 # Left panel - Controls
 left_frame = tk.Frame(main_frame, bg=ColorScheme.PANEL, relief='raised', bd=2)
 left_frame.pack(side='left', fill='y', padx=(0, 5))
 left_frame.configure(width=500)
 left_frame.pack_propagate(False)

 # Serial connection
 conn_panel = self.create_serial_connection_panel(left_frame)
 conn_panel.pack(fill='x', padx=10, pady=10)

 # Command controls
 self.create_command_panel(left_frame)

 # Color palette
 self.create_color_palette(left_frame)

 # Statistics
 stats_panel = self.create_statistics_panel(left_frame)
 stats_panel.pack(fill='x', padx=10, pady=10)

 # Right panel - Monitoring
 right_frame = tk.Frame(main_frame, bg=ColorScheme.BACKGROUND)
 right_frame.pack(side='right', fill='both', expand=True, padx=(5, 0))

 # Notebook
 notebook = ttk.Notebook(right_frame)
 notebook.pack(fill='both', expand=True)

 # Color Matrix tab
 matrix_frame = tk.Frame(notebook, bg=ColorScheme.TEXT_LIGHT)
 notebook.add(matrix_frame, text=" Color Matrix")
 self.create_color_matrix_panel(matrix_frame)

 # Sensor Visualization tab
 viz_frame = tk.Frame(notebook, bg=ColorScheme.TEXT_LIGHT)
 notebook.add(viz_frame, text=" Sensor View")
 self.create_sensor_viz_panel(viz_frame)

 # Packet Log tab
 log_frame = tk.Frame(notebook, bg=ColorScheme.TEXT_LIGHT)
 notebook.add(log_frame, text=" Packet Monitor")
 log_panel = self.create_packet_log_panel(log_frame)

 # Encoding Reference tab
 ref_frame = tk.Frame(notebook, bg=ColorScheme.TEXT_LIGHT)
 notebook.add(ref_frame, text=" Encoding Reference")
 self.create_reference_panel(ref_frame)

 def create_command_panel(self, parent):
 """Create SS command panel"""
 cmd_frame = tk.LabelFrame(parent, text=" SS Commands",
 font=('Arial', 11, 'bold'), bg=ColorScheme.PANEL,
 fg='white', padx=10, pady=10)
 cmd_frame.pack(fill='x', padx=10, pady=10)

 # Color selector
 tk.Label(cmd_frame, text="Color Combination:", bg=ColorScheme.PANEL,
 fg='white', font=('Arial', 9, 'bold')).pack(anchor='w')

 self.color_var = tk.StringVar(value="S2_GREEN")
 color_combo = ttk.Combobox(cmd_frame, textvariable=self.color_var,
 values=list(self.all_color_tests.keys()), width=25)
 color_combo.pack(fill='x', pady=(0, 5))

 self.color_desc_label = tk.Label(cmd_frame,
 text=self.all_color_tests['S2_GREEN'][1],
 bg=ColorScheme.PANEL, fg=ColorScheme.TEXT_LIGHT,
 font=('Arial', 8, 'italic'), wraplength=400)
 self.color_desc_label.pack(anchor='w', pady=(0, 10))

 color_combo.bind('<<ComboboxSelected>>', self.on_color_selected)

 # Send color button
 self.send_color_btn = tk.Button(cmd_frame, text="Send SS:1 (Color Data)",
 command=self.send_color_packet,
 bg=ColorScheme.SUCCESS, fg='white',
 font=('Arial', 10, 'bold'), state='disabled')
 self.send_color_btn.pack(fill='x', pady=(0, 15))

 # Angle selector
 tk.Label(cmd_frame, text="Angle (°):", bg=ColorScheme.PANEL,
 fg='white', font=('Arial', 9, 'bold')).pack(anchor='w')

 angle_frame = tk.Frame(cmd_frame, bg=ColorScheme.PANEL)
 angle_frame.pack(fill='x', pady=(0, 10))

 self.angle_var = tk.IntVar(value=0)
 angle_scale = tk.Scale(angle_frame, from_=0, to=90, orient='horizontal',
 variable=self.angle_var, bg=ColorScheme.PANEL,
 fg='white', font=('Arial', 9))
 angle_scale.pack(side='left', fill='x', expand=True)

 self.angle_label = tk.Label(angle_frame, text="0°", bg=ColorScheme.PANEL,
 fg=ColorScheme.SUCCESS_BG, font=('Arial', 12, 'bold'),
 width=6)
 self.angle_label.pack(side='right')

 angle_scale.config(command=lambda v: self.angle_label.config(text=f"{int(float(v))}°"))

 # Send angle button
 self.send_angle_btn = tk.Button(cmd_frame, text="Send SS:2 (Angle Data)",
 command=self.send_angle_packet,
 bg=ColorScheme.INFO, fg='white',
 font=('Arial', 10, 'bold'), state='disabled')
 self.send_angle_btn.pack(fill='x', pady=(0, 15))

 # End-of-maze button
 self.send_eom_btn = tk.Button(cmd_frame, text="Send SS:3 (End-of-Maze)",
 command=self.send_eom_packet,
 bg=ColorScheme.ERROR, fg='white',
 font=('Arial', 10, 'bold'), state='disabled')
 self.send_eom_btn.pack(fill='x')

 def create_color_palette(self, parent):
 """Create color quick-select palette"""
 palette_frame = tk.LabelFrame(parent, text=" Quick Color Tests",
 font=('Arial', 11, 'bold'), bg=ColorScheme.PANEL,
 fg='white', padx=10, pady=10)
 palette_frame.pack(fill='both', expand=True, padx=10, pady=10)

 # Quick test buttons
 quick_tests = [
 ("Test All WHITE", lambda: self.quick_color_test('WHITE')),
 ("Test GREEN Line (S2)", lambda: self.quick_color_test('S2_GREEN')),
 ("Test RED Line (S2)", lambda: self.quick_color_test('S2_RED')),
 ("Test BLUE Wall (S2)", lambda: self.quick_color_test('S2_BLUE')),
 ("Test BLACK Wall (S2)", lambda: self.quick_color_test('S2_BLACK')),
 ("Test Edge Sensors", self.test_edge_sensors),
 ("Test All Angles (0-90°)", self.test_all_angles),
 (" Run COMPLETE Color Matrix", self.run_complete_matrix)
 ]

 self.palette_buttons = []
 for text, command in quick_tests:
 btn = tk.Button(palette_frame, text=text, command=command,
 bg=ColorScheme.WARNING if "COMPLETE" in text else ColorScheme.INFO,
 fg='white', font=('Arial', 9, 'bold'), state='disabled')
 btn.pack(fill='x', pady=3)
 self.palette_buttons.append(btn)

 def create_color_matrix_panel(self, parent):
 """Create color test matrix"""
 matrix_container = tk.Frame(parent, bg=ColorScheme.TEXT_LIGHT)
 matrix_container.pack(fill='both', expand=True, padx=10, pady=10)

 tk.Label(matrix_container, text="Color Test Matrix - Complete Coverage",
 font=('Arial', 14, 'bold'), bg=ColorScheme.TEXT_LIGHT,
 fg=ColorScheme.TEXT_DARK).pack(anchor='w', pady=(0, 10))

 # Matrix display
 columns = ("Color", "Code", "Description", "Status")
 self.matrix_tree = ttk.Treeview(matrix_container, columns=columns,
 show='tree headings', height=15)

 self.matrix_tree.heading("#0", text="ID")
 self.matrix_tree.heading("Color", text="Color Key")
 self.matrix_tree.heading("Code", text="Byte Value")
 self.matrix_tree.heading("Description", text="Description")
 self.matrix_tree.heading("Status", text="Status")

 self.matrix_tree.column("#0", width=50)
 self.matrix_tree.column("Color", width=120)
 self.matrix_tree.column("Code", width=80)
 self.matrix_tree.column("Description", width=300)
 self.matrix_tree.column("Status", width=100)

 # Populate matrix
 for i, (color_key, (color_code, description)) in enumerate(self.all_color_tests.items(), 1):
 self.matrix_tree.insert("", "end", iid=color_key,
 text=f"#{i}",
 values=(color_key, f"0x{color_code:02X}", description, "PENDING"))

 # Scrollbar
 scrollbar = ttk.Scrollbar(matrix_container, orient='vertical',
 command=self.matrix_tree.yview)
 self.matrix_tree.configure(yscrollcommand=scrollbar.set)

 self.matrix_tree.pack(side='left', fill='both', expand=True)
 scrollbar.pack(side='right', fill='y')

 # Summary
 self.matrix_summary = tk.Label(matrix_container,
 text=f"Coverage: 0/{len(self.all_color_tests)} colors tested",
 bg=ColorScheme.TEXT_LIGHT,
 fg=ColorScheme.TEXT_DARK,
 font=('Arial', 11, 'bold'))
 self.matrix_summary.pack(pady=10)

 def create_sensor_viz_panel(self, parent):
 """Create sensor visualization"""
 viz_container = tk.Frame(parent, bg=ColorScheme.TEXT_LIGHT)
 viz_container.pack(fill='both', expand=True, padx=10, pady=10)

 tk.Label(viz_container, text="Sensor Array Visualization",
 font=('Arial', 12, 'bold'), bg=ColorScheme.TEXT_LIGHT,
 fg=ColorScheme.TEXT_DARK).pack(anchor='w', pady=(0, 10))

 # Sensor canvas
 self.sensor_canvas = tk.Canvas(viz_container, bg='white', height=250)
 self.sensor_canvas.pack(fill='x', pady=(0, 10))

 self.draw_sensor_array()

 # Current detection display
 detection_frame = tk.Frame(viz_container, bg=ColorScheme.BACKGROUND,
 relief='raised', bd=2, padx=20, pady=15)
 detection_frame.pack(fill='x')

 tk.Label(detection_frame, text="Current Detection:",
 bg=ColorScheme.BACKGROUND, fg='white',
 font=('Arial', 11, 'bold')).pack(anchor='w')

 self.detection_text = tk.Text(detection_frame, height=8, wrap='word',
 font=('Courier New', 10),
 bg=ColorScheme.BACKGROUND,
 fg=ColorScheme.TEXT_LIGHT)
 self.detection_text.pack(fill='x', pady=(5, 0))

 self.update_detection_display("WHITE", 0)

 def create_reference_panel(self, parent):
 """Create encoding reference"""
 ref_container = tk.Frame(parent, bg=ColorScheme.TEXT_LIGHT)
 ref_container.pack(fill='both', expand=True, padx=10, pady=10)

 tk.Label(ref_container, text="Color Encoding Reference",
 font=('Arial', 12, 'bold'), bg=ColorScheme.TEXT_LIGHT,
 fg=ColorScheme.TEXT_DARK).pack(anchor='w', pady=(0, 10))

 ref_text = scrolledtext.ScrolledText(ref_container,
 font=('Courier New', 9),
 bg=ColorScheme.BACKGROUND,
 fg=ColorScheme.TEXT_LIGHT,
 wrap='word')
 ref_text.pack(fill='both', expand=True)

 reference = """

 COLOR ENCODING REFERENCE 


 MAZE:SS:1 - Color Data Packet


CONTROL: (SYS=2 | SUB=3 | IST=1) = 0xB1 = 177
DAT1: 0 (reserved)
DAT0: Color encoding byte (see table below)
DEC: 0

Purpose: Report color detected by 3-sensor array (S1, S2, S3)
Update: Continuous (every control loop ~5ms)



 COLOR ENCODING FORMAT:


Byte Structure: (S3[7:6] | S2[4:3] | S1[1:0])

 Bit 7-6: S3 color (right edge sensor)
 Bit 4-3: S2 color (center sensor)
 Bit 1-0: S1 color (left edge sensor)

Color Values per Sensor:
 00 (0): WHITE - No line detected
 01 (1): RED - End marker or navigable line
 10 (2): GREEN - Junction/intersection
 11 (3): BLUE - Wall/obstacle (S2 only)
 BLACK - Wall/obstacle (special encoding)



 COMMON COLOR CODES:


Code Binary S3 S2 S1 Description

0 0b00000000 WHT WHT WHT All white (normal surface)
2 0b00000010 WHT WHT GRN Left edge GREEN
8 0b00001000 WHT RED WHT Center RED
16 0b00010000 WHT GRN WHT Center GREEN ← Most common
24 0b00011000 WHT BLU WHT Center BLUE (wall)
32 0b00100000 WHT BLK WHT Center BLACK (wall)
128 0b10000000 GRN WHT WHT Right edge GREEN
73 0b01001001 RED RED RED All RED (end-of-maze)



 MAZE:SS:2 - Angle Data Packet


CONTROL: (SYS=2 | SUB=3 | IST=2) = 0xB2 = 178
DAT1: Angle in degrees (0-90°)
DAT0: 0 (or angle fractional part)
DEC: 0 (or direction flag)

Purpose: Report line incidence angle θ_i
Range: 0° (perpendicular) to 90° (parallel)
Usage: NAVCON uses angle to determine navigation action

Angle Categories:
 θ ≤ 5°: STRAIGHT - Direct crossing
 5° < θ ≤ 45°: ALIGNMENT - Incremental correction
 θ > 45°: STEEP - Major rotation required



 MAZE:SS:3 - End-of-Maze Signal


CONTROL: (SYS=2 | SUB=3 | IST=3) = 0xB3 = 179
DAT1: 0
DAT0: 0
DEC: 0

Purpose: Signal completion of maze
Trigger: Detection of ALL_RED (0x49) after 360° rotation
Action: SNC transitions MAZE → IDLE



 DETECTION EXAMPLES:


Example 1: GREEN line at center, 35° angle
 → SS:1: DAT0 = 0x10 (16), DAT1 = 0
 → SS:2: DAT1 = 35, DAT0 = 0
 → Expected: NAVCON alignment correction

Example 2: BLUE wall at center, 25° angle
 → SS:1: DAT0 = 0x18 (24), DAT1 = 0
 → SS:2: DAT1 = 25, DAT0 = 0
 → Expected: NAVCON obstacle avoidance

Example 3: GREEN on left edge (steep approach)
 → SS:1: DAT0 = 0x02 (2), DAT1 = 0
 → SS:2: DAT1 = 0 (or edge angle)
 → Expected: NAVCON rotation

Example 4: All RED (end-of-maze)
 → SS:1: DAT0 = 0x49 (73), DAT1 = 0
 → SS:3: DAT1 = 0, DAT0 = 0
 → Expected: Stop, signal completion



 SENSOR CONSTRAINTS:


• Detection Distance: 2-10 mm above surface
• Color Discrimination: 5 colors (WHITE, RED, GREEN, BLUE, BLACK)
• Angle Measurement: ±2° accuracy
• Update Rate: 200 Hz (5 ms period)
• Ambient Light Compensation: Required
• Calibration: Performed in CAL state



 TEST COVERAGE:


Complete SS testing should validate:

 All 8 primary color combinations
 Angles 0°, 5°, 10°, 30°, 45°, 60°, 90° (category boundaries)
 Edge sensor detection (S1, S3)
 Center sensor detection (S2)
 Multi-sensor scenarios
 End-of-maze detection
 Wall avoidance (BLUE, BLACK)
 Navigable lines (RED, GREEN)


"""
 ref_text.insert(1.0, reference)

 def draw_sensor_array(self):
 """Draw sensor array visualization"""
 self.sensor_canvas.delete("all")

 # Draw robot outline
 robot_x = 400
 robot_y = 125

 self.sensor_canvas.create_rectangle(robot_x-80, robot_y-50,
 robot_x+80, robot_y+50,
 fill='#34495e', outline='black', width=2)

 # Draw sensors
 sensor_positions = [
 (robot_x-60, robot_y+70, "S1 (Left Edge)"),
 (robot_x, robot_y+70, "S2 (Center)"),
 (robot_x+60, robot_y+70, "S3 (Right Edge)")
 ]

 for x, y, label in sensor_positions:
 # Sensor circle
 self.sensor_canvas.create_oval(x-15, y-15, x+15, y+15,
 fill=ColorScheme.INFO, outline='black', width=2)

 # Label
 self.sensor_canvas.create_text(x, y+35, text=label, font=('Arial', 9))

 # Title
 self.sensor_canvas.create_text(robot_x, 30, text="3-Sensor Color Detection Array",
 font=('Arial', 12, 'bold'))

 def on_color_selected(self, event):
 """Handle color selection"""
 color_key = self.color_var.get()
 if color_key in self.all_color_tests:
 _, description = self.all_color_tests[color_key]
 self.color_desc_label.config(text=description)

 def send_color_packet(self):
 """Send color packet"""
 color_key = self.color_var.get()
 color_code, description = self.all_color_tests[color_key]

 pkt = make_maze_ss_color_packet(color_code)
 self.send_packet(pkt, f"SS:1 Color={color_key} (0x{color_code:02X})")

 # Mark as tested
 self.tests_completed.add(color_key)
 self.matrix_tree.set(color_key, "Status", " TESTED")
 self.update_matrix_summary()

 # Update visualization
 angle = self.angle_var.get()
 self.update_detection_display(color_key, angle)

 def send_angle_packet(self):
 """Send angle packet"""
 angle = self.angle_var.get()

 pkt = make_maze_ss_angle_packet(angle)
 self.send_packet(pkt, f"SS:2 Angle={angle}°")

 # Update visualization
 color_key = self.color_var.get()
 self.update_detection_display(color_key, angle)

 def send_eom_packet(self):
 """Send end-of-maze packet"""
 pkt = make_maze_ss_eom_packet()
 self.send_packet(pkt, "SS:3 End-of-Maze Signal")

 self.log_message(" End-of-Maze signal sent - MAZE → IDLE expected", "WARNING")

 def quick_color_test(self, color_key: str):
 """Quick test for specific color"""
 self.color_var.set(color_key)
 self.on_color_selected(None)
 self.send_color_packet()

 def test_edge_sensors(self):
 """Test edge sensor scenarios"""
 self.log_message(" Testing edge sensors (S1, S3)...", "INFO")
 threading.Thread(target=self._test_edge_sensors, daemon=True).start()

 def _test_edge_sensors(self):
 """Execute edge sensor tests"""
 edge_tests = ['S1_GREEN', 'S3_GREEN']

 for color_key in edge_tests:
 color_code, _ = self.all_color_tests[color_key]

 pkt = make_maze_ss_color_packet(color_code)
 self.send_packet(pkt, f"SS:1 Edge test: {color_key}")

 self.tests_completed.add(color_key)
 self.matrix_tree.set(color_key, "Status", " TESTED")

 time.sleep(1.0)

 self.update_matrix_summary()
 self.log_message(" Edge sensor tests complete", "SUCCESS")

 def test_all_angles(self):
 """Test all angle categories"""
 self.log_message(" Testing all angles (0-90°)...", "INFO")
 threading.Thread(target=self._test_all_angles, daemon=True).start()

 def _test_all_angles(self):
 """Execute angle tests"""
 test_angles = [0, 1, 5, 10, 20, 30, 45, 50, 60, 75, 90]

 for angle in test_angles:
 pkt = make_maze_ss_angle_packet(angle)
 self.send_packet(pkt, f"SS:2 Angle={angle}°")
 time.sleep(0.5)

 self.log_message(" All angle tests complete", "SUCCESS")

 def run_complete_matrix(self):
 """Run complete color matrix test"""
 if not messagebox.askyesno("Complete Matrix",
 "Test all color combinations?\nThis will send 8 color packets."):
 return

 self.log_message(" Running COMPLETE color matrix...", "INFO")
 threading.Thread(target=self._run_complete_matrix, daemon=True).start()

 def _run_complete_matrix(self):
 """Execute complete matrix test"""
 for color_key in self.all_color_tests.keys():
 color_code, description = self.all_color_tests[color_key]

 pkt = make_maze_ss_color_packet(color_code)
 self.send_packet(pkt, f"SS:1 {color_key} (0x{color_code:02X})")

 self.tests_completed.add(color_key)
 self.matrix_tree.set(color_key, "Status", " TESTED")
 self.update_matrix_summary()

 time.sleep(0.8)

 self.log_message(" COMPLETE color matrix test finished!", "SUCCESS")

 def update_detection_display(self, color_key: str, angle: int):
 """Update detection display"""
 color_code, description = self.all_color_tests[color_key]

 # Decode sensors
 s1 = color_code & 0x03
 s2 = (color_code >> 3) & 0x03
 s3 = (color_code >> 6) & 0x03

 color_names = {0: "WHITE", 1: "RED", 2: "GREEN", 3: "BLUE", 4: "BLACK"}

 detection_info = f"""

 CURRENT DETECTION 

 
 Color Code: {color_key:<18} 
 Byte Value: 0x{color_code:02X} ({color_code:3d}) 
 
 S1 (Left): {color_names.get(s1, str(s1)):<18} 
 S2 (Center): {color_names.get(s2, str(s2)):<18} 
 S3 (Right): {color_names.get(s3, str(s3)):<18} 
 
 Angle: {angle:2d}° 
 

"""

 self.detection_text.delete(1.0, tk.END)
 self.detection_text.insert(1.0, detection_info)

 def update_matrix_summary(self):
 """Update matrix summary"""
 total = len(self.all_color_tests)
 tested = len(self.tests_completed)
 coverage = (tested / total * 100) if total > 0 else 0

 self.matrix_summary.config(
 text=f"Coverage: {tested}/{total} colors tested ({coverage:.1f}%)"
 )

 def connect_serial(self):
 """Override to enable command buttons"""
 super().connect_serial()
 if self.is_connected:
 self.send_color_btn.config(state='normal')
 self.send_angle_btn.config(state='normal')
 self.send_eom_btn.config(state='normal')
 for btn in self.palette_buttons:
 btn.config(state='normal')

 def disconnect_serial(self):
 """Override to disable command buttons"""
 super().disconnect_serial()
 self.send_color_btn.config(state='disabled')
 self.send_angle_btn.config(state='disabled')
 self.send_eom_btn.config(state='disabled')
 for btn in self.palette_buttons:
 btn.config(state='disabled')


if __name__ == "__main__":
 app = MAZESSCommandTester()
 app.run()
