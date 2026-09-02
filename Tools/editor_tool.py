import tkinter as tk
from tkinter import filedialog, messagebox
import socket

# UDP設定
UDP_IP = "127.0.0.1"
UDP_PORT = 50000

class EditorTool(tk.Tk):
    def __init__(self):
        super().__init__()
        self.title("Map Editor")
        self.geometry("400x500")
        
        self.sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        
        self.create_widgets()
        self.after(2000, self.check_engine_alive)
        
    def check_engine_alive(self):
        try:
            import subprocess
            output = subprocess.check_output('tasklist /FI "IMAGENAME eq GJ.exe" /NH', shell=True, text=True)
            if "GJ.exe" not in output:
                print("Engine closed. Exiting tool...")
                self.destroy()
                return
        except Exception as e:
            print(f"Process check failed: {e}")
        
        self.after(2000, self.check_engine_alive)
        
    def create_widgets(self):
        # ファイル操作フレーム
        file_frame = tk.LabelFrame(self, text="File Operations", padx=10, pady=10)
        file_frame.pack(fill="x", padx=10, pady=5)
        
        load_btn = tk.Button(file_frame, text="Load JSON", command=self.load_file)
        load_btn.pack(side="left", padx=5)
        
        save_btn = tk.Button(file_frame, text="Save JSON", command=self.save_file)
        save_btn.pack(side="left", padx=5)
        
        self.current_file_lbl = tk.Label(file_frame, text="No file selected")
        self.current_file_lbl.pack(side="bottom", anchor="w", pady=5)
        
        # パレット（ブロック種類）フレーム
        palette_frame = tk.LabelFrame(self, text="Palette (Block Type)", padx=10, pady=10)
        palette_frame.pack(fill="both", expand=True, padx=10, pady=5)
        
        self.palette_var = tk.IntVar()
        self.palette_var.set(1) # default
        
        # 仮のパレットリスト（※実際のマップチップIDに合わせる）
        palettes = [
            (0, "Air (Delete)"),
            (1, "Block (Normal)"),
            (2, "Moving Block (Gimmick)"),
            (3, "Spike"),
            (4, "Goal"),
            (99, "Player Spawn"),
        ]
        
        for val, name in palettes:
            rb = tk.Radiobutton(palette_frame, text=name, variable=self.palette_var, value=val, command=self.on_palette_change)
            rb.pack(anchor="w")
            
    def send_command(self, cmd):
        try:
            self.sock.sendto(cmd.encode('utf-8'), (UDP_IP, UDP_PORT))
            print(f"Sent: {cmd}")
        except Exception as e:
            print(f"Error sending command: {e}")

    def load_file(self):
        filepath = filedialog.askopenfilename(
            title="Select Level JSON",
            filetypes=(("JSON files", "*.json"), ("all files", "*.*"))
        )
        if filepath:
            self.current_file_lbl.config(text=filepath)
            # フルパスを送るかファイル名だけ送るかはエンジン側の仕様次第。
            self.send_command(f"LOAD:{filepath}")
            
    def save_file(self):
        filepath = filedialog.asksaveasfilename(
            title="Save Level JSON",
            defaultextension=".json",
            filetypes=(("JSON files", "*.json"), ("all files", "*.*"))
        )
        if filepath:
            self.current_file_lbl.config(text=filepath)
            self.send_command(f"SAVE:{filepath}")
            
    def on_palette_change(self):
        val = self.palette_var.get()
        self.send_command(f"PALETTE:{val}")

if __name__ == "__main__":
    app = EditorTool()
    app.mainloop()
