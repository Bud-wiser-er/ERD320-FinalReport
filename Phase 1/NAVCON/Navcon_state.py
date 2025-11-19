
#!/usr/bin/env python3
"""
NAVCON Simple Tester — single USB, SNC starts in MAZE
=====================================================
What it does (no extra frills):
1) Connect to ONE serial port (USB to SNC).
2) For each scenario, send MDPS IST1..IST4, then SS IST1..IST2 (and optional IST3=EoM).
3) Wait for the NAVCON decision mirrored by firmware as: 0xFE + 4 bytes.
4) Decode and PASS/FAIL.

Minimal firmware hooks (already shown before):
- INJECT emulation over USB:
    '@' + 'M'/'S' + <control><dat1><dat0><dec>
- NAVCON decision mirror out over USB:
    0xFE + <control><dat1><dat0><dec> whenever SNC transmits its IST3 packet.
"""

import tkinter as tk
from tkinter import ttk, scrolledtext, messagebox
import serial, serial.tools.list_ports
import time, struct
from enum import IntEnum
from dataclasses import dataclass
from typing import Optional, List, Tuple

MIRROR_DECISION = 0xFE

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

class ColorType(IntEnum):
    WHITE = 0
    RED   = 1
    GREEN = 2
    BLUE  = 3
    BLACK = 4

class NAVCONAction(IntEnum):
    FORWARD  = 0
    BACKWARD = 1
    ROTATE_L = 2
    ROTATE_R = 3
    STOP     = 4
    STEER    = 10
    L180     = 11
    R180     = 12
    L360     = 13
    ERROR    = 99

@dataclass
class SCSPacket:
    control: int
    dat1: int
    dat0: int
    dec:  int
    def to_bytes(self) -> bytes:
        return bytes([self.control & 0xFF, self.dat1 & 0xFF, self.dat0 & 0xFF, self.dec & 0xFF])
    def __str__(self):
        return f"SCS[0x{self.control:02X} d1={self.dat1} d0={self.dat0} dec={self.dec}]"

def make_control(sys: SystemState, sub: SubsystemID, ist: int) -> int:
    return ((int(sys) & 0x03) << 6) | ((int(sub) & 0x03) << 4) | (ist & 0x0F)

def pkt_ss_colors(s1: ColorType, s2: ColorType, s3: ColorType) -> SCSPacket:
    data = ((int(s1)&7)<<6) | ((int(s2)&7)<<3) | (int(s3)&7)
    return SCSPacket(make_control(SystemState.SYS_MAZE, SubsystemID.SUB_SS, 1), (data>>8)&0xFF, data&0xFF, 0)

def pkt_ss_angle(angle_deg_u8: int) -> SCSPacket:
    return SCSPacket(make_control(SystemState.SYS_MAZE, SubsystemID.SUB_SS, 2), angle_deg_u8 & 0xFF, 0, 0)

def pkt_ss_eom() -> SCSPacket:
    return SCSPacket(make_control(SystemState.SYS_MAZE, SubsystemID.SUB_SS, 3), 0, 0, 0)

def pkt_mdps_ist1_batt(level: int=80) -> SCSPacket:
    return SCSPacket(make_control(SystemState.SYS_MAZE, SubsystemID.SUB_MDPS, 1), level & 0xFF, 0, 0)

def pkt_mdps_ist2_rotation(angle: int=0, direction: int=0) -> SCSPacket:
    return SCSPacket(make_control(SystemState.SYS_MAZE, SubsystemID.SUB_MDPS, 2), (angle>>8)&0xFF, angle&0xFF, direction & 0xFF)

def pkt_mdps_ist3_speed(vr: int=0, vl: int=0) -> SCSPacket:
    return SCSPacket(make_control(SystemState.SYS_MAZE, SubsystemID.SUB_MDPS, 3), vr & 0xFF, vl & 0xFF, 0)

def pkt_mdps_ist4_distance(mm: int) -> SCSPacket:
    return SCSPacket(make_control(SystemState.SYS_MAZE, SubsystemID.SUB_MDPS, 4), (mm>>8)&0xFF, mm&0xFF, 0)


class SimpleTester:
    def __init__(self):
        self.root = tk.Tk()
        self.root.title("NAVCON Simple Tester — single USB")
        self.root.geometry("980x680")
        self.ser: Optional[serial.Serial] = None

        self._build_ui()
        self._refresh_ports()
        self._build_scenarios()
        self._render_scenarios()

    def _build_ui(self):
        f = ttk.Frame(self.root, padding=10); f.pack(fill="both", expand=True)
        conn = ttk.LabelFrame(f, text="🔌 Connection", padding=10); conn.pack(fill="x")
        ttk.Label(conn, text="Port:").grid(row=0, column=0, sticky="w")
        self.port_var = tk.StringVar()
        self.port_combo = ttk.Combobox(conn, textvariable=self.port_var, state="readonly", width=24)
        self.port_combo.grid(row=0, column=1, padx=6)
        ttk.Label(conn, text="Baud:").grid(row=0, column=2, sticky="e")
        self.baud_var = tk.StringVar(value="115200")
        ttk.Combobox(conn, textvariable=self.baud_var, values=["9600","57600","115200","230400"], width=12).grid(row=0, column=3, padx=6)
        ttk.Button(conn, text="🔄 Refresh", command=self._refresh_ports).grid(row=0, column=4, padx=8)
        self.btn_conn = ttk.Button(conn, text="🔌 Connect", command=self._toggle_conn); self.btn_conn.grid(row=0, column=5)
        self.status = tk.StringVar(value="❌ Disconnected")
        ttk.Label(conn, textvariable=self.status, foreground="red").grid(row=1, column=0, columnspan=6, sticky="w", pady=(6,0))

        left = ttk.LabelFrame(f, text="🧪 Scenarios (MDPS→SS, then wait decision)", padding=10)
        left.pack(side="left", fill="y", padx=(0,10), pady=(10,0)); self.left_frame = left

        right = ttk.LabelFrame(f, text="📜 Log", padding=10)
        right.pack(side="right", fill="both", expand=True, pady=(10,0))
        self.log_text = scrolledtext.ScrolledText(right, wrap="word", font=("Consolas", 10), state="disabled")
        self.log_text.pack(fill="both", expand=True)

        actions = ttk.Frame(f); actions.pack(fill="x", pady=(8,0))
        ttk.Button(actions, text="🗑 Clear Log", command=self._clear).pack(side="left")
        ttk.Button(actions, text="🔄 Reset (R)", command=lambda:self._send_ascii('R')).pack(side="left", padx=8)

        for tag, color in [("info","blue"),("sent","purple"),("rx","darkgreen"),("pass","green"),("fail","red"),("hdr","navy"),("warn","orange")]:
            self.log_text.tag_configure(tag, foreground=color)

    def _refresh_ports(self):
        ports = [p.device for p in serial.tools.list_ports.comports()]
        self.port_combo['values'] = ports
        if ports and not self.port_var.get():
            self.port_var.set(ports[0])

    def _toggle_conn(self):
        if self.ser:
            try: self.ser.close()
            except Exception: pass
            self.ser=None
            self.status.set("❌ Disconnected"); self.btn_conn.configure(text="🔌 Connect")
        else:
            try:
                self.ser = serial.Serial(self.port_var.get(), int(self.baud_var.get()), timeout=0.3)
                time.sleep(1.8)
                self.status.set(f"✅ Connected {self.port_var.get()} @ {self.baud_var.get()}")
                self.btn_conn.configure(text="🔌 Disconnect")
                self._log("Connected", "info")
            except Exception as e:
                self._log(f"Connect error: {e}", "fail"); self.ser=None

    def _log(self, msg, tag="info"):
        self.log_text.configure(state="normal")
        ts = time.strftime("%H:%M:%S")
        self.log_text.insert("end", f"[{ts}] {msg}\n", tag)
        self.log_text.configure(state="disabled"); self.log_text.see("end")

    def _clear(self):
        self.log_text.configure(state="normal"); self.log_text.delete("1.0","end"); self.log_text.configure(state="disabled")

    # -------- serial helpers --------
    def _send_ascii(self, ch: str):
        if not self.ser: self._log("Not connected", "fail"); return False
        try:
            self.ser.write(ch.encode("ascii")); self.ser.flush()
            self._log(f"CTRL '{ch}'", "sent"); time.sleep(0.05); return True
        except Exception as e:
            self._log(f"Send error: {e}", "fail"); return False

    def inject(self, target: str, pkt: SCSPacket):
        if not self.ser: self._log("Not connected", "fail"); return False
        frame = b'@' + target.encode('ascii') + pkt.to_bytes()
        try:
            self.ser.write(frame); self.ser.flush()
            self._log(f"Inject {target} -> {pkt}", "sent")
            time.sleep(0.08); return True
        except Exception as e:
            self._log(f"Injection error: {e}", "fail"); return False

    def read_decision(self, timeout_s=3.0) -> Optional[SCSPacket]:
        if not self.ser: return None
        start = time.time()
        while time.time() - start < timeout_s:
            if self.ser.in_waiting:
                b = self.ser.read(1)
                if not b: continue
                if b[0] == MIRROR_DECISION:
                    data = self.ser.read(4)
                    if len(data) == 4:
                        c,d1,d0,dec = struct.unpack("BBBB", data)
                        pkt = SCSPacket(c,d1,d0,dec)
                        self._log(f"Decision <- {pkt}", "rx")
                        return pkt
                else:
                    # ignore other prints
                    time.sleep(0.003)
            else:
                time.sleep(0.01)
        return None

    # -------- scenarios --------
    def _build_scenarios(self):
        self.scenarios: List[dict] = [
            dict(
                name="Clear floor → Forward",
                desc="MDPS: 1..4 (1000), SS: W,W,W + angle 0 → Expect FORWARD.",
                colors=(ColorType.WHITE, ColorType.WHITE, ColorType.WHITE),
                angle=0,
                dists=[1000],
                eom=False,
                expect=NAVCONAction.FORWARD
            ),
            dict(
                name="Green mid → Forward",
                desc="SS: W,G,W + angle 5 → Expect FORWARD.",
                colors=(ColorType.WHITE, ColorType.GREEN, ColorType.WHITE),
                angle=5,
                dists=[1200],
                eom=False,
                expect=NAVCONAction.FORWARD
            ),
            dict(
                name="Red S1 steep ΔD → STOP",
                desc="MDPS ΔD: 1000→1100, SS: R,W,W angle 50 → Expect STOP.",
                colors=(ColorType.RED, ColorType.WHITE, ColorType.WHITE),
                angle=50,
                dists=[1000,1100],
                eom=False,
                expect=NAVCONAction.STOP
            ),
            dict(
                name="Black S1 (wall) → STOP",
                desc="SS: K,W,W angle 20 → Expect STOP (enter WALL_NAV).",
                colors=(ColorType.BLACK, ColorType.WHITE, ColorType.WHITE),
                angle=20,
                dists=[3000,3020],
                eom=False,
                expect=NAVCONAction.STOP
            ),
            dict(
                name="End-of-Maze → STOP",
                desc="SS: IST3 EoM → Expect STOP ready for 360°.",
                colors=None,
                angle=None,
                dists=[1500],
                eom=True,
                expect=NAVCONAction.STOP
            ),
        ]

    def _render_scenarios(self):
        for i, sc in enumerate(self.scenarios):
            ttk.Button(self.left_frame, text=sc["name"], width=34,
                       command=lambda idx=i: self.run_scenario(idx)).pack(fill="x", pady=3)

    def run_scenario(self, idx: int):
        if not self.ser:
            messagebox.showwarning("Not connected", "Connect to the SNC USB serial first")
            return
        sc = self.scenarios[idx]
        self._log(f"\n=== {sc['name']} ===\n{sc['desc']}", "hdr")

        try:
            self.ser.reset_input_buffer(); self.ser.reset_output_buffer()
        except Exception: pass
        time.sleep(0.15)

        # B) MDPS IST1..IST4 (we send what SNC expects to have “seen”)
        if not self.inject('M', pkt_mdps_ist1_batt(80)): return
        if not self.inject('M', pkt_mdps_ist2_rotation(0,0)): return
        if not self.inject('M', pkt_mdps_ist3_speed(0,0)): return
        for mm in sc["dists"]:
            if not self.inject('M', pkt_mdps_ist4_distance(mm)): return

        # C) SS IST1..IST2 (and optional IST3 EoM)
        if sc["colors"]:
            s1,s2,s3 = sc["colors"]
            if not self.inject('S', pkt_ss_colors(s1,s2,s3)): return
        if sc["angle"] is not None:
            if not self.inject('S', pkt_ss_angle(int(sc["angle"]))): return
        if sc["eom"]:
            if not self.inject('S', pkt_ss_eom()): return

        # Wait for decision
        pkt = self.read_decision(timeout_s=3.0)
        if not pkt:
            self._log("Decision timeout", "fail"); return

        act, desc = self._decode(pkt)
        self._log(f"NAVCON => {act.name}: {desc}", "rx")
        expect = sc["expect"]
        if act == expect or (expect==NAVCONAction.FORWARD and act==NAVCONAction.STEER):
            self._log(f"✅ PASS expected {expect.name}, got {act.name}", "pass")
        else:
            self._log(f"❌ FAIL expected {expect.name}, got {act.name}", "fail")

    # decode SNC decision packet
    def _decode(self, pkt: SCSPacket) -> Tuple[NAVCONAction,str]:
        dec = pkt.dec
        if dec == 0: return (NAVCONAction.FORWARD,  f"Forward R={pkt.dat1} L={pkt.dat0}")
        if dec == 1: return (NAVCONAction.BACKWARD, f"Backward R={pkt.dat1} L={pkt.dat0}")
        if dec == 2:
            ang = (pkt.dat1<<8)|pkt.dat0
            if ang==180: return (NAVCONAction.L180, "Left 180°")
            if ang==360: return (NAVCONAction.L360, "Left 360°")
            if ang<=10:  return (NAVCONAction.STEER, f"Left steer {ang}°")
            return (NAVCONAction.ROTATE_L, f"Left {ang}°")
        if dec == 3:
            ang = (pkt.dat1<<8)|pkt.dat0
            if ang==180: return (NAVCONAction.R180, "Right 180°")
            if ang<=10:  return (NAVCONAction.STEER, f"Right steer {ang}°")
            return (NAVCONAction.ROTATE_R, f"Right {ang}°")
        if dec == 4: return (NAVCONAction.STOP, "Stop")
        return (NAVCONAction.ERROR, f"Unknown DEC={dec}")

    def run(self):
        try: self.root.mainloop()
        finally:
            try:
                if self.ser: self.ser.close()
            except Exception: pass

def main():
    app = SimpleTester(); app.run()

if __name__ == "__main__":
    main()
