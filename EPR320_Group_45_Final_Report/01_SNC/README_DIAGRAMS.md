# SNC Subsystem Diagrams

This directory contains PlantUML source files and generated diagrams for the SNC subsystem needs analysis.

## Directory Structure

```
01_SNC/
├── images/                      # Generated PNG images
│   ├── objectives_tree.png
│   ├── state_machine.png
│   └── icd_sketch.png
├── objectives_tree.puml         # PlantUML source files
├── state_machine.puml
├── icd_sketch.puml
├── plantuml.jar                 # PlantUML compiler
├── generate_diagrams.bat        # Regeneration script
└── README_DIAGRAMS.md           # This file
```

## Diagram Files

### Source Files (PlantUML)
- `objectives_tree.puml` - Objectives tree showing O1-O4 decomposition
- `state_machine.puml` - State machine diagram (IDLE/CAL/MAZE/SOS)
- `icd_sketch.puml` - Interface Control Document byte structure
- `conops_swimlane.puml` - Concept of Operations swimlane diagram
- `architecture_block.puml` - System architecture with 5 functional layers
- `dataflow.puml` - Logical dataflow diagram
- `hmi_wireframe.puml` - Human-Machine Interface wireframe

### Generated Images (in `images/` folder)
- `images/objectives_tree.png` (and .svg)
- `images/state_machine.png` (and .svg)
- `images/icd_sketch.png` (and .svg)
- `images/conops_swimlane.png` (and .svg)
- `images/architecture_block.png` (and .svg)
- `images/dataflow.png` (and .svg)
- `images/hmi_wireframe.png` (and .svg)

## Regenerating Diagrams

If you need to modify the diagrams, edit the `.puml` files and regenerate:

### Method 1: Using the Batch Script (Windows)
```batch
generate_diagrams.bat
```

### Method 2: Manual Generation (PNG)
```batch
java -jar plantuml.jar -tpng objectives_tree.puml state_machine.puml icd_sketch.puml conops_swimlane.puml architecture_block.puml dataflow.puml hmi_wireframe.puml
```

### Method 3: Generate SVG (Vector Format)
```batch
java -jar plantuml.jar -tsvg objectives_tree.puml state_machine.puml icd_sketch.puml conops_swimlane.puml architecture_block.puml dataflow.puml hmi_wireframe.puml
```

### Method 4: Online PlantUML Editor
1. Go to http://www.plantuml.com/plantuml/uml/
2. Copy/paste the contents of any `.puml` file
3. Download the generated image

## Requirements

- Java Runtime Environment (JRE) - Already installed on your system
- `plantuml.jar` - Already downloaded in this directory

## Diagram Descriptions

### 1. Objectives Tree (`objectives_tree.puml`)
Shows the hierarchical breakdown of the four primary SNC objectives:
- O1: State Control & Communications
- O2: Navigation Control
- O3: Safety & HMI
- O4: Testability & Verification

### 2. State Machine (`state_machine.puml`)
Displays the SNC state machine with:
- Four states: IDLE, CAL, MAZE, SOS
- Guarded transitions with conditions
- State-specific behaviors and notes

### 3. ICD Sketch (`icd_sketch.puml`)
Illustrates the SCS protocol structure:
- CONTROL byte (SYS, SUB, IST fields)
- DATA byte (DAT1, DAT0, DEC fields)
- Command encoding examples
- Data consumption from MDPS and SS subsystems

### 4. ConOps Swimlane (`conops_swimlane.puml`)
Shows operational concept across subsystems:
- Swimlanes for Operator, SNC, SS, MDPS
- State transitions (IDLE → CAL → MAZE → IDLE)
- SOS toggle mechanism
- End-of-Calibration and End-of-Maze flows

### 5. Architecture Block Diagram (`architecture_block.puml`)
Displays layered architecture:
- Layer 1: State Management Core
- Layer 2: Navigation Decision (NAVCON)
- Layer 3: Communication Protocol (SCS)
- Layer 4: Human-Machine Interface
- Layer 5: Supervision and Diagnostics
- External interfaces to SS, MDPS, Operator, HUB

### 6. Dataflow Diagram (`dataflow.puml`)
Illustrates information flows:
- Inputs from SS, MDPS, and Operator
- Internal processing through NAVCON, State Manager, SCS Handler
- Episode state storage
- Outputs to MDPS and Display

### 7. HMI Wireframe (`hmi_wireframe.puml`)
Shows interface layout:
- State indicator (IDLE/CAL/MAZE/SOS)
- Sensor data fields (colors, angle, EoM)
- Motion parameters (speeds, rotation, distance)
- Operator controls (touch, tone)

## Integration with LaTeX

The diagrams are referenced in `needs_analysis.tex` and `concept_definition.tex` as:
```latex
% Needs Analysis diagrams
\includegraphics[width=0.85\textwidth]{01_SNC/diagrams/objectives_tree/out/objectives_tree.png}
\includegraphics[width=0.9\textwidth]{01_SNC/diagrams/state_machine/out/state_machine.png}
\includegraphics[width=0.9\textwidth]{01_SNC/diagrams/icd_sketch/out/icd_sketch.png}

% Concept Definition diagrams
\includegraphics[width=0.95\textwidth]{01_SNC/diagrams/conops_swimlane/out/conops_swimlane.png}
\includegraphics[width=0.98\textwidth]{01_SNC/diagrams/architecture_block/out/architecture_block.png}
\includegraphics[width=0.98\textwidth]{01_SNC/diagrams/dataflow/out/dataflow.png}
\includegraphics[width=0.85\textwidth]{01_SNC/diagrams/hmi_wireframe/out/hmi_wireframe.png}
```

**Note:** All generated PNG and SVG files are stored in the `images/` subdirectory to maintain a clean project structure. Both PNG (raster) and SVG (vector) formats are available; PNG is used in the LaTeX document for compatibility with pdflatex.

## Tips for Editing

- **PlantUML Syntax**: See https://plantuml.com/ for complete documentation
- **State Diagrams**: Use `@startuml` and `@enduml` tags
- **WBS Diagrams**: Use `@startwbs` and `@endwbs` tags
- **Colors**: Use `BackgroundColor`, `BorderColor` in skinparam
- **Icons**: Use `<&icon-name>` for OpenIconic icons
