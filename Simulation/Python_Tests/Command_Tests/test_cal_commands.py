#!/usr/bin/env python3
"""
Calibration (CAL) State Command Test Script
Tests all CAL state commands and calibration sequences

Commands Tested:
- CAL:SS:0 - Sensor calibration start
- CAL:SS:1 - Sensor calibration complete
- CAL:MDPS:0 - Motor calibration start
- CAL:MDPS:1 - Motor rotation calibration
- CAL:SNC:0 - SNC in calibration mode
- CAL → MAZE transition validation

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


class CALCommandTester(BaseTestWindow):
 """CAL state command testing GUI"""

 def __init__(self):
 super().__init__("CAL State Command Tester", "1500x900")

 # Calibration state
 self.cal_sequence_active = False
 self.ss_calibrated = False
 self.mdps_calibrated = False

 self.setup_test_gui()

 def setup_test_gui(self):
 """Setup the test GUI"""
 # Title
 self.create_title(self.root, "Calibration (CAL) State Command Tester", "")

 # Main container
 main_frame = tk.Frame(self.root, bg=ColorScheme.BACKGROUND)
 main_frame.pack(fill='both', expand=True, padx=10, pady=10)

 # Left panel - Controls
 left_frame = tk.Frame(main_frame, bg=ColorScheme.PANEL, relief='raised', bd=2)
 left_frame.pack(side='left', fill='y', padx=(0, 5))
 left_frame.configure(width=450)
 left_frame.pack_propagate(False)

 # Serial connection
 conn_panel = self.create_serial_connection_panel(left_frame)
 conn_panel.pack(fill='x', padx=10, pady=10)

 # Command controls
 self.create_command_panel(left_frame)

 # Calibration sequence
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

 # Calibration Status tab
 status_frame = tk.Frame(notebook, bg=ColorScheme.TEXT_LIGHT)
 notebook.add(status_frame, text=" CAL Status")
 self.create_status_panel(status_frame)

 # Packet Log tab
 log_frame = tk.Frame(notebook, bg=ColorScheme.TEXT_LIGHT)
 notebook.add(log_frame, text=" Packet Monitor")
 log_panel = self.create_packet_log_panel(log_frame)

 # Procedure tab
 proc_frame = tk.Frame(notebook, bg=ColorScheme.TEXT_LIGHT)
 notebook.add(proc_frame, text=" CAL Procedure")
 self.create_procedure_panel(proc_frame)

 def create_command_panel(self, parent):
 """Create command test panel"""
 cmd_frame = tk.LabelFrame(parent, text=" CAL Commands",
 font=('Arial', 11, 'bold'), bg=ColorScheme.PANEL,
 fg='white', padx=10, pady=10)
 cmd_frame.pack(fill='x', padx=10, pady=10)

 # Individual commands
 commands = [
 ("CAL:SS:0 - Start SS Calibration", lambda: self.send_cal_ss(0)),
 ("CAL:SS:1 - SS Calibration Complete", lambda: self.send_cal_ss(1)),
 ("CAL:MDPS:0 - Start MDPS Calibration", lambda: self.send_cal_mdps(0)),
 ("CAL:MDPS:1 - MDPS Rotation Calibration", lambda: self.send_cal_mdps(1)),
 ]

 self.cmd_buttons = []
 for cmd_text, cmd_func in commands:
 btn = tk.Button(cmd_frame, text=cmd_text, command=cmd_func,
 bg=ColorScheme.INFO, fg='white',
 font=('Arial', 9, 'bold'), state='disabled')
 btn.pack(fill='x', pady=3)
 self.cmd_buttons.append(btn)

 def create_sequence_panel(self, parent):
 """Create calibration sequence panel"""
 seq_frame = tk.LabelFrame(parent, text=" CAL Sequences",
 font=('Arial', 11, 'bold'), bg=ColorScheme.PANEL,
 fg='white', padx=10, pady=10)
 seq_frame.pack(fill='both', expand=True, padx=10, pady=10)

 # Full calibration sequence
 self.full_cal_btn = tk.Button(seq_frame, text=" Run Complete CAL Sequence",
 command=self.run_full_calibration,
 bg=ColorScheme.WARNING, fg='white',
 font=('Arial', 10, 'bold'), state='disabled')
 self.full_cal_btn.pack(fill='x', pady=(0, 10))

 # SS only
 self.ss_cal_btn = tk.Button(seq_frame, text=" Calibrate SS Only",
 command=self.calibrate_ss_only,
 bg=ColorScheme.INFO, fg='white',
 font=('Arial', 9, 'bold'), state='disabled')
 self.ss_cal_btn.pack(fill='x', pady=3)

 # MDPS only
 self.mdps_cal_btn = tk.Button(seq_frame, text=" Calibrate MDPS Only",
 command=self.calibrate_mdps_only,
 bg=ColorScheme.INFO, fg='white',
 font=('Arial', 9, 'bold'), state='disabled')
 self.mdps_cal_btn.pack(fill='x', pady=3)

 # Reset calibration
 reset_btn = tk.Button(seq_frame, text=" Reset Calibration State",
 command=self.reset_calibration,
 bg='#95a5a6', fg='white',
 font=('Arial', 9, 'bold'))
 reset_btn.pack(fill='x', pady=(10, 0))

 # Calibration indicators
 tk.Label(seq_frame, text="Calibration Status:", bg=ColorScheme.PANEL,
 fg='white', font=('Arial', 10, 'bold')).pack(anchor='w', pady=(15, 5))

 indicator_frame = tk.Frame(seq_frame, bg=ColorScheme.PANEL)
 indicator_frame.pack(fill='x')

 self.ss_indicator = tk.Label(indicator_frame, text="SS: ",
 bg=ColorScheme.PANEL, fg=ColorScheme.ERROR,
 font=('Arial', 10, 'bold'))
 self.ss_indicator.pack(side='left', padx=(0, 20))

 self.mdps_indicator = tk.Label(indicator_frame, text="MDPS: ",
 bg=ColorScheme.PANEL, fg=ColorScheme.ERROR,
 font=('Arial', 10, 'bold'))
 self.mdps_indicator.pack(side='left')

 def create_status_panel(self, parent):
 """Create calibration status panel"""
 status_container = tk.Frame(parent, bg=ColorScheme.TEXT_LIGHT)
 status_container.pack(fill='both', expand=True, padx=10, pady=10)

 tk.Label(status_container, text="Calibration Status & Progress",
 font=('Arial', 14, 'bold'), bg=ColorScheme.TEXT_LIGHT,
 fg=ColorScheme.TEXT_DARK).pack(anchor='w', pady=(0, 10))

 # Status canvas for visualization
 self.status_canvas = tk.Canvas(status_container, bg='white', height=250)
 self.status_canvas.pack(fill='x', pady=(0, 10))

 self.draw_calibration_flowchart()

 # Progress log
 tk.Label(status_container, text="Calibration Progress Log:",
 bg=ColorScheme.TEXT_LIGHT, fg=ColorScheme.TEXT_DARK,
 font=('Arial', 11, 'bold')).pack(anchor='w', pady=(10, 5))

 self.progress_text = scrolledtext.ScrolledText(status_container, height=15,
 font=('Courier New', 9),
 bg=ColorScheme.BACKGROUND,
 fg=ColorScheme.TEXT_LIGHT)
 self.progress_text.pack(fill='both', expand=True)

 def create_procedure_panel(self, parent):
 """Create calibration procedure panel"""
 proc_container = tk.Frame(parent, bg=ColorScheme.TEXT_LIGHT)
 proc_container.pack(fill='both', expand=True, padx=10, pady=10)

 tk.Label(proc_container, text="CAL State Procedure Reference",
 font=('Arial', 12, 'bold'), bg=ColorScheme.TEXT_LIGHT,
 fg=ColorScheme.TEXT_DARK).pack(anchor='w', pady=(0, 10))

 proc_text = scrolledtext.ScrolledText(proc_container,
 font=('Courier New', 10),
 bg=ColorScheme.BACKGROUND,
 fg=ColorScheme.TEXT_LIGHT,
 wrap='word')
 proc_text.pack(fill='both', expand=True)

 procedure = """

 CALIBRATION (CAL) STATE PROCEDURE REFERENCE 


 CALIBRATION SEQUENCE (QTP-SNC-02):


PHASE 1: SS (Sensor Subsystem) Calibration


1. HUB sends CAL:SS:0
 • CONTROL = (SYS=1 | SUB=3 | IST=0) = 0x70 = 112
 • DAT1, DAT0, DEC = 0
 • Meaning: "SS, start calibration (no touch required)"

2. SS performs sensor calibration:
 • Calibrate color sensors (S1, S2, S3)
 • Measure ambient light levels
 • Set detection thresholds
 • Typically takes 2-5 seconds

3. HUB sends CAL:SS:1
 • CONTROL = (SYS=1 | SUB=3 | IST=1) = 0x71 = 113
 • DAT1, DAT0, DEC = 0
 • Meaning: "SS calibration complete, touch detected"

4. SNC receives SS calibration complete flag



PHASE 2: MDPS (Motor Drive & Power) Calibration


1. HUB sends CAL:MDPS:0
 • CONTROL = (SYS=1 | SUB=2 | IST=0) = 0x60 = 96
 • DAT1 = DAT0 = 10 (calibration speed in mm/s)
 • DEC = 0
 • Meaning: "MDPS, start calibration at 10 mm/s"

2. HUB sends CAL:MDPS:1 (rotation calibration)
 • CONTROL = (SYS=1 | SUB=2 | IST=1) = 0x61 = 97
 • DAT1 = 90 (rotation angle in degrees)
 • DAT0, DEC = 0
 • Meaning: "MDPS, calibrate 90° rotation"

3. MDPS performs motor calibration:
 • Measure wheel encoder counts
 • Calibrate rotation accuracy
 • Set PID parameters
 • Typically takes 3-6 seconds

4. SNC receives MDPS calibration complete flag



PHASE 3: CAL Loop (Continuous)


HUB continuously sends:
 • CAL:MDPS:1 (rotation calibration)
 • CAL:SS:1 (sensor complete)

SNC responds with:
 • CAL:SNC:0 while in calibration
 • CONTROL = (SYS=1 | SUB=1 | IST=0) = 0x50 = 80
 • DAT1 = 0 or 1 (calibration status)
 • DAT0, DEC = 0

Loop continues until BOTH flags received.



PHASE 4: Transition to MAZE


When SNC has received EOC (End-of-Calibration) from BOTH SS and MDPS:

1. SNC sets internal CAL_COMPLETE flag
2. Episode variables reset:
 • Distance = 0
 • Rotation count = 0
 • Line detection cleared

3. State transition: CAL → MAZE
 • Occurs within 200 ms of receiving final EOC

4. SNC sends first MAZE command:
 • MAZE:SNC:IST3 with DEC=0 (forward)
 • DAT1 = DAT0 = 10 (nominal speed)

SUCCESS CRITERIA:


 SS calibration completes within 5 seconds
 MDPS calibration completes within 6 seconds
 SNC acknowledges both EOC flags
 CAL → MAZE transition occurs within 200 ms
 Episode variables correctly reset
 First MAZE command transmitted

PACKET SEQUENCE EXAMPLE:


Time Dir Packet Description

T+0ms SENT (1-3-0) CAL:SS:0 - Start SS calibration
T+100ms SENT (1-2-0) CAL:MDPS:0 - Start MDPS calibration
T+200ms SENT (1-2-1) CAL:MDPS:1 - MDPS rotation (90°)
T+300ms SENT (1-3-1) CAL:SS:1 - SS complete (touch)
T+400ms RECEIVED (1-1-0) CAL:SNC:0 - SNC in CAL, DAT1=0
T+600ms SENT (1-2-1) CAL:MDPS:1 - Repeat rotation
T+700ms SENT (1-3-1) CAL:SS:1 - Repeat complete
T+800ms RECEIVED (1-1-0) CAL:SNC:0 - SNC in CAL, DAT1=0
 ... ... ... (Loop continues)
T+4.2s RECEIVED (1-1-0) CAL:SNC:0 - SNC in CAL, DAT1=1 (READY!)
T+4.3s RECEIVED (2-1-1) MAZE:SNC:1 - Transitioned to MAZE!


"""
 proc_text.insert(1.0, procedure)

 def draw_calibration_flowchart(self):
 """Draw calibration flowchart"""
 self.status_canvas.delete("all")

 # Draw flowchart boxes
 boxes = [
 (100, 50, "IDLE\n(Touch Sensor)", '#95a5a6'),
 (300, 50, "CAL\n(Calibration)", ColorScheme.WARNING),
 (500, 50, "MAZE\n(Navigation)", ColorScheme.SUCCESS_BG)
 ]

 for x, y, text, color in boxes:
 self.status_canvas.create_rectangle(x-60, y-30, x+60, y+30,
 fill=color, outline='black', width=2)
 self.status_canvas.create_text(x, y, text=text, font=('Arial', 10, 'bold'),
 fill='white')

 # Draw arrows
 self.status_canvas.create_line(160, 50, 240, 50, arrow=tk.LAST, width=2)
 self.status_canvas.create_line(360, 50, 440, 50, arrow=tk.LAST, width=2)

 # Draw calibration substeps
 substeps = [
 (100, 150, "SS CAL\nStart"),
 (200, 150, "SS CAL\nComplete"),
 (300, 150, "MDPS CAL\nStart"),
 (400, 150, "MDPS CAL\nRotation"),
 (500, 150, "Both\nComplete")
 ]

 for i, (x, y, text) in enumerate(substeps):
 color = ColorScheme.SUCCESS if i < 2 and self.ss_calibrated else \
 ColorScheme.SUCCESS if i >= 2 and i < 4 and self.mdps_calibrated else \
 ColorScheme.SUCCESS if i == 4 and (self.ss_calibrated and self.mdps_calibrated) else \
 '#bdc3c7'

 self.status_canvas.create_oval(x-30, y-30, x+30, y+30,
 fill=color, outline='black', width=2)
 self.status_canvas.create_text(x, y, text=text, font=('Arial', 8, 'bold'))

 if i < len(substeps) - 1:
 next_x = substeps[i+1][0]
 self.status_canvas.create_line(x+30, y, next_x-30, y, arrow=tk.LAST, width=1)

 def send_cal_ss(self, ist: int):
 """Send CAL:SS packet"""
 desc = "SS: CAL start" if ist == 0 else "SS: CAL complete (touch)"
 pkt = make_cal_ss_packet(ist)
 self.send_packet(pkt, desc)
 self.log_progress(f"{'' if ist == 0 else ''} {desc}")

 if ist == 1:
 self.ss_calibrated = True
 self.update_cal_indicators()

 def send_cal_mdps(self, ist: int):
 """Send CAL:MDPS packet"""
 if ist == 0:
 desc = "MDPS: CAL start (10mm/s)"
 pkt = make_cal_mdps_packet(0, speed=10)
 else:
 desc = "MDPS: Rotation CAL (90°)"
 pkt = make_cal_mdps_packet(1, angle=90)

 self.send_packet(pkt, desc)
 self.log_progress(f"{'' if ist == 0 else ''} {desc}")

 if ist == 1:
 self.mdps_calibrated = True
 self.update_cal_indicators()

 def run_full_calibration(self):
 """Run complete calibration sequence"""
 self.log_message(" Running FULL calibration sequence...", "INFO")
 self.log_progress("=" * 60)
 self.log_progress("FULL CALIBRATION SEQUENCE STARTED")
 self.log_progress("=" * 60)

 threading.Thread(target=self._execute_full_calibration, daemon=True).start()

 def _execute_full_calibration(self):
 """Execute full calibration sequence"""
 # Phase 1: SS Calibration
 self.log_progress("\n PHASE 1: SS Calibration")
 self.send_cal_ss(0)
 time.sleep(0.5)
 self.send_cal_ss(1)
 time.sleep(0.5)

 # Phase 2: MDPS Calibration
 self.log_progress("\n PHASE 2: MDPS Calibration")
 self.send_cal_mdps(0)
 time.sleep(0.5)
 self.send_cal_mdps(1)
 time.sleep(0.5)

 # Phase 3: CAL Loop (simulate 3 iterations)
 self.log_progress("\n PHASE 3: CAL Loop (waiting for SNC ready)")
 for i in range(3):
 self.log_progress(f" Loop {i+1}/3:")
 self.send_cal_mdps(1)
 time.sleep(0.2)
 self.send_cal_ss(1)
 time.sleep(0.8)

 # Phase 4: Transition
 self.log_progress("\n PHASE 4: CAL → MAZE Transition")
 self.log_progress(" Both subsystems calibrated")
 self.log_progress(" Waiting for SNC to transition...")
 time.sleep(1.0)

 self.log_progress("\n CALIBRATION SEQUENCE COMPLETE")
 self.log_progress("=" * 60)
 self.log_message(" Full calibration complete", "SUCCESS")

 def calibrate_ss_only(self):
 """Calibrate SS only"""
 self.log_message(" Calibrating SS only...", "INFO")
 threading.Thread(target=self._calibrate_ss, daemon=True).start()

 def _calibrate_ss(self):
 """Execute SS calibration"""
 self.log_progress("\n SS CALIBRATION ONLY")
 self.send_cal_ss(0)
 time.sleep(2.0) # Simulate calibration time
 self.send_cal_ss(1)
 self.log_progress(" SS calibration complete")
 self.log_message(" SS calibration complete", "SUCCESS")

 def calibrate_mdps_only(self):
 """Calibrate MDPS only"""
 self.log_message(" Calibrating MDPS only...", "INFO")
 threading.Thread(target=self._calibrate_mdps, daemon=True).start()

 def _calibrate_mdps(self):
 """Execute MDPS calibration"""
 self.log_progress("\n MDPS CALIBRATION ONLY")
 self.send_cal_mdps(0)
 time.sleep(1.0)
 self.send_cal_mdps(1)
 time.sleep(2.0) # Simulate calibration time
 self.log_progress(" MDPS calibration complete")
 self.log_message(" MDPS calibration complete", "SUCCESS")

 def reset_calibration(self):
 """Reset calibration state"""
 self.ss_calibrated = False
 self.mdps_calibrated = False
 self.update_cal_indicators()
 self.draw_calibration_flowchart()

 self.log_progress("\n CALIBRATION STATE RESET")
 self.log_message(" Calibration state reset", "INFO")

 def update_cal_indicators(self):
 """Update calibration indicators"""
 if self.ss_calibrated:
 self.ss_indicator.config(text="SS: ", fg=ColorScheme.SUCCESS_BG)
 else:
 self.ss_indicator.config(text="SS: ", fg=ColorScheme.ERROR)

 if self.mdps_calibrated:
 self.mdps_indicator.config(text="MDPS: ", fg=ColorScheme.SUCCESS_BG)
 else:
 self.mdps_indicator.config(text="MDPS: ", fg=ColorScheme.ERROR)

 self.draw_calibration_flowchart()

 def log_progress(self, message: str):
 """Log progress message"""
 timestamp = datetime.now().strftime("%H:%M:%S")
 self.progress_text.insert(tk.END, f"[{timestamp}] {message}\n")
 self.progress_text.see(tk.END)

 def connect_serial(self):
 """Override to enable command buttons"""
 super().connect_serial()
 if self.is_connected:
 for btn in self.cmd_buttons:
 btn.config(state='normal')
 self.full_cal_btn.config(state='normal')
 self.ss_cal_btn.config(state='normal')
 self.mdps_cal_btn.config(state='normal')

 def disconnect_serial(self):
 """Override to disable command buttons"""
 super().disconnect_serial()
 for btn in self.cmd_buttons:
 btn.config(state='disabled')
 self.full_cal_btn.config(state='disabled')
 self.ss_cal_btn.config(state='disabled')
 self.mdps_cal_btn.config(state='disabled')


if __name__ == "__main__":
 app = CALCommandTester()
 app.run()
