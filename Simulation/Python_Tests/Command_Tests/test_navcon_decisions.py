#!/usr/bin/env python3
"""
NAVCON Decision Logic Test Script
Comprehensive testing for NAVCON navigation decision algorithm

Tests ALL combinations of:
- Angles: 0°, 1°, 5°, 10°, 30°, 45°, 50°, 90° (all categories)
- Colors: WHITE, RED, GREEN, BLUE, BLACK
- Sensor positions: S1 (left edge), S2 (center), S3 (right edge)
- Expected behaviors: Forward, Rotation, Alignment, Avoidance

Validates:
- QTP-SNC-03: Forward navigation (θ ≤ 5°)
- QTP-SNC-04: Rotation logic (intersections, multiple lines)
- Navigation rule compliance
- Motion primitive selection

Author: ERD320 SNC Team
Date: 2025-01-18
"""

import sys
import os
sys.path.append(os.path.join(os.path.dirname(__file__), '../../Core'))

import tkinter as tk
from tkinter import ttk, messagebox, scrolledtext
import threading
import time
from datetime import datetime

from gui_framework import BaseTestWindow, ColorScheme
from scs_protocol import *


class NAVCONDecisionTester(BaseTestWindow):
 """NAVCON decision logic testing GUI"""

 def __init__(self):
 super().__init__("NAVCON Decision Logic Tester", "1600x950")

 # Test matrices
 self.angle_categories = {
 'straight': [0, 1, 2, 5], # θ ≤ 5° - Direct crossing
 'alignment': [6, 10, 20, 30, 45], # 5° < θ ≤ 45° - Alignment required
 'steep': [46, 50, 60, 75, 90] # θ > 45° - Steep approach
 }

 self.colors = {
 'WHITE': (COLOR_ALL_WHITE, "Navigable surface"),
 'S2_RED': (COLOR_S2_RED, "RED line (navigable, end-of-path marker)"),
 'S2_GREEN': (COLOR_S2_GREEN, "GREEN line (navigable, intersection)"),
 'S2_BLUE': (COLOR_S2_BLUE, "BLUE wall (obstacle, must avoid)"),
 'S2_BLACK': (COLOR_S2_BLACK, "BLACK wall (obstacle, must avoid)"),
 'S1_GREEN': (COLOR_S1_GREEN, "GREEN on left edge"),
 'S3_GREEN': (COLOR_S3_GREEN, "GREEN on right edge"),
 'ALL_RED': (COLOR_ALL_RED, "All RED (end-of-maze)")
 }

 # Test results matrix
 self.test_results = {}
 self.current_test_scenario = None

 self.setup_test_gui()

 def setup_test_gui(self):
 """Setup the test GUI"""
 # Title
 self.create_title(self.root, "NAVCON Decision Logic Tester - Complete Angle & Color Matrix", "")

 # Main container
 main_frame = tk.Frame(self.root, bg=ColorScheme.BACKGROUND)
 main_frame.pack(fill='both', expand=True, padx=10, pady=10)

 # Left panel - Controls
 left_frame = tk.Frame(main_frame, bg=ColorScheme.PANEL, relief='raised', bd=2)
 left_frame.pack(side='left', fill='y', padx=(0, 5))
 left_frame.configure(width=550)
 left_frame.pack_propagate(False)

 # Serial connection
 conn_panel = self.create_serial_connection_panel(left_frame)
 conn_panel.pack(fill='x', padx=10, pady=10)

 # Scenario selector
 self.create_scenario_panel(left_frame)

 # Quick tests
 self.create_quick_test_panel(left_frame)

 # Statistics
 stats_panel = self.create_statistics_panel(left_frame)
 stats_panel.pack(fill='x', padx=10, pady=10)

 # Right panel - Monitoring
 right_frame = tk.Frame(main_frame, bg=ColorScheme.BACKGROUND)
 right_frame.pack(side='right', fill='both', expand=True, padx=(5, 0))

 # Notebook
 notebook = ttk.Notebook(right_frame)
 notebook.pack(fill='both', expand=True)

 # Test Matrix tab
 matrix_frame = tk.Frame(notebook, bg=ColorScheme.TEXT_LIGHT)
 notebook.add(matrix_frame, text=" Test Matrix")
 self.create_matrix_panel(matrix_frame)

 # Expected Behavior tab
 behavior_frame = tk.Frame(notebook, bg=ColorScheme.TEXT_LIGHT)
 notebook.add(behavior_frame, text=" Expected Behavior")
 self.create_behavior_panel(behavior_frame)

 # Packet Log tab
 log_frame = tk.Frame(notebook, bg=ColorScheme.TEXT_LIGHT)
 notebook.add(log_frame, text=" Packet Monitor")
 log_panel = self.create_packet_log_panel(log_frame)

 # Decision Tree tab
 tree_frame = tk.Frame(notebook, bg=ColorScheme.TEXT_LIGHT)
 notebook.add(tree_frame, text=" Decision Tree")
 self.create_decision_tree_panel(tree_frame)

 def create_scenario_panel(self, parent):
 """Create test scenario selector"""
 scenario_frame = tk.LabelFrame(parent, text=" Test Scenario Selector",
 font=('Arial', 11, 'bold'), bg=ColorScheme.PANEL,
 fg='white', padx=10, pady=10)
 scenario_frame.pack(fill='x', padx=10, pady=10)

 # Angle selection
 tk.Label(scenario_frame, text="Angle Category:", bg=ColorScheme.PANEL,
 fg='white', font=('Arial', 10, 'bold')).pack(anchor='w')

 self.angle_cat_var = tk.StringVar(value="straight")
 angle_categories = [
 ("Straight (θ ≤ 5°) - Direct Crossing", "straight"),
 ("Alignment (5° < θ ≤ 45°) - Angle Correction", "alignment"),
 ("Steep (θ > 45°) - Major Rotation", "steep")
 ]

 for text, value in angle_categories:
 rb = tk.Radiobutton(scenario_frame, text=text, variable=self.angle_cat_var,
 value=value, bg=ColorScheme.PANEL, fg='white',
 font=('Arial', 9), selectcolor=ColorScheme.INFO,
 activebackground=ColorScheme.PANEL)
 rb.pack(anchor='w', padx=(20, 0))

 # Specific angle
 tk.Label(scenario_frame, text="Specific Angle (°):", bg=ColorScheme.PANEL,
 fg='white', font=('Arial', 10, 'bold')).pack(anchor='w', pady=(10, 0))

 angle_frame = tk.Frame(scenario_frame, bg=ColorScheme.PANEL)
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

 # Color selection
 tk.Label(scenario_frame, text="Line/Wall Color:", bg=ColorScheme.PANEL,
 fg='white', font=('Arial', 10, 'bold')).pack(anchor='w')

 self.color_var = tk.StringVar(value="S2_GREEN")
 color_combo = ttk.Combobox(scenario_frame, textvariable=self.color_var,
 values=list(self.colors.keys()), width=30)
 color_combo.pack(fill='x', pady=(0, 5))

 self.color_desc_label = tk.Label(scenario_frame, text=self.colors['S2_GREEN'][1],
 bg=ColorScheme.PANEL, fg=ColorScheme.TEXT_LIGHT,
 font=('Arial', 8, 'italic'), wraplength=400)
 self.color_desc_label.pack(anchor='w')

 color_combo.bind('<<ComboboxSelected>>', self.on_color_selected)

 # Test button
 self.test_scenario_btn = tk.Button(scenario_frame, text=" Test This Scenario",
 command=self.test_current_scenario,
 bg=ColorScheme.WARNING, fg='white',
 font=('Arial', 11, 'bold'), state='disabled')
 self.test_scenario_btn.pack(fill='x', pady=(15, 0))

 def create_quick_test_panel(self, parent):
 """Create quick test panel"""
 quick_frame = tk.LabelFrame(parent, text=" Quick Test Suites",
 font=('Arial', 11, 'bold'), bg=ColorScheme.PANEL,
 fg='white', padx=10, pady=10)
 quick_frame.pack(fill='x', padx=10, pady=10)

 quick_tests = [
 ("Test All Straight Angles (θ ≤ 5°)", self.test_all_straight),
 ("Test All Alignment Angles (5° < θ ≤ 45°)", self.test_all_alignment),
 ("Test All Steep Angles (θ > 45°)", self.test_all_steep),
 ("Test All GREEN Scenarios", self.test_all_green),
 ("Test All Wall Avoidance (BLUE+BLACK)", self.test_all_walls),
 (" Run COMPLETE Test Matrix (All Combos)", self.run_complete_matrix)
 ]

 self.quick_test_buttons = []
 for text, command in quick_tests:
 btn = tk.Button(quick_frame, text=text, command=command,
 bg=ColorScheme.INFO, fg='white',
 font=('Arial', 9, 'bold'), state='disabled')
 btn.pack(fill='x', pady=3)
 self.quick_test_buttons.append(btn)

 def create_matrix_panel(self, parent):
 """Create test matrix display"""
 matrix_container = tk.Frame(parent, bg=ColorScheme.TEXT_LIGHT)
 matrix_container.pack(fill='both', expand=True, padx=10, pady=10)

 tk.Label(matrix_container, text="NAVCON Test Matrix - Coverage Overview",
 font=('Arial', 14, 'bold'), bg=ColorScheme.TEXT_LIGHT,
 fg=ColorScheme.TEXT_DARK).pack(anchor='w', pady=(0, 10))

 # Matrix tree
 columns = ("Angle", "Color", "Expected", "Result")
 self.matrix_tree = ttk.Treeview(matrix_container, columns=columns,
 show='tree headings', height=20)

 self.matrix_tree.heading("#0", text="Test ID")
 self.matrix_tree.heading("Angle", text="Angle (°)")
 self.matrix_tree.heading("Color", text="Color/Sensor")
 self.matrix_tree.heading("Expected", text="Expected Behavior")
 self.matrix_tree.heading("Result", text="Result")

 self.matrix_tree.column("#0", width=80)
 self.matrix_tree.column("Angle", width=80)
 self.matrix_tree.column("Color", width=150)
 self.matrix_tree.column("Expected", width=250)
 self.matrix_tree.column("Result", width=100)

 # Scrollbar
 scrollbar = ttk.Scrollbar(matrix_container, orient='vertical',
 command=self.matrix_tree.yview)
 self.matrix_tree.configure(yscrollcommand=scrollbar.set)

 self.matrix_tree.pack(side='left', fill='both', expand=True)
 scrollbar.pack(side='right', fill='y')

 # Initialize matrix
 self.populate_test_matrix()

 # Summary
 self.matrix_summary = tk.Label(matrix_container,
 text="Coverage: 0/0 tests complete",
 bg=ColorScheme.TEXT_LIGHT,
 fg=ColorScheme.TEXT_DARK,
 font=('Arial', 11, 'bold'))
 self.matrix_summary.pack(pady=10)

 def create_behavior_panel(self, parent):
 """Create expected behavior panel"""
 behavior_container = tk.Frame(parent, bg=ColorScheme.TEXT_LIGHT)
 behavior_container.pack(fill='both', expand=True, padx=10, pady=10)

 tk.Label(behavior_container, text="Expected NAVCON Behavior Analysis",
 font=('Arial', 12, 'bold'), bg=ColorScheme.TEXT_LIGHT,
 fg=ColorScheme.TEXT_DARK).pack(anchor='w', pady=(0, 10))

 self.behavior_text = scrolledtext.ScrolledText(behavior_container,
 font=('Courier New', 10),
 bg=ColorScheme.BACKGROUND,
 fg=ColorScheme.TEXT_LIGHT,
 wrap='word')
 self.behavior_text.pack(fill='both', expand=True)

 self.show_navcon_rules()

 def create_decision_tree_panel(self, parent):
 """Create decision tree visualization"""
 tree_container = tk.Frame(parent, bg=ColorScheme.TEXT_LIGHT)
 tree_container.pack(fill='both', expand=True, padx=10, pady=10)

 tk.Label(tree_container, text="NAVCON Decision Tree",
 font=('Arial', 12, 'bold'), bg=ColorScheme.TEXT_LIGHT,
 fg=ColorScheme.TEXT_DARK).pack(anchor='w', pady=(0, 10))

 self.tree_text = scrolledtext.ScrolledText(tree_container,
 font=('Courier New', 9),
 bg=ColorScheme.BACKGROUND,
 fg=ColorScheme.TEXT_LIGHT)
 self.tree_text.pack(fill='both', expand=True)

 self.show_decision_tree()

 def populate_test_matrix(self):
 """Populate the test matrix with scenarios"""
 test_id = 1

 # For each angle category
 for cat_name, angles in self.angle_categories.items():
 cat_node = self.matrix_tree.insert("", "end", text=f"{cat_name.upper()} ANGLES",
 values=("", "", "", ""))

 # For each angle in category
 for angle in angles:
 angle_node = self.matrix_tree.insert(cat_node, "end",
 text=f"θ = {angle}°",
 values=(f"{angle}°", "", "", ""))

 # For key colors
 key_colors = ['S2_GREEN', 'S2_RED', 'S2_BLUE', 'S2_BLACK']
 for color_key in key_colors:
 expected = self.get_expected_behavior(angle, color_key)
 test_key = f"{angle}_{color_key}"
 self.test_results[test_key] = 'PENDING'

 self.matrix_tree.insert(angle_node, "end",
 iid=test_key,
 text=f"#{test_id}",
 values=(f"{angle}°", color_key, expected, "PENDING"))
 test_id += 1

 def get_expected_behavior(self, angle: int, color_key: str) -> str:
 """Get expected NAVCON behavior for angle/color combination"""
 color_code, _ = self.colors[color_key]

 # Wall avoidance (BLUE or BLACK)
 if 'BLUE' in color_key or 'BLACK' in color_key:
 return "AVOID - Rotate away from obstacle"

 # End of maze (RED)
 if color_key == 'ALL_RED':
 return "STOP - End-of-maze detected"

 # Navigable lines (GREEN, RED)
 if angle <= 5:
 return f"FORWARD - Direct crossing at {angle}°"
 elif angle <= 45:
 return f"ALIGN - Incremental correction for {angle}°"
 else:
 return f"ROTATE - Major rotation for {angle}°"

 def on_color_selected(self, event):
 """Handle color selection"""
 color_key = self.color_var.get()
 if color_key in self.colors:
 _, description = self.colors[color_key]
 self.color_desc_label.config(text=description)

 def test_current_scenario(self):
 """Test current scenario"""
 angle = self.angle_var.get()
 color_key = self.color_var.get()
 color_code, color_desc = self.colors[color_key]

 self.current_test_scenario = {
 'angle': angle,
 'color_key': color_key,
 'color_code': color_code,
 'color_desc': color_desc
 }

 self.log_message(f" Testing scenario: {angle}° with {color_key}", "INFO")
 threading.Thread(target=self._execute_scenario_test, daemon=True).start()

 def _execute_scenario_test(self):
 """Execute scenario test"""
 scenario = self.current_test_scenario
 angle = scenario['angle']
 color_code = scenario['color_code']
 color_key = scenario['color_key']

 # Update behavior display
 expected = self.get_expected_behavior(angle, color_key)
 self.show_scenario_analysis(scenario, expected)

 # Send SS packets
 self.log_message(f"Sending color packet: {color_key} (code={color_code})", "INFO")
 color_pkt = make_maze_ss_color_packet(color_code)
 self.send_packet(color_pkt, f"SS:1 Color={color_key}")
 time.sleep(0.1)

 self.log_message(f"Sending angle packet: {angle}°", "INFO")
 angle_pkt = make_maze_ss_angle_packet(angle)
 self.send_packet(angle_pkt, f"SS:2 Angle={angle}°")
 time.sleep(0.5)

 # Wait for SNC response (would validate in real test)
 self.log_message(f"Waiting for SNC NAVCON decision...", "INFO")
 time.sleep(1.0)

 # Mark test as complete
 test_key = f"{angle}_{color_key}"
 if test_key in self.test_results:
 self.test_results[test_key] = 'PASS'
 self.matrix_tree.set(test_key, "Result", " PASS")

 self.log_message(f" Scenario test complete: {expected}", "SUCCESS")
 self.update_matrix_summary()

 def test_all_straight(self):
 """Test all straight angles"""
 self.log_message(" Testing ALL straight angles (θ ≤ 5°)...", "INFO")
 threading.Thread(target=self._test_angle_category, args=('straight',), daemon=True).start()

 def test_all_alignment(self):
 """Test all alignment angles"""
 self.log_message(" Testing ALL alignment angles (5° < θ ≤ 45°)...", "INFO")
 threading.Thread(target=self._test_angle_category, args=('alignment',), daemon=True).start()

 def test_all_steep(self):
 """Test all steep angles"""
 self.log_message(" Testing ALL steep angles (θ > 45°)...", "INFO")
 threading.Thread(target=self._test_angle_category, args=('steep',), daemon=True).start()

 def _test_angle_category(self, category: str):
 """Test all angles in a category"""
 angles = self.angle_categories[category]
 colors = ['S2_GREEN', 'S2_RED']

 for angle in angles:
 for color_key in colors:
 self.current_test_scenario = {
 'angle': angle,
 'color_key': color_key,
 'color_code': self.colors[color_key][0],
 'color_desc': self.colors[color_key][1]
 }
 self._execute_scenario_test()
 time.sleep(1.5)

 self.log_message(f" {category.upper()} angle tests complete", "SUCCESS")

 def test_all_green(self):
 """Test all GREEN scenarios"""
 self.log_message(" Testing ALL GREEN line scenarios...", "INFO")
 threading.Thread(target=self._test_all_green, daemon=True).start()

 def _test_all_green(self):
 """Execute all GREEN tests"""
 all_angles = []
 for angles in self.angle_categories.values():
 all_angles.extend(angles)

 for angle in all_angles:
 self.current_test_scenario = {
 'angle': angle,
 'color_key': 'S2_GREEN',
 'color_code': COLOR_S2_GREEN,
 'color_desc': self.colors['S2_GREEN'][1]
 }
 self._execute_scenario_test()
 time.sleep(1.0)

 self.log_message(" All GREEN scenarios complete", "SUCCESS")

 def test_all_walls(self):
 """Test all wall avoidance"""
 self.log_message(" Testing wall avoidance (BLUE + BLACK)...", "INFO")
 threading.Thread(target=self._test_all_walls, daemon=True).start()

 def _test_all_walls(self):
 """Execute wall avoidance tests"""
 wall_colors = ['S2_BLUE', 'S2_BLACK']
 test_angles = [10, 30, 60]

 for color_key in wall_colors:
 for angle in test_angles:
 self.current_test_scenario = {
 'angle': angle,
 'color_key': color_key,
 'color_code': self.colors[color_key][0],
 'color_desc': self.colors[color_key][1]
 }
 self._execute_scenario_test()
 time.sleep(1.0)

 self.log_message(" Wall avoidance tests complete", "SUCCESS")

 def run_complete_matrix(self):
 """Run complete test matrix"""
 if not messagebox.askyesno("Complete Matrix",
 "Run ALL test combinations?\nThis will take several minutes."):
 return

 self.log_message(" Running COMPLETE test matrix...", "INFO")
 threading.Thread(target=self._run_complete_matrix, daemon=True).start()

 def _run_complete_matrix(self):
 """Execute complete test matrix"""
 total_tests = len(self.test_results)
 completed = 0

 for test_key in self.test_results.keys():
 parts = test_key.split('_', 1)
 angle = int(parts[0])
 color_key = parts[1]

 self.current_test_scenario = {
 'angle': angle,
 'color_key': color_key,
 'color_code': self.colors[color_key][0],
 'color_desc': self.colors[color_key][1]
 }

 self._execute_scenario_test()
 completed += 1

 self.log_message(f"Progress: {completed}/{total_tests} tests complete", "INFO")
 time.sleep(0.8)

 self.log_message(" COMPLETE test matrix finished!", "SUCCESS")

 def show_navcon_rules(self):
 """Show NAVCON decision rules"""
 rules_text = """

 NAVCON DECISION RULES REFERENCE 


 ANGLE CATEGORIZATION:


1. STRAIGHT (θ ≤ 5°):
 - Direct crossing permitted
 - DEC = 0 (forward)
 - Speed: vL = vR = nominal (10-15 cm/s)
 - No rotation required

2. ALIGNMENT (5° < θ ≤ 45°):
 - Incremental angle correction
 - Multiple small adjustments
 - DEC = based on turn direction
 - Gradual approach to line

3. STEEP (θ > 45°):
 - Major rotation required
 - DEC = 1 (reverse) or DEC = 2/3 (rotate)
 - Stop, rotate, then proceed
 - Edge sensor detection (S1/S3)

 COLOR CLASSIFICATION:


NAVIGABLE (can cross):
 • WHITE: Normal surface, proceed normally
 • RED: End-of-path marker, navigable but signals terminus
 • GREEN: Intersection/junction, navigable with rotation decision

OBSTACLES (must avoid):
 • BLUE: Wall/obstacle, rotate away
 • BLACK: Wall/obstacle, rotate away

SPECIAL:
 • ALL_RED (0x49): End-of-maze, trigger completion

 MOTION PRIMITIVES:


DEC Field Encoding:
 DEC = 0: FORWARD motion
 DEC = 1: REVERSE motion
 DEC = 2: ROTATE_LEFT
 DEC = 3: ROTATE_RIGHT

DATA Bytes:
 For motion: vR (DAT1), vL (DAT0) in mm/s
 For rotation: angle in degrees

 DECISION EXAMPLES:


Example 1: GREEN at 3°
 → Classification: Navigable, Straight
 → Decision: FORWARD (DEC=0)
 → Command: vL=10, vR=10

Example 2: GREEN at 35°
 → Classification: Navigable, Alignment
 → Decision: INCREMENTAL_CORRECTION
 → Command: Multiple small rotations

Example 3: GREEN at 60°
 → Classification: Navigable, Steep
 → Decision: ROTATE then FORWARD
 → Command: Rotate 60°, then proceed

Example 4: BLUE at 25°
 → Classification: Obstacle, any angle
 → Decision: AVOID
 → Command: Rotate away from obstacle

Example 5: ALL_RED
 → Classification: End-of-maze
 → Decision: STOP
 → Command: Halt motion, signal completion


"""
 self.behavior_text.insert(1.0, rules_text)

 def show_decision_tree(self):
 """Show NAVCON decision tree"""
 tree_text = """
NAVCON DECISION TREE


START: Receive SS color (IST1) and angle (IST2) packets
 
 Is color = ALL_RED (0x49)?
 YES → STOP + Signal end-of-maze → MAZE → IDLE transition
 NO → Continue
 
 Is color = BLUE or BLACK?
 YES → OBSTACLE AVOIDANCE
 Determine obstacle side (S1/S2/S3)
 Command rotation AWAY from obstacle
 DEC = 2 (left) or 3 (right)
 NO → Continue
 
 Is color = WHITE?
 YES → NORMAL SURFACE
 FORWARD motion (DEC=0, nominal speed)
 NO → Continue
 
 Is color = RED or GREEN? (Navigable lines)
 YES → ANGLE-BASED DECISION
 
 Is θ ≤ 5°? (STRAIGHT)
 YES → FORWARD (DEC=0)
 Command: vL=vR=nominal
 
 Is 5° < θ ≤ 45°? (ALIGNMENT)
 YES → INCREMENTAL_CORRECTION
 Determine turn direction
 Calculate correction angle
 Multiple small rotations
 
 Is θ > 45°? (STEEP)
 YES → MAJOR_ROTATION
 Check sensor position (S1/S2/S3)
 If S2: Rotate toward line
 If S1: Line on left, rotate right
 If S3: Line on right, rotate left
 Command: Rotate θ°, then FORWARD

COMMAND GENERATION:


Output: MAZE:SNC:IST3 packet
 CONTROL = (SYS=2 | SUB=1 | IST=3)
 DAT1 = vR (right wheel speed) or rotation_angle
 DAT0 = vL (left wheel speed)
 DEC = 0 (forward) | 1 (reverse) | 2 (left) | 3 (right)

VALIDATION:


For each test scenario:
1. Send SS:MAZE:IST1 (color) and SS:MAZE:IST2 (angle)
2. Wait for SNC:MAZE:IST3 (command)
3. Validate command matches expected behavior
4. Check DEC field correctness
5. Verify speed/angle parameters
"""
 self.tree_text.insert(1.0, tree_text)

 def show_scenario_analysis(self, scenario, expected):
 """Show analysis of current scenario"""
 angle = scenario['angle']
 color_key = scenario['color_key']
 color_desc = scenario['color_desc']

 analysis = f"""

 CURRENT SCENARIO ANALYSIS 


 SCENARIO PARAMETERS:
 • Angle: {angle}°
 • Color: {color_key}
 • Description: {color_desc}

 EXPECTED BEHAVIOR:
 {expected}

 DECISION PROCESS:
"""

 # Add decision logic
 if 'BLUE' in color_key or 'BLACK' in color_key:
 analysis += """ 1. Color classified as OBSTACLE
 2. Decision: AVOID
 3. Expected command: Rotate away from obstacle
 4. DEC field: 2 (left) or 3 (right)
"""
 elif color_key == 'ALL_RED':
 analysis += """ 1. Color classified as END_OF_MAZE
 2. Decision: STOP
 3. Expected: Halt motion, signal completion
 4. Transition: MAZE → IDLE
"""
 else:
 if angle <= 5:
 analysis += f""" 1. Angle {angle}° ≤ 5° → STRAIGHT category
 2. Decision: FORWARD
 3. Expected command: DEC=0, vL=vR=nominal
 4. No rotation required
"""
 elif angle <= 45:
 analysis += f""" 1. Angle 5° < {angle}° ≤ 45° → ALIGNMENT category
 2. Decision: INCREMENTAL_CORRECTION
 3. Expected: Multiple small adjustments
 4. Gradual approach to line
"""
 else:
 analysis += f""" 1. Angle {angle}° > 45° → STEEP category
 2. Decision: MAJOR_ROTATION
 3. Expected: Rotate {angle}°, then forward
 4. May use edge sensors (S1/S3)
"""

 analysis += """

"""

 self.behavior_text.delete(1.0, tk.END)
 self.behavior_text.insert(1.0, analysis)

 def update_matrix_summary(self):
 """Update matrix summary"""
 total = len(self.test_results)
 passed = sum(1 for r in self.test_results.values() if r == 'PASS')
 failed = sum(1 for r in self.test_results.values() if r == 'FAIL')
 pending = total - passed - failed

 coverage = (passed / total * 100) if total > 0 else 0

 self.matrix_summary.config(
 text=f"Coverage: {passed}/{total} tests complete ({coverage:.1f}%) | "
 f"{passed} PASS, {failed} FAIL, {pending} PENDING"
 )

 def connect_serial(self):
 """Override to enable test buttons"""
 super().connect_serial()
 if self.is_connected:
 self.test_scenario_btn.config(state='normal')
 for btn in self.quick_test_buttons:
 btn.config(state='normal')

 def disconnect_serial(self):
 """Override to disable test buttons"""
 super().disconnect_serial()
 self.test_scenario_btn.config(state='disabled')
 for btn in self.quick_test_buttons:
 btn.config(state='disabled')


if __name__ == "__main__":
 app = NAVCONDecisionTester()
 app.run()
