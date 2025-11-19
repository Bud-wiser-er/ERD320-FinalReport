# Diagram Reorganization Summary

## What Changed

All PlantUML diagrams have been reorganized to make it easier to upgrade from PNG to PDF.

## New Structure

### Before:
```
01_SNC/
├── diagrams/
│   ├── diagram1.puml
│   ├── diagram1.png         ← Mixed together
│   ├── diagram2.puml
│   └── diagram2.png
└── images/
    ├── diagram3.png         ← Outputs in different location
    └── diagram4.png
```

### After:
```
01_SNC/
├── diagrams/
│   ├── diagram1/
│   │   ├── diagram1.puml    ← Source
│   │   └── out/
│   │       ├── diagram1.png ← Current output
│   │       └── diagram1.pdf ← Add PDF here later
│   ├── diagram2/
│   │   ├── diagram2.puml
│   │   └── out/
│   │       ├── diagram2.png
│   │       └── diagram2.pdf
│   └── README.md            ← NEW: Full instructions
└── images/
    ├── filter_fft_analysis.png      ← Lab results (not PlantUML)
    └── envelope_detector_output.png  ← Lab results (not PlantUML)
```

## All Reorganized Diagrams

✅ architecture_block
✅ architecture_interfaces
✅ architecture_main_esp32
✅ architecture_wifi_esp32
✅ conops_swimlane
✅ dataflow
✅ dataflow_main_control
✅ dataflow_remote_control
✅ dataflow_telemetry
✅ hmi_wireframe
✅ icd_sketch
✅ navcon_decision_logic
✅ navcon_state_machine
✅ objectives_tree
✅ pure_tone_detection_flow
✅ snc_architecture
✅ state_machine
✅ tone_detection_signal_chain

## Updated LaTeX Files

The following files have been updated with new image paths:

1. **concept_definition.tex**
   - `snc_architecture.png` → `snc_architecture/out/snc_architecture.png`
   - `architecture_block.png` → `architecture_block/out/architecture_block.png`

2. **engineering_design.tex**
   - `pure_tone_detection_flow.png` → `pure_tone_detection_flow/out/pure_tone_detection_flow.png`
   - `navcon_state_machine.png` → `navcon_state_machine/out/navcon_state_machine.png`
   - `navcon_decision_logic.png` → `navcon_decision_logic/out/navcon_decision_logic.png`

3. **README_DIAGRAMS.md**
   - Updated all example paths to new structure

## How to Upgrade PNG to PDF

### Quick Steps:

1. **Convert to PDF** (since PlantUML won't do it directly):
   - Use online PlantUML editor (http://www.plantuml.com/plantuml/)
   - Export as SVG
   - Convert SVG to PDF using Inkscape or online tool

2. **Place PDF in out/ folder**:
   ```
   diagrams/snc_architecture/out/
   ├── snc_architecture.png  ← Keep this
   └── snc_architecture.pdf  ← Add this
   ```

3. **Update LaTeX** (just change extension):
   ```latex
   % From:
   {01_SNC/diagrams/snc_architecture/out/snc_architecture.png}

   % To:
   {01_SNC/diagrams/snc_architecture/out/snc_architecture.pdf}
   ```

## Benefits

✅ PlantUML source next to output - easy to find and update
✅ PNG and PDF in same folder - just drop in the PDF
✅ Simple LaTeX update - change `.png` to `.pdf`
✅ Clean organization - each diagram self-contained
✅ Easy to maintain - clear structure for future updates

## Documentation

See `diagrams/README.md` for complete instructions on:
- Converting PlantUML to PDF
- Regenerating diagrams
- LaTeX integration
- Example workflows

## Notes

- Lab results/oscilloscope images remain in `images/` (they're not PlantUML)
- All SVG files also moved to respective `out/` folders
- Original structure preserved in git history
- All LaTeX references updated and verified
