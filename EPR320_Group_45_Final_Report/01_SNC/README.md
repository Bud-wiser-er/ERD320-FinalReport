# SNC Subsystem Documentation Structure

This folder contains all documentation and diagrams for the State-and-Navigation Control (SNC) subsystem.

## Folder Structure

```
01_SNC/
├── README.md                      # This file
├── generate_diagrams.bat          # Batch script to generate all diagrams
├── plantuml.jar                   # PlantUML tool (auto-downloaded)
│
├── diagrams/                      # PlantUML source files (.puml)
│   ├── objectives_tree.puml
│   ├── state_machine.puml
│   ├── icd_sketch.puml
│   ├── conops_swimlane.puml
│   ├── hmi_wireframe.puml
│   ├── architecture_block.puml         (legacy - single diagram)
│   ├── architecture_main_esp32.puml    (split version)
│   ├── architecture_wifi_esp32.puml    (split version)
│   ├── architecture_interfaces.puml    (split version)
│   ├── dataflow.puml                   (legacy - single diagram)
│   ├── dataflow_main_control.puml      (split version)
│   ├── dataflow_telemetry.puml         (split version)
│   └── dataflow_remote_control.puml    (split version)
│
├── images/                        # Generated PNG files (auto-generated)
│   ├── objectives_tree.png
│   ├── state_machine.png
│   ├── icd_sketch.png
│   ├── conops_swimlane.png
│   ├── hmi_wireframe.png
│   ├── architecture_*.png         (3 split + 1 legacy)
│   └── dataflow_*.png             (3 split + 1 legacy)
│
└── *.tex                          # LaTeX source files
    ├── title.tex
    ├── needs_analysis.tex
    ├── concept_definition.tex
    └── concept_exploration.tex
```

## Usage

### Generating Diagrams

1. Run `generate_diagrams.bat` (Windows)
2. All PNG files will be automatically generated in the `images/` folder
3. PlantUML will be auto-downloaded if not present

### Editing Diagrams

1. Edit `.puml` files in the `diagrams/` folder
2. Run `generate_diagrams.bat` to regenerate PNG files
3. LaTeX will automatically pick up updated images on next compile

### LaTeX Compilation

The LaTeX files reference images using:
```latex
\includegraphics{01_SNC/images/diagram_name.png}
```

No changes needed when regenerating diagrams - paths remain consistent.

## Diagram Organization

### Core Diagrams (5)
- **objectives_tree**: System objectives hierarchy
- **state_machine**: FSM with IDLE/CAL/MAZE/SOS states
- **icd_sketch**: Interface Control Document sketch
- **conops_swimlane**: Operational concept with color-coded swimlanes
- **hmi_wireframe**: Human-Machine Interface layout

### Architecture Diagrams (4)
Split for better readability:
1. **architecture_main_esp32**: Five-layer Main ESP32 architecture
2. **architecture_wifi_esp32**: WiFi ESP32 telemetry subsystem
3. **architecture_interfaces**: External interfaces and inter-MCU communication
4. **architecture_block**: *(legacy - single combined diagram)*

### Dataflow Diagrams (4)
Split for better readability:
1. **dataflow_main_control**: State management and NAVCON control loop
2. **dataflow_telemetry**: SPI telemetry to web dashboard
3. **dataflow_remote_control**: Remote command injection via GPIO
4. **dataflow**: *(legacy - single combined diagram)*

## Color Scheme

ConOps swimlane uses professional pastel colors:
- **Operator**: Light blue (#E8F4F8)
- **SNC**: Light yellow (#FFF8DC)
- **SS**: Light green (#E0FFE0)
- **MDPS**: Light pink (#FFE4E1)
- **HUB**: Light gold (#F0E68C)

## Notes

- Legacy diagrams kept for reference but not used in final report
- All diagrams sized to fit LaTeX page boundaries (0.88\textwidth × 0.65\textheight)
- Figures use `[h!]` placement with `\FloatBarrier` to prevent floating
- Total of 13 diagrams generated (9 active + 2 legacy + 2 core supporting)
