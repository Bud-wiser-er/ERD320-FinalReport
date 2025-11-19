# Pure Tone Detection Simulation Images

This folder should contain **2 LTspice simulation images** for the Engineering Design section:

## Image 1: Bandpass Filter Frequency Response (Bode Plot)
**Filename:** `bandpass_bode_plot.png` or `.pdf`

**What to show:**
- X-axis: Frequency (log scale, 100 Hz to 10 kHz)
- Y-axis: Magnitude (dB) and Phase (degrees)
- **Key features to highlight:**
  - Peak at 2800 Hz (mark with annotation)
  - -3 dB bandwidth points (show BW = ~1080 Hz)
  - Stopband rejection at 400 Hz (>70 dB)
  - Stopband rejection at 1 kHz (>65 dB)
  - Q-factor annotation
  - 4th-order rolloff slopes (80 dB/decade)

**Caption suggestion:** "LTspice AC analysis of cascaded 4th-order MFB bandpass filter showing center frequency f₀=2835 Hz, bandwidth 1080 Hz (Q≈2.6), and stopband rejection >70 dB at 400 Hz"

---

## Image 2: Envelope Detector Transient Response
**Filename:** `envelope_detector_transient.png` or `.pdf`

**What to show:**
- X-axis: Time (0 to 200 ms)
- Y-axis: Voltage (0 to 3.3V)
- **Three waveforms (stacked vertically):**
  1. **Top trace:** 2800 Hz tone input from microphone (small amplitude sine wave)
  2. **Middle trace:** Bandpass filter output (amplified 2800 Hz sine)
  3. **Bottom trace:** Envelope detector output (smooth DC rising curve)

**Key annotations:**
- Mark the 2.8V GPIO threshold crossing point
- Show rise time τ ≈ 68 ms
- Mark tone duration (e.g., 0.8s burst)
- Indicate ripple attenuation

**Caption suggestion:** "Transient simulation of pure tone detection signal chain showing 2800 Hz input, bandpass amplification, and envelope detection with 68 ms rise time (τ=33 ms) crossing 2.8V threshold for GPIO interrupt assertion"

---

## How to Export from LTspice:
1. Run the simulation
2. Right-click on plot → "File" → "Export as bitmap/metafile"
3. Choose PNG (recommended) or save as PDF
4. Use 300 DPI minimum for print quality
5. Save with descriptive filenames in this folder

## Integration into LaTeX:
Add to `engineering_design.tex` after the "Simulation and Analysis" paragraph:

```latex
\begin{figure}[H]
\centering
\includegraphics[width=0.9\textwidth]{01_SNC/diagrams/pure_tone_simulation/bandpass_bode_plot.png}
\caption{LTspice AC analysis of cascaded 4th-order MFB bandpass filter showing center frequency f₀=2835 Hz, bandwidth 1080 Hz (Q≈2.6), and stopband rejection >70 dB at 400 Hz}
\label{fig:bandpass-bode}
\end{figure}

\begin{figure}[H]
\centering
\includegraphics[width=0.9\textwidth]{01_SNC/diagrams/pure_tone_simulation/envelope_detector_transient.png}
\caption{Transient simulation of pure tone detection signal chain showing 2800 Hz input, bandpass amplification, and envelope detection with 68 ms rise time crossing 2.8V threshold}
\label{fig:envelope-transient}
\end{figure}
```
