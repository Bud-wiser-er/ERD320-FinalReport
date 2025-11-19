#!/usr/bin/env python3
"""
MAZE-MDPS Command Test Script
Comprehensive testing for MAZE state MDPS (Motor Drive & Power Supply) commands

Commands Tested:
- MAZE:MDPS:1 - Stop/Rotate commands
- MAZE:MDPS:2 - Confirmation packets
- MAZE:MDPS:3 - Forward motion with speed control
- MAZE:MDPS:4 - Distance update packets

Validates speed ranges, rotation angles, distance tracking, and command sequencing.

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


class MAZEMDPSCommandTester(BaseTestWindow):
 """MAZE-MDPS command testing GUI"""

 def __init__(self):
 super().__init__("MAZE-MDPS Command Tester", "1500x950")

 # Virtual robot state
 self.virtual_distance = 0
 self.virtual_speed_l = 0
 self.virtual_speed_r = 0
 self.virtual_rotation = 0

 self.setup_test_gui()

 def setup_test_gui(self):
 """Setup the test GUI"""
 # Title
 self.create_title(self.root, "MAZE-MDPS Command Tester - Motor Control Validation", "")

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

 # Test sequences
 self.create_sequence_panel(left_frame)

 # Statistics
 stats_panel = self.create_statistics_panel(left_frame)
 stats_panel.pack(fill='x', padx=10, pady=10)

 # Right panel - Monitoring
 right_frame = tk.Frame(main_frame, bg=ColorScheme.BACKGROUND)
 right_frame.pack(side='right', fill='both', expand=True, padx=(5, 0))

 # Notebook
 notebook = ttk.Notebook(right_frame)
 notebook.pack(fill='both', expand=True)

 # Robot State tab
 state_frame = tk.Frame(notebook, bg=ColorScheme.TEXT_LIGHT)
 notebook.add(state_frame, text=" Robot State")
 self.create_robot_state_panel(state_frame)

 # Speed Control tab
 speed_frame = tk.Frame(notebook, bg=ColorScheme.TEXT_LIGHT)
 notebook.add(speed_frame, text=" Speed Control")
 self.create_speed_panel(speed_frame)

 # Packet Log tab
 log_frame = tk.Frame(notebook, bg=ColorScheme.TEXT_LIGHT)
 notebook.add(log_frame, text=" Packet Monitor")
 log_panel = self.create_packet_log_panel(log_frame)

 # Command Reference tab
 ref_frame = tk.Frame(notebook, bg=ColorScheme.TEXT_LIGHT)
 notebook.add(ref_frame, text=" Command Reference")
 self.create_reference_panel(ref_frame)

 def create_command_panel(self, parent):
 """Create MDPS command panel"""
 cmd_frame = tk.LabelFrame(parent, text=" MDPS Commands",
 font=('Arial', 11, 'bold'), bg=ColorScheme.PANEL,
 fg='white', padx=10, pady=10)
 cmd_frame.pack(fill='x', padx=10, pady=10)

 # IST1 - Stop/Rotate
 tk.Label(cmd_frame, text="MAZE:MDPS:1 - Stop/Rotate:", bg=ColorScheme.PANEL,
 fg='white', font=('Arial', 9, 'bold')).pack(anchor='w')

 rot_frame = tk.Frame(cmd_frame, bg=ColorScheme.PANEL)
 rot_frame.pack(fill='x', pady=(0, 10))

 tk.Label(rot_frame, text="Angle:", bg=ColorScheme.PANEL,
 fg='white', font=('Arial', 8)).pack(side='left')

 self.rotation_var = tk.IntVar(value=90)
 rot_spin = tk.Spinbox(rot_frame, from_=0, to=180, textvariable=self.rotation_var,
 width=8, font=('Arial', 9))
 rot_spin.pack(side='left', padx=(5, 10))

 ist1_btn = tk.Button(rot_frame, text="Send IST1",
 command=self.send_mdps_ist1,
 bg=ColorScheme.INFO, fg='white',
 font=('Arial', 8, 'bold'), state='disabled')
 ist1_btn.pack(side='left')
 self.cmd_buttons = [ist1_btn]

 # IST2 - Confirmation
 ist2_btn = tk.Button(cmd_frame, text="MAZE:MDPS:2 - Send Confirmation",
 command=self.send_mdps_ist2,
 bg=ColorScheme.INFO, fg='white',
 font=('Arial', 9, 'bold'), state='disabled')
 ist2_btn.pack(fill='x', pady=3)
 self.cmd_buttons.append(ist2_btn)

 # IST3 - Forward motion
 tk.Label(cmd_frame, text="MAZE:MDPS:3 - Forward Motion:", bg=ColorScheme.PANEL,
 fg='white', font=('Arial', 9, 'bold')).pack(anchor='w', pady=(10, 0))

 speed_frame = tk.Frame(cmd_frame, bg=ColorScheme.PANEL)
 speed_frame.pack(fill='x', pady=(0, 5))

 tk.Label(speed_frame, text="vL:", bg=ColorScheme.PANEL,
 fg='white', font=('Arial', 8)).pack(side='left')

 self.speed_l_var = tk.IntVar(value=10)
 speed_l_spin = tk.Spinbox(speed_frame, from_=0, to=50,
 textvariable=self.speed_l_var,
 width=5, font=('Arial', 9))
 speed_l_spin.pack(side='left', padx=(5, 15))

 tk.Label(speed_frame, text="vR:", bg=ColorScheme.PANEL,
 fg='white', font=('Arial', 8)).pack(side='left')

 self.speed_r_var = tk.IntVar(value=10)
 speed_r_spin = tk.Spinbox(speed_frame, from_=0, to=50,
 textvariable=self.speed_r_var,
 width=5, font=('Arial', 9))
 speed_r_spin.pack(side='left', padx=(5, 0))

 ist3_btn = tk.Button(cmd_frame, text="Send IST3 (Forward)",
 command=self.send_mdps_ist3,
 bg=ColorScheme.SUCCESS, fg='white',
 font=('Arial', 9, 'bold'), state='disabled')
 ist3_btn.pack(fill='x', pady=3)
 self.cmd_buttons.append(ist3_btn)

 # IST4 - Distance update
 tk.Label(cmd_frame, text="MAZE:MDPS:4 - Distance Update:", bg=ColorScheme.PANEL,
 fg='white', font=('Arial', 9, 'bold')).pack(anchor='w', pady=(10, 0))

 dist_btn = tk.Button(cmd_frame, text="Send Distance (auto-increment)",
 command=self.send_mdps_ist4,
 bg=ColorScheme.INFO, fg='white',
 font=('Arial', 9, 'bold'), state='disabled')
 dist_btn.pack(fill='x', pady=3)
 self.cmd_buttons.append(dist_btn)

 def create_sequence_panel(self, parent):
 """Create test sequence panel"""
 seq_frame = tk.LabelFrame(parent, text=" Test Sequences",
 font=('Arial', 11, 'bold'), bg=ColorScheme.PANEL,
 fg='white', padx=10, pady=10)
 seq_frame.pack(fill='both', expand=True, padx=10, pady=10)

 sequences = [
 (" Basic Forward Motion (10 iterations)", self.test_forward_motion),
 (" Rotation Sequence (90°, 180°, 45°)", self.test_rotations),
 (" Distance Tracking (0-5m)", self.test_distance_tracking),
 (" Speed Variation Test", self.test_speed_variation),
 (" Complete MDPS Command Suite", self.run_complete_suite)
 ]

 self.seq_buttons = []
 for text, command in sequences:
 btn = tk.Button(seq_frame, text=text, command=command,
 bg=ColorScheme.WARNING, fg='white',
 font=('Arial', 9, 'bold'), state='disabled')
 btn.pack(fill='x', pady=3)
 self.seq_buttons.append(btn)

 def create_robot_state_panel(self, parent):
 """Create robot state visualization"""
 state_container = tk.Frame(parent, bg=ColorScheme.TEXT_LIGHT)
 state_container.pack(fill='both', expand=True, padx=10, pady=10)

 tk.Label(state_container, text="Virtual Robot State",
 font=('Arial', 14, 'bold'), bg=ColorScheme.TEXT_LIGHT,
 fg=ColorScheme.TEXT_DARK).pack(anchor='w', pady=(0, 10))

 # State display
 self.state_canvas = tk.Canvas(state_container, bg='white', height=300)
 self.state_canvas.pack(fill='both', expand=True, pady=(0, 10))

 self.draw_robot_state()

 # State values
 values_frame = tk.Frame(state_container, bg=ColorScheme.TEXT_LIGHT)
 values_frame.pack(fill='x')

 # Distance
 dist_frame = tk.Frame(values_frame, bg=ColorScheme.BACKGROUND,
 relief='raised', bd=2, padx=15, pady=10)
 dist_frame.pack(side='left', fill='both', expand=True, padx=(0, 5))

 tk.Label(dist_frame, text="Distance Traveled", bg=ColorScheme.BACKGROUND,
 fg='white', font=('Arial', 10, 'bold')).pack()
 self.dist_value_label = tk.Label(dist_frame, text="0.00 m",
 bg=ColorScheme.BACKGROUND,
 fg=ColorScheme.SUCCESS_BG,
 font=('Arial', 16, 'bold'))
 self.dist_value_label.pack()

 # Speed
 speed_frame = tk.Frame(values_frame, bg=ColorScheme.BACKGROUND,
 relief='raised', bd=2, padx=15, pady=10)
 speed_frame.pack(side='left', fill='both', expand=True, padx=5)

 tk.Label(speed_frame, text="Current Speed", bg=ColorScheme.BACKGROUND,
 fg='white', font=('Arial', 10, 'bold')).pack()
 self.speed_value_label = tk.Label(speed_frame, text="L:0 R:0 mm/s",
 bg=ColorScheme.BACKGROUND,
 fg=ColorScheme.SUCCESS_BG,
 font=('Arial', 14, 'bold'))
 self.speed_value_label.pack()

 # Rotation
 rot_frame = tk.Frame(values_frame, bg=ColorScheme.BACKGROUND,
 relief='raised', bd=2, padx=15, pady=10)
 rot_frame.pack(side='left', fill='both', expand=True, padx=(5, 0))

 tk.Label(rot_frame, text="Total Rotation", bg=ColorScheme.BACKGROUND,
 fg='white', font=('Arial', 10, 'bold')).pack()
 self.rot_value_label = tk.Label(rot_frame, text="0°",
 bg=ColorScheme.BACKGROUND,
 fg=ColorScheme.SUCCESS_BG,
 font=('Arial', 16, 'bold'))
 self.rot_value_label.pack()

 def create_speed_panel(self, parent):
 """Create speed control panel"""
 speed_container = tk.Frame(parent, bg=ColorScheme.TEXT_LIGHT)
 speed_container.pack(fill='both', expand=True, padx=10, pady=10)

 tk.Label(speed_container, text="Speed Control Testing",
 font=('Arial', 12, 'bold'), bg=ColorScheme.TEXT_LIGHT,
 fg=ColorScheme.TEXT_DARK).pack(anchor='w', pady=(0, 10))

 # Speed test matrix
 test_speeds = [
 (0, 0, "Stop"),
 (5, 5, "Slow (5 mm/s)"),
 (10, 10, "Nominal (10 mm/s)"),
 (15, 15, "Fast (15 mm/s)"),
 (10, 5, "Turn Left"),
 (5, 10, "Turn Right"),
 (10, 0, "Pivot Left"),
 (0, 10, "Pivot Right")
 ]

 tk.Label(speed_container, text="Quick Speed Tests:", bg=ColorScheme.TEXT_LIGHT,
 fg=ColorScheme.TEXT_DARK, font=('Arial', 11, 'bold')).pack(anchor='w')

 for vl, vr, desc in test_speeds:
 btn = tk.Button(speed_container, text=f"{desc} (vL={vl}, vR={vr})",
 command=lambda l=vl, r=vr: self.test_specific_speed(l, r),
 bg=ColorScheme.INFO, fg='white',
 font=('Arial', 9), state='disabled')
 btn.pack(fill='x', pady=2)
 self.seq_buttons.append(btn)

 def create_reference_panel(self, parent):
 """Create command reference"""
 ref_container = tk.Frame(parent, bg=ColorScheme.TEXT_LIGHT)
 ref_container.pack(fill='both', expand=True, padx=10, pady=10)

 tk.Label(ref_container, text="MAZE:MDPS Command Reference",
 font=('Arial', 12, 'bold'), bg=ColorScheme.TEXT_LIGHT,
 fg=ColorScheme.TEXT_DARK).pack(anchor='w', pady=(0, 10))

 ref_text = scrolledtext.ScrolledText(ref_container,
 font=('Courier New', 9),
 bg=ColorScheme.BACKGROUND,
 fg=ColorScheme.TEXT_LIGHT,
 wrap='word')
 ref_text.pack(fill='both', expand=True)

 reference = """

 MAZE:MDPS COMMAND REFERENCE 


 MAZE:MDPS:1 - Stop/Rotate Command


CONTROL: (SYS=2 | SUB=2 | IST=1) = 0xA1 = 161
DAT1: Rotation angle (degrees) OR 90 for stop
DAT0: 0
DEC: 0

Purpose: Halt motion and/or initiate rotation
Usage: Sent when robot needs to stop or change direction

Example: Stop + Rotate 90° right
 CONTROL = 161, DAT1 = 90, DAT0 = 0, DEC = 0



 MAZE:MDPS:2 - Confirmation Packet


CONTROL: (SYS=2 | SUB=2 | IST=2) = 0xA2 = 162
DAT1: Distance traveled (meters)
DAT0: Distance traveled (centimeters)
DEC: Direction (0=forward, 1=reverse, 2=left, 3=right)

Purpose: Acknowledge command and report current state
Usage: Response to IST1, confirms rotation/stop

Example: Confirm at 1.44 m, forward direction
 CONTROL = 162, DAT1 = 1, DAT0 = 44, DEC = 0



 MAZE:MDPS:3 - Forward Motion Command


CONTROL: (SYS=2 | SUB=2 | IST=3) = 0xA3 = 163
DAT1: vR - Right wheel speed (mm/s)
DAT0: vL - Left wheel speed (mm/s)
DEC: 0 (forward) | 1 (reverse)

Purpose: Set wheel speeds for motion
Usage: Continuous forward/reverse motion with speed control

Speed Ranges:
 • 0 mm/s: Stop
 • 1-5 mm/s: Very slow (precise positioning)
 • 5-15 mm/s: Normal navigation speed
 • 15-30 mm/s: Fast (straight-line only)
 • 30+ mm/s: Maximum (use with caution)

Examples:
 Straight ahead (10 mm/s):
 DAT1 = 10, DAT0 = 10, DEC = 0

 Turn left (differential):
 DAT1 = 10 (right faster), DAT0 = 5, DEC = 0

 Reverse:
 DAT1 = 10, DAT0 = 10, DEC = 1



 MAZE:MDPS:4 - Distance Update


CONTROL: (SYS=2 | SUB=2 | IST=4) = 0xA4 = 164
DAT1: Distance (meters)
DAT0: Distance (centimeters)
DEC: 0

Purpose: Report cumulative distance traveled
Usage: Periodic updates (typically every control loop)

Format: Distance = DAT1.DAT0 meters
 Example: 3.25 m → DAT1 = 3, DAT0 = 25

Update Rate:
 • Nominal: 200 Hz (every 5 ms)
 • Distance increment: ~0.5-2 cm per update at 10 mm/s



 TYPICAL COMMAND SEQUENCE:

Forward Motion:
 1. MDPS:1 (Stop/Rotate if needed)
 2. MDPS:2 (Confirm ready)
 3. MDPS:3 (Set forward speed)
 4. MDPS:4 (Distance updates...)
 5. MDPS:4 (Distance updates...)
 6. MDPS:1 (Stop)

Rotation:
 1. MDPS:1 (Stop + Rotate θ°)
 2. MDPS:2 (Confirm rotation)
 3. MDPS:3 (Forward after rotation)
 4. MDPS:4 (Distance updates...)



 CONSTRAINTS:

• Wheel Speed Range: 0-50 mm/s
• Maximum Differential: vR - vL ≤ 20 mm/s
• Rotation Range: 0-180°
• Distance Max: 255.99 m (DAT1=255, DAT0=99)
• Update Rate: 5-200 Hz (200 Hz nominal)


"""
 ref_text.insert(1.0, reference)

 def draw_robot_state(self):
 """Draw robot state visualization"""
 self.state_canvas.delete("all")

 # Draw robot (simple rectangle)
 robot_x = 400
 robot_y = 150

 # Robot body
 self.state_canvas.create_rectangle(robot_x-40, robot_y-30,
 robot_x+40, robot_y+30,
 fill=ColorScheme.INFO, outline='black', width=2)

 # Direction arrow
 arrow_length = 50
 self.state_canvas.create_line(robot_x, robot_y, robot_x, robot_y-arrow_length,
 arrow=tk.LAST, width=3, fill='red')

 # Wheels (left and right)
 wheel_width = 8
 wheel_height = 20

 # Left wheel
 left_color = ColorScheme.SUCCESS if self.virtual_speed_l > 0 else '#95a5a6'
 self.state_canvas.create_rectangle(robot_x-40-wheel_width, robot_y-wheel_height,
 robot_x-40, robot_y+wheel_height,
 fill=left_color, outline='black', width=2)

 # Right wheel
 right_color = ColorScheme.SUCCESS if self.virtual_speed_r > 0 else '#95a5a6'
 self.state_canvas.create_rectangle(robot_x+40, robot_y-wheel_height,
 robot_x+40+wheel_width, robot_y+wheel_height,
 fill=right_color, outline='black', width=2)

 # Labels
 self.state_canvas.create_text(robot_x, robot_y+60, text=" VIRTUAL ROBOT",
 font=('Arial', 12, 'bold'))

 def send_mdps_ist1(self):
 """Send MAZE:MDPS:1"""
 angle = self.rotation_var.get()
 pkt = make_maze_mdps_packet(1, angle, 0, 0)
 self.send_packet(pkt, f"MDPS:1 Stop/Rotate {angle}°")

 self.virtual_rotation += angle
 self.update_robot_display()

 def send_mdps_ist2(self):
 """Send MAZE:MDPS:2"""
 dist_m = int(self.virtual_distance // 100)
 dist_cm = int(self.virtual_distance % 100)
 pkt = make_maze_mdps_packet(2, dist_m, dist_cm, 0)
 self.send_packet(pkt, f"MDPS:2 Confirm at {dist_m}.{dist_cm:02d}m")

 def send_mdps_ist3(self):
 """Send MAZE:MDPS:3"""
 vl = self.speed_l_var.get()
 vr = self.speed_r_var.get()

 self.virtual_speed_l = vl
 self.virtual_speed_r = vr

 pkt = make_maze_mdps_packet(3, vr, vl, 0)
 self.send_packet(pkt, f"MDPS:3 Forward vL={vl}, vR={vr}")

 self.update_robot_display()

 def send_mdps_ist4(self):
 """Send MAZE:MDPS:4"""
 self.virtual_distance += 2 # Increment by 2 cm

 dist_m = int(self.virtual_distance // 100)
 dist_cm = int(self.virtual_distance % 100)

 pkt = make_maze_mdps_packet(4, dist_m, dist_cm, 0)
 self.send_packet(pkt, f"MDPS:4 Distance={dist_m}.{dist_cm:02d}m")

 self.update_robot_display()

 def test_forward_motion(self):
 """Test forward motion"""
 self.log_message(" Testing forward motion (10 iterations)...", "INFO")
 threading.Thread(target=self._execute_forward_motion, daemon=True).start()

 def _execute_forward_motion(self):
 """Execute forward motion test"""
 # Set speed
 vl = vr = 10
 self.virtual_speed_l = vl
 self.virtual_speed_r = vr

 pkt = make_maze_mdps_packet(3, vr, vl, 0)
 self.send_packet(pkt, f"MDPS:3 Forward vL={vl}, vR={vr}")
 time.sleep(0.2)

 # Send distance updates
 for i in range(10):
 self.send_mdps_ist4()
 time.sleep(0.5)

 self.log_message(" Forward motion test complete", "SUCCESS")

 def test_rotations(self):
 """Test rotation sequence"""
 self.log_message(" Testing rotations (90°, 180°, 45°)...", "INFO")
 threading.Thread(target=self._execute_rotations, daemon=True).start()

 def _execute_rotations(self):
 """Execute rotation tests"""
 angles = [90, 180, 45]

 for angle in angles:
 pkt = make_maze_mdps_packet(1, angle, 0, 0)
 self.send_packet(pkt, f"MDPS:1 Rotate {angle}°")

 self.virtual_rotation += angle
 self.update_robot_display()

 time.sleep(1.5)

 self.log_message(" Rotation test complete", "SUCCESS")

 def test_distance_tracking(self):
 """Test distance tracking"""
 self.log_message(" Testing distance tracking (0-5m)...", "INFO")
 threading.Thread(target=self._execute_distance_tracking, daemon=True).start()

 def _execute_distance_tracking(self):
 """Execute distance tracking test"""
 target_distance = 500 # 5 m in cm

 while self.virtual_distance < target_distance:
 self.send_mdps_ist4()
 time.sleep(0.1)

 self.log_message(" Distance tracking test complete (5m)", "SUCCESS")

 def test_speed_variation(self):
 """Test speed variation"""
 self.log_message(" Testing speed variation...", "INFO")
 threading.Thread(target=self._execute_speed_variation, daemon=True).start()

 def _execute_speed_variation(self):
 """Execute speed variation test"""
 test_speeds = [(5, 5), (10, 10), (15, 15), (10, 5), (5, 10)]

 for vl, vr in test_speeds:
 pkt = make_maze_mdps_packet(3, vr, vl, 0)
 self.send_packet(pkt, f"MDPS:3 vL={vl}, vR={vr}")

 self.virtual_speed_l = vl
 self.virtual_speed_r = vr
 self.update_robot_display()

 time.sleep(1.0)

 self.log_message(" Speed variation test complete", "SUCCESS")

 def test_specific_speed(self, vl, vr):
 """Test specific speed"""
 pkt = make_maze_mdps_packet(3, vr, vl, 0)
 self.send_packet(pkt, f"MDPS:3 vL={vl}, vR={vr}")

 self.virtual_speed_l = vl
 self.virtual_speed_r = vr
 self.update_robot_display()

 def run_complete_suite(self):
 """Run complete MDPS test suite"""
 self.log_message(" Running COMPLETE MDPS command suite...", "INFO")
 threading.Thread(target=self._run_complete_suite, daemon=True).start()

 def _run_complete_suite(self):
 """Execute complete test suite"""
 self._execute_forward_motion()
 time.sleep(2.0)

 self._execute_rotations()
 time.sleep(2.0)

 self._execute_speed_variation()
 time.sleep(2.0)

 self.log_message(" COMPLETE MDPS test suite finished!", "SUCCESS")

 def update_robot_display(self):
 """Update robot display"""
 # Update distance
 dist_m = self.virtual_distance / 100.0
 self.dist_value_label.config(text=f"{dist_m:.2f} m")

 # Update speed
 self.speed_value_label.config(text=f"L:{self.virtual_speed_l} R:{self.virtual_speed_r} mm/s")

 # Update rotation
 self.rot_value_label.config(text=f"{self.virtual_rotation}°")

 # Redraw robot
 self.draw_robot_state()

 def connect_serial(self):
 """Override to enable command buttons"""
 super().connect_serial()
 if self.is_connected:
 for btn in self.cmd_buttons:
 btn.config(state='normal')
 for btn in self.seq_buttons:
 btn.config(state='normal')

 def disconnect_serial(self):
 """Override to disable command buttons"""
 super().disconnect_serial()
 for btn in self.cmd_buttons:
 btn.config(state='disabled')
 for btn in self.seq_buttons:
 btn.config(state='disabled')


if __name__ == "__main__":
 app = MAZEMDPSCommandTester()
 app.run()
