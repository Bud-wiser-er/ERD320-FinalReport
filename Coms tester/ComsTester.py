import tkinter as tk
from tkinter import ttk, messagebox
import serial
import serial.tools.list_ports

# Serial config
BAUD_RATE = 19200

# ----- Dropdown Options -----
SYS_OPTIONS = {
    "IDLE (0)": 0,
    "CAL (1)": 1,
    "MAZE (2)": 2,
    "SOS (3)": 3
}

SUB_OPTIONS = {
    "HUB (0)": 0,
    "SNC (1)": 1,
    "MDPS (2)": 2,
    "SENSOR (3)": 3
}

IST_OPTIONS = {
    "0": 0,
    "1": 1,
    "2": 2,
    "3": 3,
    "4": 4,
    "5": 5
}

DEC_OPTIONS = {
    "Forward (0)": 0,
    "Backward (1)": 1,
    "Left Rotation (2)": 2,
    "Right Rotation (3)": 3,
    "Custom (manual)": None
}

# ----- GUI Functions -----
def get_serial_ports():
    return [port.device for port in serial.tools.list_ports.comports()]


def update_control_byte(*args):
    try:
        sys_val = SYS_OPTIONS[sys_var.get()]
        sub_val = SUB_OPTIONS[sub_var.get()]
        ist_val = IST_OPTIONS[ist_var.get()]
        ctrl = (sys_val << 6) | (sub_val << 4) | ist_val
        control_val.set(f"{ctrl:02X}")
    except Exception:
        control_val.set("")


def send_packet():
    try:
        port = port_var.get()
        if not port:
            raise ValueError("No COM port selected.")

        # Get values
        ctrl_byte = int(control_val.get(), 16)
        dat1_byte = int(dat1_entry.get(), 16)
        dat0_byte = int(dat0_entry.get(), 16)

        # Handle DEC
        dec_choice = dec_var.get()
        if DEC_OPTIONS[dec_choice] is not None:
            dec_byte = DEC_OPTIONS[dec_choice]
        else:
            dec_byte = int(dec_custom_entry.get(), 16)

        for val in (ctrl_byte, dat1_byte, dat0_byte, dec_byte):
            if not (0 <= val <= 255):
                raise ValueError("Byte values must be between 00 and FF.")

        # Send over serial
        with serial.Serial(port, BAUD_RATE, timeout=1) as ser:
            ser.write(bytes([ctrl_byte, dat1_byte, dat0_byte, dec_byte]))
            log.insert(tk.END, f"Sent: {ctrl_byte:02X} {dat1_byte:02X} {dat0_byte:02X} {dec_byte:02X}\n")
            log.see(tk.END)

    except Exception as e:
        messagebox.showerror("Error", str(e))


# ----- GUI Layout -----
root = tk.Tk()
root.title("MARV Phase 0 Packet Sender")

main = ttk.Frame(root, padding=10)
main.pack(fill=tk.BOTH, expand=True)

# Port Selection
ttk.Label(main, text="Serial Port:").grid(row=0, column=0, sticky="e")
port_var = tk.StringVar()
port_dropdown = ttk.Combobox(main, textvariable=port_var, values=get_serial_ports(), width=20)
port_dropdown.grid(row=0, column=1, padx=5, pady=2)
if get_serial_ports():
    port_dropdown.current(0)

# CONTROL Byte Generator
ttk.Label(main, text="SYS:").grid(row=1, column=0, sticky="e")
sys_var = tk.StringVar(value="MAZE (2)")
sys_dropdown = ttk.OptionMenu(main, sys_var, "MAZE (2)", *SYS_OPTIONS.keys(), command=update_control_byte)
sys_dropdown.grid(row=1, column=1, sticky="w")

ttk.Label(main, text="SUB:").grid(row=2, column=0, sticky="e")
sub_var = tk.StringVar(value="MDPS (2)")
sub_dropdown = ttk.OptionMenu(main, sub_var, "MDPS (2)", *SUB_OPTIONS.keys(), command=update_control_byte)
sub_dropdown.grid(row=2, column=1, sticky="w")

ttk.Label(main, text="IST:").grid(row=3, column=0, sticky="e")
ist_var = tk.StringVar(value="4")
ist_dropdown = ttk.OptionMenu(main, ist_var, "4", *IST_OPTIONS.keys(), command=update_control_byte)
ist_dropdown.grid(row=3, column=1, sticky="w")

ttk.Label(main, text="CONTROL Byte (hex):").grid(row=4, column=0, sticky="e")
control_val = tk.StringVar(value="")
ctrl_entry = ttk.Entry(main, textvariable=control_val, width=6)
ctrl_entry.grid(row=4, column=1, sticky="w")
update_control_byte()

# Data Bytes
ttk.Label(main, text="DAT1 (hex):").grid(row=5, column=0, sticky="e")
dat1_entry = ttk.Entry(main, width=6)
dat1_entry.insert(0, "01")
dat1_entry.grid(row=5, column=1, sticky="w")

ttk.Label(main, text="DAT0 (hex):").grid(row=6, column=0, sticky="e")
dat0_entry = ttk.Entry(main, width=6)
dat0_entry.insert(0, "F4")
dat0_entry.grid(row=6, column=1, sticky="w")

# DEC Byte
ttk.Label(main, text="DEC:").grid(row=7, column=0, sticky="e")
dec_var = tk.StringVar(value="Forward (0)")
dec_dropdown = ttk.OptionMenu(main, dec_var, "Forward (0)", *DEC_OPTIONS.keys())
dec_dropdown.grid(row=7, column=1, sticky="w")

ttk.Label(main, text="DEC (custom hex):").grid(row=8, column=0, sticky="e")
dec_custom_entry = ttk.Entry(main, width=6)
dec_custom_entry.insert(0, "00")
dec_custom_entry.grid(row=8, column=1, sticky="w")

# Send Button
send_button = ttk.Button(main, text="Send Packet", command=send_packet)
send_button.grid(row=9, column=0, columnspan=2, pady=10)

# Log
log = tk.Text(root, height=8, width=50)
log.pack(padx=10, pady=5)
log.insert(tk.END, "Packet send log:\n")

root.mainloop()
