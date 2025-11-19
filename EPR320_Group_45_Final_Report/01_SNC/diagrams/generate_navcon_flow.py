#!/usr/bin/env python3
"""
Generate NAVCON Decision Flow Diagram
Shows the complete navigation decision logic with angle categorization and motion primitives
"""

from graphviz import Digraph

def create_navcon_flow():
    dot = Digraph(comment='NAVCON Decision Flow', format='png')
    dot.attr(rankdir='TB', size='10,14', dpi='300')
    dot.attr('node', shape='box', style='rounded,filled', fillcolor='lightblue',
             fontname='Arial', fontsize='11')

    # Start
    dot.node('start', 'NAVCON Decision\nCycle Start', shape='ellipse', fillcolor='lightgreen')

    # Input acquisition
    dot.node('input', 'Receive SCS Packet\nfrom Sensor Subsystem\n(Colors, Angle θᵢ, Distances)',
             fillcolor='lightyellow')

    # Line detection check
    dot.node('line_check', 'Line Detected\non Any Sensor?', shape='diamond', fillcolor='orange')
    dot.node('no_line', 'No Line Detected:\nContinue Forward Scan\n(FORWARD primitive)',
             fillcolor='#FFE6E6')

    # Line classification
    dot.node('classify', 'Classify Line Type\nby Color Code', fillcolor='lightyellow')
    dot.node('nav_check', 'Navigable Line?\n(RED or GREEN)', shape='diamond', fillcolor='orange')

    # Wall handling
    dot.node('wall', 'WALL Detected\n(BLACK or BLUE):\nObstacle Avoidance\nProtocol', fillcolor='#FFE6E6')

    # Angle categorization
    dot.node('angle_cat', 'Categorize\nApproach Angle θᵢ', fillcolor='lightyellow')

    # Three angle branches
    dot.node('angle_small', 'θᵢ ≤ 5°:\nDirect Crossing', shape='box', fillcolor='#E6F3FF')
    dot.node('angle_medium', '5° < θᵢ ≤ 45°:\nAlignment Required', shape='box', fillcolor='#E6F3FF')
    dot.node('angle_large', 'θᵢ > 45°:\nSteep Approach', shape='box', fillcolor='#E6F3FF')

    # Sensor position tracking
    dot.node('sensor_pos', 'Determine Active\nSensor Position\n(S1, S2, S3)', fillcolor='lightyellow')

    # Motion primitive selection
    dot.node('motion_select', 'Select Motion\nPrimitive Based on:\n• Line Type\n• Angle Category\n• Sensor Position',
             fillcolor='#FFE6CC')

    # Motion primitives
    dot.node('primitives', 'Motion Primitives:\n• FORWARD\n• REVERSE\n• ROTATE_LEFT\n• ROTATE_RIGHT\n• STOP\n• INCREMENTAL_CORRECTION',
             fillcolor='#E6FFE6')

    # Command encoding
    dot.node('encode', 'Encode SCS Command:\nDEC field + DATA bytes\n(Speed or Angle)',
             fillcolor='lightyellow')

    # Transmit
    dot.node('transmit', 'Transmit Command\nto MDPS via SCS', fillcolor='#E6F3FF')

    # Wait for completion
    dot.node('wait', 'Wait for MDPS\nCompletion Confirmation', fillcolor='lightyellow')

    # End
    dot.node('end', 'Return to\nFORWARD_SCAN State', shape='ellipse', fillcolor='lightgreen')

    # Edges
    dot.edge('start', 'input')
    dot.edge('input', 'line_check')
    dot.edge('line_check', 'no_line', label='No')
    dot.edge('line_check', 'classify', label='Yes')
    dot.edge('no_line', 'end')

    dot.edge('classify', 'nav_check')
    dot.edge('nav_check', 'wall', label='No\n(BLACK/BLUE)')
    dot.edge('nav_check', 'angle_cat', label='Yes\n(RED/GREEN)')
    dot.edge('wall', 'end')

    dot.edge('angle_cat', 'angle_small', label='≤5°')
    dot.edge('angle_cat', 'angle_medium', label='5-45°')
    dot.edge('angle_cat', 'angle_large', label='>45°')

    dot.edge('angle_small', 'sensor_pos')
    dot.edge('angle_medium', 'sensor_pos')
    dot.edge('angle_large', 'sensor_pos')

    dot.edge('sensor_pos', 'motion_select')
    dot.edge('motion_select', 'primitives')
    dot.edge('primitives', 'encode')
    dot.edge('encode', 'transmit')
    dot.edge('transmit', 'wait')
    dot.edge('wait', 'end')

    return dot

if __name__ == '__main__':
    diagram = create_navcon_flow()
    output_path = '/home/user/ERD320-SNC/Final Report/01_SNC/diagrams/navcon_decision_flow'
    diagram.render(output_path, cleanup=True)
    print(f"NAVCON decision flow diagram generated: {output_path}.png")
