# Software Architecture Diagrams

This folder should contain **software architecture diagram(s)** for the Engineering Design section.

## Option 1: Software Architecture Block Diagram (Recommended)
**Filename:** `snc_firmware_architecture.png` or `.pdf`

**What to create:**
A hierarchical block diagram showing the Main ESP32 firmware modules and data flow:

```
┌─────────────────────────────────────────────────────────┐
│          Main ESP32 Firmware Architecture                │
├─────────────────────────────────────────────────────────┤
│                                                           │
│  ┌────────────────┐         ┌────────────────┐          │
│  │ StateMachine   │◄────────│  PureTone      │          │
│  │  .h/.cpp       │  flag   │   .h/.cpp      │◄─── ADC  │
│  └────────┬───────┘         └────────────────┘   GPIO36 │
│           │ state                                        │
│           ▼                                              │
│  ┌────────────────┐         ┌────────────────┐          │
│  │   NAVCON       │◄────────│ SCS_Protocol   │◄─── UART │
│  │  .h/.cpp       │  data   │   .h/.cpp      │    RX/TX │
│  └────────┬───────┘         └────────┬───────┘          │
│           │ commands                 │ packets          │
│           │                          │                  │
│           └──────────┬───────────────┘                  │
│                      ▼                                  │
│           ┌────────────────────┐                        │
│           │  SPI_Telemetry     │──────► WiFi ESP32     │
│           │    .h/.cpp         │  SPI                   │
│           └────────────────────┘  DMA                   │
│                                                          │
└──────────────────────────────────────────────────────────┘
```

**Key elements:**
- Module boxes showing .h/.cpp separation
- Arrow directions showing data flow
- Interface labels (UART, SPI, ADC, GPIO)
- Clear hierarchical control flow (StateMachine → NAVCON → Protocol)
- External connections (WiFi ESP32, sensor inputs)

**Tools you can use:**
- draw.io (free online)
- Microsoft Visio
- PowerPoint/Keynote
- LaTeX TikZ (if you're comfortable)
- Lucidchart (free tier)

**Caption suggestion:** "Main ESP32 firmware architecture showing modular design with separated header/implementation files, hierarchical control flow from state machine through navigation logic to protocol handling, and SPI telemetry interface to WiFi ESP32"

---

## Option 2: Data Flow Diagram (Alternative)
**Filename:** `snc_data_flow.png` or `.pdf`

Shows how data moves through the system:
- UART packets → SCS_Protocol parser → NAVCON logic → State machine
- Touch sensor → State machine
- Pure tone detector → State machine
- State machine + NAVCON → SPI telemetry

---

## Option 3: Code Snippet Alternative

If you prefer **code snippets** instead of a diagram, add 1-2 key code examples to `engineering_design.tex`:

### Example 1: State Machine Guard Logic
```cpp
// State transition with explicit guard conditions
if (currentState == STATE_CAL &&
    ss_eoc_received && mdps_eoc_received) {
    currentState = STATE_MAZE;
    resetEpisodeVariables();
    transmitInitialCommand();
}
```
**Evaluation:** "Boolean guard prevents spurious transitions. Both subsystems must signal completion before maze entry."

### Example 2: NAVCON Angle Decision
```cpp
// Angle-dependent navigation rule from specification
if (theta_i <= 5.0) {
    command = DEC_FORWARD;  // Cross directly
} else if (theta_i <= 45.0) {
    command = DEC_ALIGN;    // Gradual alignment
} else {
    command = DEC_ROTATE;   // Sharp turn required
}
```
**Evaluation:** "Threshold-based logic matches specification requirements. Hysteresis at boundaries prevents oscillation (see QTP-SNC-04 partial result)."

---

## Integration into LaTeX

**For architecture diagram:**
```latex
\paragraph{Firmware Architecture}

The Main ESP32 firmware implements a modular architecture with clear separation of concerns. Figure~\ref{fig:firmware-architecture} illustrates the hierarchical organization with StateMachine coordinating high-level behavior, NAVCON implementing navigation decision rules, SCS\_Protocol handling communication framing, PureTone managing dual-tone validation, and SPI\_Telemetry streaming diagnostics to the WiFi subsystem.

\begin{figure}[H]
\centering
\includegraphics[width=0.95\textwidth]{01_SNC/diagrams/software_architecture/snc_firmware_architecture.png}
\caption{Main ESP32 firmware architecture showing modular design with separated header/implementation files and hierarchical control flow}
\label{fig:firmware-architecture}
\end{figure}
```

**For code snippets:**
Add to the existing "Approach to coding and code testing" paragraph in engineering_design.tex.

---

## Rubric Alignment

This addresses the **"sound approach to coding and code testing"** criterion worth **5/40 points** in Engineering Design:
- Architecture diagram shows systematic organization
- Code snippets demonstrate correctness evaluation
- Both options satisfy "appropriate code snippets provided" requirement
