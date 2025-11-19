
#!/usr/bin/env python3
"""
NAVCON Clear-Floor Tester (Single-Port, 4-byte SCS)
===================================================
- Connects to the SNC USB serial.
- Sends ONLY 4-byte SCS frames: [control, dat1, dat0, dec].
- Test sequence (SNC already in MAZE):
    1) MDPS: IST1 (battery), IST2 (rotation), IST3 (speed), IST4 (distance=1000mm)
    2) SS  : IST1 (colors=WWW), IST2 (angle=0°)
- Waits for SNC log line "TX NAVCON:" and parses Control/DAT1/DAT0/DEC.
- PASS if DEC==0 (FORWARD) and speeds are non-zero (typical 50/50).

NOTE: This assumes your firmware routes incoming USB SCS frames
to the emulated subsystem based on the SUB field in the control byte.
"""

import tkinter as tk
from tkinter import ttk, scrolledtext, messagebox
import serial, serial.tools.list_ports
import time, re
from enum import IntEnum

# --------------------- Protocol helpers ---------------------

class SystemState(IntEnum):
    SYS_IDLE = 0
    SYS_CAL  = 1
    SYS_MAZE = 2
    SYS_SOS  = 3

class SubsystemID(IntEnum):
    SUB_HUB = 0
    SUB_SNC = 1
    SUB_MDPS= 2
    SUB_SS  = 3

def ctrl(sys: SystemState, sub: SubsystemID, ist: int) -> int:
    return ((int(sys)&0x03)<<6) | ((int(sub)&0x03)<<4) | (ist & 0x0F)

# Expected control bytes in MAZE for MDPS and SS
CTRL_A1 = ctrl(SystemState.SYS_MAZE, SubsystemID.SUB_MDPS, 1)  # 0xA1
CTRL_A2 = ctrl(SystemState.SYS_MAZE, SubsystemID.SUB_MDPS, 2)  # 0xA2
CTRL_A3 = ctrl(SystemState.SYS_MAZE, SubsystemID.SUB_MDPS, 3)  # 0xA3
CTRL_A4 = ctrl(SystemState.SYS_MAZE, SubsystemID.SUB_MDPS, 4)  # 0xA4
CTRL_B1 = ctrl(SystemState.SYS_MAZE, SubsystemID.SUB_SS,   1)  # 0xB1
CTRL_B2 = ctrl(SystemState.SYS_MAZE, SubsystemID.SUB_SS,   2)  # 0xB2

# Regex to parse the firmware's print line:
# Example: "TX NAVCON: [MAZE:SNC:IST3] Control:0x93 DAT1:50 DAT0:50 DEC:0"
RE_TX_NAVCON = re.compile(
    r"TX NAVCON:.*Control:0x([0-9A-Fa-f]{2})\s+DAT1:(\d+)\s+DAT0:(\d+)\s+DEC:(\d+)"
)

# --------------------- GUI ---------------------

class ClearFloorTester:
    def __init__(self):
        self.root = tk.Tk()
        self.root.title("NAVCON Clear-Floor Tester — single USB, 4-byte SCS")
        self.root.geometry("960x640")

        self.ser = None

        self._build_ui()
        self._refresh_ports()

    def _build_ui(self):
        main = ttk.Frame(self.root, padding=10); main.pack(fill="both", expand=True)
        top  = ttk.LabelFrame(main, text="🔌 Connection", padding=10); top.pack(fill="x")
        ttk.Label(top, text="Port:").grid(row=0, column=0, sticky="w")
        self.port_var = tk.StringVar()
        self.port_cb  = ttk.Combobox(top, textvariable=self.port_var, state="readonly", width=28)
        self.port_cb.grid(row=0, column=1, padx=6)
        ttk.Label(top, text="Baud:").grid(row=0, column=2, sticky="e")
        self.baud_var = tk.StringVar(value="115200")
        ttk.Combobox(top, textvariable=self.baud_var, values=["9600","57600","115200","230400"], width=12).grid(row=0, column=3, padx=6)
        ttk.Button(top, text="🔄 Refresh", command=self._refresh_ports).grid(row=0, column=4, padx=8)
        self.btn_conn = ttk.Button(top, text="🔌 Connect", command=self._toggle_conn); self.btn_conn.grid(row=0, column=5)
        self.status = tk.StringVar(value="❌ Disconnected")
        ttk.Label(top, textvariable=self.status, foreground="red").grid(row=1, column=0, columnspan=6, sticky="w", pady=(6,0))

        left = ttk.LabelFrame(main, text="🧪 Tests", padding=10); left.pack(side="left", fill="y", padx=(0,10), pady=(10,0))
        ttk.Button(left, text="Run: Clear Floor → Forward", width=32, command=self.run_clear_floor).pack(fill="x", pady=4)
        ttk.Separator(left).pack(fill="x", pady=6)
        ttk.Button(left, text="🗑 Clear Log", command=self._clear).pack(fill="x", pady=2)
        ttk.Button(left, text="🔄 Reset (send 'R')", command=lambda:self._send_text('R')).pack(fill="x", pady=2)

        right = ttk.LabelFrame(main, text="📜 Log", padding=10); right.pack(side="right", fill="both", expand=True, pady=(10,0))
        self.log = scrolledtext.ScrolledText(right, wrap="word", font=("Consolas", 10), state="disabled")
        self.log.pack(fill="both", expand=True)

        for tag,color in [("info","blue"),("ok","green"),("warn","orange"),("err","red"),("tx","purple"),("rx","darkgreen"),("hdr","navy")]:
            self.log.tag_configure(tag, foreground=color)

    def _refresh_ports(self):
        ports = [p.device for p in serial.tools.list_ports.comports()]
        self.port_cb['values'] = ports
        if ports and not self.port_var.get():
            self.port_var.set(ports[0])

    def _toggle_conn(self):
        if self.ser:
            try: self.ser.close()
            except Exception: pass
            self.ser=None
            self.status.set("❌ Disconnected"); self.btn_conn.configure(text="🔌 Connect")
            self._log("Disconnected", "warn")
        else:
            try:
                self.ser = serial.Serial(self.port_var.get(), int(self.baud_var.get()), timeout=0.2)
                time.sleep(1.8)
                self.status.set(f"✅ Connected {self.port_var.get()} @ {self.baud_var.get()}"); self.btn_conn.configure(text="🔌 Disconnect")
                self._log("Connected", "info")
            except Exception as e:
                self._log(f"Connect error: {e}", "err"); self.ser=None

    def _log(self, msg, tag="info"):
        self.log.configure(state="normal"); ts = time.strftime("%H:%M:%S")
        self.log.insert("end", f"[{ts}] {msg}\n", tag); self.log.configure(state="disabled"); self.log.see("end")

    def _clear(self):
        self.log.configure(state="normal"); self.log.delete("1.0","end"); self.log.configure(state="disabled")

    def _send_text(self, s: str):
        if not self.ser: self._log("Not connected", "err"); return False
        try:
            self.ser.write(s.encode("ascii")); self.ser.flush()
            self._log(f"TX text: {s!r}", "tx"); return True
        except Exception as e:
            self._log(f"Send text error: {e}", "err"); return False

    # --------------------- SCS I/O ---------------------
    def send_scs(self, control:int, dat1:int, dat0:int, dec:int):
        """Send exactly 4 bytes: control, dat1, dat0, dec."""
        if not self.ser: self._log("Not connected", "err"); return False
        try:
            frame = bytes([control & 0xFF, dat1 & 0xFF, dat0 & 0xFF, dec & 0xFF])
            self.ser.write(frame); self.ser.flush()
            # decode SUB/SYS/IST for visibility
            sys = (control>>6)&0x03; sub=(control>>4)&0x03; ist=control&0x0F
            self._log(f"TX SCS: ctrl=0x{control:02X} (SYS={sys} SUB={sub} IST={ist})  d1={dat1} d0={dat0} dec={dec}", "tx")
            time.sleep(0.05)
            return True
        except Exception as e:
            self._log(f"Send SCS error: {e}", "err"); return False

    def read_navcon_decision(self, timeout_s=3.0):
        """Wait for a 'TX NAVCON:' log line and parse control/dat1/dat0/dec."""
        if not self.ser: return None
        end = time.time() + timeout_s
        buf = b""
        while time.time() < end:
            try:
                data = self.ser.read(256)
                if data:
                    buf += data
                    # split by lines
                    while b"\n" in buf:
                        line, buf = buf.split(b"\n", 1)
                        try:
                            s = line.decode("utf-8", errors="ignore")
                        except Exception:
                            s = repr(line)
                        s_stripped = s.strip()
                        if s_stripped:
                            self._log(s_stripped, "rx")
                        m = RE_TX_NAVCON.search(s_stripped)
                        if m:
                            c_hex, d1_s, d0_s, dec_s = m.groups()
                            control = int(c_hex, 16)
                            dat1    = int(d1_s)
                            dat0    = int(d0_s)
                            dec     = int(dec_s)
                            return (control, dat1, dat0, dec)
                else:
                    time.sleep(0.01)
            except Exception as e:
                self._log(f"Read error: {e}", "err")
                time.sleep(0.05)
        return None

    # --------------------- Test ---------------------
    def run_clear_floor(self):
        if not self.ser:
            messagebox.showwarning("Not connected", "Connect to the SNC USB serial first")
            return

        self._log("\n=== Test: Clear Floor → Forward ===", "hdr")
        # clean buffers
        try:
            self.ser.reset_input_buffer(); self.ser.reset_output_buffer()
        except Exception: pass
        time.sleep(0.1)

        # MDPS IST1..4
        if not self.send_scs(CTRL_A1, 0x50, 0x00, 0x00): return   # batt ~80%
        if not self.send_scs(CTRL_A2, 0x00, 0x00, 0x00): return   # rotation 0°
        if not self.send_scs(CTRL_A3, 0x00, 0x00, 0x00): return   # speed 0,0
        if not self.send_scs(CTRL_A4, 0x03, 0xE8, 0x00): return   # distance 1000mm

        # SS IST1..2
        if not self.send_scs(CTRL_B1, 0x00, 0x00, 0x00): return   # colors WWW
        if not self.send_scs(CTRL_B2, 0x00, 0x00, 0x00): return   # angle 0°

        # Wait for decision
        self._log("Waiting for SNC NAVCON decision (TX NAVCON)...", "info")
        decision = self.read_navcon_decision(timeout_s=4.0)
        if not decision:
            self._log("❌ FAIL: No NAVCON decision within timeout", "err")
            return
        control, d1, d0, dec = decision
        self._log(f"Decision parsed: ctrl=0x{control:02X} d1={d1} d0={d0} dec={dec}", "ok")

        # Assert: FORWARD expected (DEC=0) with non-zero symmetric speeds
        if dec == 0 and (d1 > 0 or d0 > 0):
            self._log("✅ PASS: FORWARD with non-zero speeds", "ok")
        else:
            self._log("❌ FAIL: Expected FORWARD(dec=0) non-zero speeds", "err")

    def run(self):
        try:
            self.root.mainloop()
        finally:
            try:
                if self.ser: self.ser.close()
            except Exception:
                pass

def main():
    app = ClearFloorTester()
    app.run()

if __name__ == "__main__":
    main()
