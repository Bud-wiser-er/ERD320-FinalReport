# SNC Diagrams - PlantUML Source and Outputs

This directory contains all PlantUML diagrams for the SNC subsystem, organized for easy maintenance and high-resolution PDF export.

## Directory Structure

Each diagram has its own folder with the following structure:

```
diagrams/
├── diagram_name/
│   ├── diagram_name.puml       # PlantUML source code
│   └── out/
│       ├── diagram_name.png    # Current PNG output
│       └── diagram_name.pdf    # High-resolution PDF (add manually)
```

### Example:
```
snc_architecture/
├── snc_architecture.puml
└── out/
    ├── snc_architecture.png
    └── snc_architecture.pdf    # You'll add this after conversion
```

## Available Diagrams

- `architecture_block/` - System architecture with 5 functional layers
- `architecture_interfaces/` - External interface connections
- `architecture_main_esp32/` - Main ESP32 internal architecture
- `architecture_wifi_esp32/` - WiFi ESP32 internal architecture
- `conops_swimlane/` - Concept of Operations swimlane diagram
- `dataflow/` - Logical dataflow diagram
- `dataflow_main_control/` - Main control dataflow
- `dataflow_remote_control/` - Remote control dataflow
- `dataflow_telemetry/` - Telemetry dataflow
- `hmi_wireframe/` - Human-Machine Interface wireframe
- `icd_sketch/` - Interface Control Document byte structure
- `navcon_decision_logic/` - Navigation decision flow logic
- `navcon_state_machine/` - Navigation state machine
- `objectives_tree/` - Objectives tree (O1-O4 decomposition)
- `pure_tone_detection_flow/` - Pure tone detection signal chain
- `snc_architecture/` - SNC hardware/software architecture
- `state_machine/` - Main state machine (IDLE/CAL/MAZE/SOS)
- `tone_detection_signal_chain/` - Tone detection signal processing

## Upgrading PNG to PDF (High-Resolution)

### Step 1: Convert PlantUML to PDF

Since PlantUML may not directly export to PDF at the moment, follow these manual conversion steps:

#### Option A: Use PlantUML Web Server
1. Go to http://www.plantuml.com/plantuml/uml/
2. Copy/paste the contents of the `.puml` file (e.g., `snc_architecture/snc_architecture.puml`)
3. Click the "PNG" dropdown and select "SVG"
4. Download the SVG file
5. Convert SVG to PDF using one of these tools:
   - Inkscape (free): File → Save As → PDF
   - Online converter: https://cloudconvert.com/svg-to-pdf
   - Adobe Illustrator

#### Option B: Use PlantUML Command Line with SVG
```bash
# Generate SVG first (already have plantuml.jar in parent directory)
java -jar ../plantuml.jar -tsvg diagram_name/diagram_name.puml -o out

# Then convert SVG to PDF using Inkscape (if installed)
inkscape diagram_name/out/diagram_name.svg --export-pdf=diagram_name/out/diagram_name.pdf
```

#### Option C: Use Online PDF Export Services
1. Use PlantUML online editor at http://www.plantuml.com/plantuml/
2. Use "Export as PDF" if available
3. Or export as SVG and use https://cloudconvert.com/svg-to-pdf

### Step 2: Place PDF in the Same Directory as PNG

After conversion, place the PDF in the `out/` folder alongside the PNG:

```
snc_architecture/
├── snc_architecture.puml
└── out/
    ├── snc_architecture.png    ← Existing PNG
    └── snc_architecture.pdf    ← NEW: Your converted PDF
```

### Step 3: Update LaTeX Code

Simply change the file extension from `.png` to `.pdf` in your LaTeX file:

**Before:**
```latex
\includegraphics[width=1.05\textwidth]{01_SNC/diagrams/snc_architecture/out/snc_architecture.png}
```

**After:**
```latex
\includegraphics[width=1.05\textwidth]{01_SNC/diagrams/snc_architecture/out/snc_architecture.pdf}
```

That's it! The path stays the same - just change the extension.

## Regenerating PNG Diagrams from PlantUML

If you need to update a diagram, edit the `.puml` file and regenerate:

### Single Diagram
```bash
java -jar ../plantuml.jar diagram_name/diagram_name.puml -o out
```

### All Diagrams
```bash
java -jar ../plantuml.jar */*.puml -o out
```

### Generate SVG Instead
```bash
java -jar ../plantuml.jar -tsvg diagram_name/diagram_name.puml -o out
```

## LaTeX Integration

All diagrams are referenced in the LaTeX files with this pattern:

```latex
\includegraphics[width=X\textwidth]{01_SNC/diagrams/diagram_name/out/diagram_name.png}
```

To use high-resolution PDFs, just change `.png` to `.pdf`:

```latex
\includegraphics[width=X\textwidth]{01_SNC/diagrams/diagram_name/out/diagram_name.pdf}
```

## Benefits of This Structure

✅ **Easy to find source**: PlantUML source is right next to its output
✅ **Easy to upgrade**: Drop PDF in the same folder as PNG
✅ **Easy to switch**: Change `.png` to `.pdf` in LaTeX
✅ **Clean separation**: Source (.puml) separate from outputs (out/)
✅ **Version control friendly**: Each diagram is self-contained

## Tips

1. **Keep both PNG and PDF**: Useful for quick previews (PNG) and final high-quality output (PDF)
2. **SVG files**: Also saved in `out/` folders - can be used for further editing
3. **Regeneration**: When you update a `.puml` file, remember to regenerate both PNG and PDF/SVG
4. **LaTeX compilation**: pdflatex handles PDF images better than PNG for scaling

## Example Workflow: Updating a Diagram

1. Edit the PlantUML source:
   ```bash
   # Edit the source file
   notepad diagrams/snc_architecture/snc_architecture.puml
   ```

2. Regenerate PNG:
   ```bash
   java -jar plantuml.jar snc_architecture/snc_architecture.puml -o out
   ```

3. Generate SVG and convert to PDF:
   ```bash
   java -jar plantuml.jar -tsvg snc_architecture/snc_architecture.puml -o out
   # Then convert SVG to PDF using your preferred method
   ```

4. Update LaTeX (if switching to PDF):
   ```latex
   % Change from:
   {01_SNC/diagrams/snc_architecture/out/snc_architecture.png}
   % To:
   {01_SNC/diagrams/snc_architecture/out/snc_architecture.pdf}
   ```

## Notes

- The `images/` folder still contains lab results and oscilloscope screenshots (filter_fft_analysis.png, envelope_detector_output.png) - these are NOT PlantUML diagrams and remain separate
- PlantUML JAR is located in the parent directory: `Final Report/01_SNC/`
- All diagram folders follow the same structure for consistency
