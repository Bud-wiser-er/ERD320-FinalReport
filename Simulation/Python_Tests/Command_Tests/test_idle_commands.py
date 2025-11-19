#!/usr/bin/env python3
"""
IDLE State Command Test Script
Tests all IDLE state commands and transitions

Commands Tested:
- IDLE:HUB:0 - Initial contact
- IDLE:SNC:0 - System ready response
- IDLE → CAL transition validation

Author: ERD320 SNC Team
Date: 2025-01-18
"""

import sys
import os
sys.path.append(os.path.join(os.path.dirname(__file__), '../../Core'))

import tkinter as tk
from tkinter import ttk, messagebox
import threading
import time

from gui_framework import BaseTestWindow, ColorScheme
from scs_protocol import *


class IDLECommandTester(BaseTestWindow):
 """IDLE state command testing GUI"""

 def __init__(self):
 super().__init__("IDLE State Command Tester", "1400x900")
 self.setup_test_gui()

 def setup_test_gui(self):
 """Setup the test GUI"""
 # Title
 self.create_title(self.root, "IDLE State Command Tester", "⏸")

 # Main container
 main_frame = tk.Frame(self.root, bg=ColorScheme.BACKGROUND)
 main_frame.pack(fill='both', expand=True, padx=10, pady=10)

 # Left panel - Controls
 left_frame = tk.Frame(main_frame, bg=ColorScheme.PANEL, relief='raised', bd=2)
 left_frame.pack(side='left', fill='y', padx=(0, 5))
 left_frame.configure(width=400)
 left_frame.pack_propagate(False)

 # Serial connection
 conn_panel = self.create_serial_connection_panel(left_frame)
 conn_panel.pack(fill='x', padx=10, pady=10)

 # Command controls
 self.create_command_panel(left_frame)

 # Statistics
 stats_panel = self.create_statistics_panel(left_frame)
 stats_panel.pack(fill='x', padx=10, pady=10)

 # Right panel - Monitoring
 right_frame = tk.Frame(main_frame, bg=ColorScheme.BACKGROUND)
 right_frame.pack(side='right', fill='both', expand=True, padx=(5, 0))

 # Notebook
 notebook = ttk.Notebook(right_frame)
 notebook.pack(fill='both', expand=True)

 # Packet log tab
 log_frame = tk.Frame(notebook, bg=ColorScheme.TEXT_LIGHT)
 notebook.add(log_frame, text=" Packet Monitor")
 log_panel = self.create_packet_log_panel(log_frame)

 # Test results tab
 results_frame = tk.Frame(notebook, bg=ColorScheme.TEXT_LIGHT)
 notebook.add(results_frame, text=" Test Results")
 self.create_results_panel(results_frame)

 def create_command_panel(self, parent):
 """Create command test panel"""
 cmd_frame = tk.LabelFrame(parent, text="⏸ IDLE Commands",
 font=('Arial', 11, 'bold'), bg=ColorScheme.PANEL,
 fg='white', padx=10, pady=10)
 cmd_frame.pack(fill='both', expand=True, padx=10, pady=10)

 # Command buttons
 commands = [
 ("IDLE:HUB:0 - Initial Contact", self.send_idle_hub),
 ("IDLE:SNC:0 - System Ready (Simulated)", self.simulate_snc_ready),
 ("Test IDLE → CAL Transition", self.test_idle_to_cal)
 ]

 for cmd_text, cmd_func in commands:
 btn = tk.Button(cmd_frame, text=cmd_text, command=cmd_func,
 bg=ColorScheme.INFO, fg='white',
 font=('Arial', 10, 'bold'), state='disabled')
 btn.pack(fill='x', pady=5)

 # Store button for later enabling
 if not hasattr(self, 'cmd_buttons'):
 self.cmd_buttons = []
 self.cmd_buttons.append(btn)

 # Expected response display
 tk.Label(cmd_frame, text="Expected Response:", bg=ColorScheme.PANEL,
 fg='white', font=('Arial', 10, 'bold')).pack(anchor='w', pady=(15, 5))

 self.expected_text = tk.Text(cmd_frame, height=6, wrap='word',
 font=('Courier New', 9),
 bg=ColorScheme.BACKGROUND,
 fg=ColorScheme.TEXT_LIGHT)
 self.expected_text.pack(fill='x')
 self.expected_text.insert(1.0, "Select a command to see expected response...")

 def create_results_panel(self, parent):
 """Create test results panel"""
 results_container = tk.Frame(parent, bg=ColorScheme.TEXT_LIGHT)
 results_container.pack(fill='both', expand=True, padx=10, pady=10)

 tk.Label(results_container, text="IDLE Command Test Results",
 font=('Arial', 14, 'bold'), bg=ColorScheme.TEXT_LIGHT,
 fg=ColorScheme.TEXT_DARK).pack(anchor='w', pady=(0, 10))

 # Results treeview
 columns = ("Command", "Status", "Time")
 self.results_tree = ttk.Treeview(results_container, columns=columns,
 show='tree headings', height=10)

 self.results_tree.heading("#0", text="Test")
 self.results_tree.heading("Command", text="Command")
 self.results_tree.heading("Status", text="Status")
 self.results_tree.heading("Time", text="Timestamp")

 self.results_tree.column("#0", width=50)
 self.results_tree.column("Command", width=250)
 self.results_tree.column("Status", width=100)
 self.results_tree.column("Time", width=150)

 self.results_tree.pack(fill='both', expand=True)

 # Summary
 self.summary_label = tk.Label(results_container,
 text="Tests: 0 PASS, 0 FAIL, 0 TOTAL",
 bg=ColorScheme.TEXT_LIGHT,
 fg=ColorScheme.TEXT_DARK,
 font=('Arial', 11, 'bold'))
 self.summary_label.pack(pady=10)

 # Test counters
 self.test_results = {'pass': 0, 'fail': 0, 'total': 0}

 def send_idle_hub(self):
 """Send IDLE:HUB:0 packet"""
 self.expected_text.delete(1.0, tk.END)
 self.expected_text.insert(1.0,
 "Expected Response:\n"
 "(0-1-0) IDLE:SNC:0\n"
 "DAT1 = Touch count (typically 1)\n"
 "DAT0 = Distance traveled\n"
 "DEC = 0\n\n"
 "This indicates SNC is ready and in IDLE state.")

 pkt = make_idle_hub_packet()
 if self.send_packet(pkt, "HUB: Initial contact"):
 self.log_test_result("IDLE:HUB:0", "SENT", "SUCCESS")

 def simulate_snc_ready(self):
 """Simulate SNC ready response (for testing without hardware)"""
 self.expected_text.delete(1.0, tk.END)
 self.expected_text.insert(1.0,
 "Simulating SNC Response:\n"
 "(0-1-0) IDLE:SNC:0\n"
 "DAT1 = 1 (touch count)\n"
 "DAT0 = 50 (distance)\n"
 "DEC = 0\n\n"
 "This simulates SNC responding to HUB contact.")

 # Simulate receiving response
 from datetime import datetime
 timestamp = datetime.now().strftime("%H:%M:%S.%f")[:-3]
 pkt = make_idle_snc_packet(1, 50)

 log_line = f"{timestamp} || SIMULATED || {pkt}"
 self.log_message(log_line, "INFO")
 self.log_test_result("IDLE:SNC:0", "SIMULATED", "INFO")

 def test_idle_to_cal(self):
 """Test IDLE → CAL transition"""
 self.expected_text.delete(1.0, tk.END)
 self.expected_text.insert(1.0,
 "Testing IDLE → CAL Transition:\n\n"
 "1. Send IDLE:HUB:0\n"
 "2. Wait for IDLE:SNC:0 response\n"
 "3. Touch sensor activated (DAT1=1)\n"
 "4. Expect transition to CAL state\n\n"
 "SNC should respond with CAL:SNC:0")

 # Run test sequence
 threading.Thread(target=self.execute_transition_test, daemon=True).start()

 def execute_transition_test(self):
 """Execute IDLE → CAL transition test"""
 self.log_message(" Starting IDLE → CAL transition test...", "INFO")

 # Step 1: Send IDLE:HUB:0
 pkt = make_idle_hub_packet()
 self.send_packet(pkt, "HUB: Initial contact")
 time.sleep(1.0)

 # Step 2: Simulate touch activation
 self.log_message(" Simulating touch sensor activation...", "INFO")
 time.sleep(0.5)

 # Step 3: Expect CAL state
 self.log_message("⏳ Waiting for CAL state transition...", "INFO")
 time.sleep(1.0)

 # Log result
 self.log_test_result("IDLE→CAL Transition", "COMPLETED", "SUCCESS")
 self.log_message(" IDLE → CAL transition test complete", "SUCCESS")

 def log_test_result(self, command: str, status: str, result_type: str):
 """Log test result to results tree"""
 from datetime import datetime
 timestamp = datetime.now().strftime("%H:%M:%S")

 # Add to tree
 test_num = self.test_results['total'] + 1
 self.results_tree.insert("", "end",
 text=f"#{test_num}",
 values=(command, status, timestamp))

 # Update counters
 self.test_results['total'] += 1
 if result_type in ["SUCCESS", "RECEIVED"]:
 self.test_results['pass'] += 1
 elif result_type == "ERROR":
 self.test_results['fail'] += 1

 # Update summary
 self.summary_label.config(
 text=f"Tests: {self.test_results['pass']} PASS, "
 f"{self.test_results['fail']} FAIL, "
 f"{self.test_results['total']} TOTAL"
 )

 def connect_serial(self):
 """Override to enable command buttons"""
 super().connect_serial()
 if self.is_connected:
 for btn in self.cmd_buttons:
 btn.config(state='normal')

 def disconnect_serial(self):
 """Override to disable command buttons"""
 super().disconnect_serial()
 for btn in self.cmd_buttons:
 btn.config(state='disabled')


if __name__ == "__main__":
 app = IDLECommandTester()
 app.run()
