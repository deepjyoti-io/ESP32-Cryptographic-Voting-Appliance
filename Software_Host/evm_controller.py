import tkinter as tk
from tkinter import messagebox, simpledialog
import sqlite3
import serial
import threading
import datetime

# --- Configuration ---
SERIAL_PORT = 'COM4'  # Ensure this matches your setup
BAUD_RATE = 115200

class EVMControlUnit:
    def __init__(self, root):
        self.root = root
        self.root.title("EVM Control Unit - Officer Dashboard")
        self.root.geometry("450x480") # Made slightly taller to fit new button
        self.root.configure(padx=20, pady=20)

        self.conn = sqlite3.connect('election.db', check_same_thread=False)
        
        try:
            self.ser = serial.Serial(SERIAL_PORT, BAUD_RATE, timeout=1)
            self.serial_status = "Connected"
        except Exception as e:
            self.ser = None
            self.serial_status = f"Disconnected"

        self.build_gui()
        
        if self.ser:
            self.listen_thread = threading.Thread(target=self.listen_to_esp32, daemon=True)
            self.listen_thread.start()

    def build_gui(self):
        color = "green" if self.ser else "red"
        self.status_lbl = tk.Label(self.root, text=f"Status: {self.serial_status}", fg=color, font=("Arial", 10, "bold"))
        self.status_lbl.pack(pady=5)

        tk.Label(self.root, text="Enter Voter ID:", font=("Arial", 12)).pack(pady=10)
        self.voter_entry = tk.Entry(self.root, font=("Arial", 14), justify="center")
        self.voter_entry.pack(pady=5)

        self.auth_btn = tk.Button(self.root, text="Authorize Voter", font=("Arial", 12, "bold"), bg="#4CAF50", fg="white", command=self.authorize_voter)
        self.auth_btn.pack(pady=10, fill="x", ipady=10)

        self.log_text = tk.Text(self.root, height=8, width=40, state="disabled")
        self.log_text.pack(pady=10)
        self.log("System Initialized.")

        # --- Admin & Reset Buttons ---
        tk.Frame(self.root, height=2, bd=1, relief="sunken").pack(fill="x", pady=5) 
        
        self.admin_btn = tk.Button(self.root, text="🔒 Admin: View Results", font=("Arial", 10), bg="#333333", fg="white", command=self.request_results)
        self.admin_btn.pack(fill="x", pady=2)

        # --- NEW: Reset Button ---
        self.reset_btn = tk.Button(self.root, text="⚠️ FORMAT & RESET ELECTION", font=("Arial", 10, "bold"), bg="#D32F2F", fg="white", command=self.reset_election)
        self.reset_btn.pack(fill="x", pady=2)

    def log(self, message):
        self.log_text.config(state="normal")
        self.log_text.insert(tk.END, f"{datetime.datetime.now().strftime('%H:%M:%S')} - {message}\n")
        self.log_text.see(tk.END)
        self.log_text.config(state="disabled")

    def authorize_voter(self):
        voter_id = self.voter_entry.get().strip()
        if not voter_id: return

        cursor = self.conn.cursor()
        cursor.execute("SELECT name, voted FROM voters WHERE voter_id=?", (voter_id,))
        result = cursor.fetchone()

        if result is None:
            messagebox.showerror("Error", f"Voter ID '{voter_id}' not found.")
            return

        name, voted = result
        if voted == 1:
            messagebox.showerror("Fraud Alert", f"Voter '{name}' has ALREADY VOTED!")
            self.log(f"REJECTED: {voter_id} - Already Voted")
            return

        if self.ser:
            auth_packet = f"AUTH|{voter_id}|1234\n" 
            self.ser.write(auth_packet.encode('utf-8'))
            self.log(f"Authorized {name} ({voter_id}). Waiting for vote...")
            
            # Disable administrative buttons during a live vote
            self.auth_btn.config(state="disabled")
            self.admin_btn.config(state="disabled") 
            self.reset_btn.config(state="disabled")
        else:
            messagebox.showerror("Hardware Error", "ESP32 not connected.")

    def request_results(self):
        password = simpledialog.askstring("Admin Login", "Enter Master Password:", show='*')
        if password == "admin123":
            if self.ser:
                self.ser.write(b"ADMIN|admin123\n")
                self.log("Admin results requested from hardware.")
        elif password is not None:
            messagebox.showerror("Access Denied", "Incorrect Password.")

    # --- NEW: Reset Election Function ---
    def reset_election(self):
        password = simpledialog.askstring("WARNING", "Enter Master Password to WIPE all data:", show='*')
        if password == "admin123":
            
            # 1. Wipe the Laptop Database (Reset all voted flags to 0)
            cursor = self.conn.cursor()
            cursor.execute("UPDATE voters SET voted=0, vote_time=NULL")
            self.conn.commit()
            
            # 2. Command ESP32 to Wipe NVS and SD Card
            if self.ser:
                self.ser.write(b"RESET|admin123\n")
                
            self.log("SYSTEM WIPED. Ready for new election.")
            messagebox.showinfo("Reset Successful", "Database and ESP32 memory have been completely formatted to zero.")
            
        elif password is not None:
            messagebox.showerror("Access Denied", "Incorrect Password.")

    def listen_to_esp32(self):
        while True:
            if self.ser and self.ser.in_waiting > 0:
                line = self.ser.readline().decode('utf-8').strip()
                
                if line.startswith("SUCCESS|"):
                    voter_id = line.split("|")[1]
                    self.mark_as_voted(voter_id)
                
                elif line.startswith("RESULTS|"):
                    parts = line.split("|")
                    if len(parts) == 5:
                        c1, c2, c3, esp_total = parts[1], parts[2], parts[3], parts[4]
                        self.root.after(0, lambda: self.display_final_tally(c1, c2, c3, esp_total))
            
    def mark_as_voted(self, voter_id):
        cursor = self.conn.cursor()
        cursor.execute("UPDATE voters SET voted=1, vote_time=datetime('now', 'localtime') WHERE voter_id=?", (voter_id,))
        self.conn.commit()
        
        self.root.after(0, lambda: self.log(f"Vote confirmed for ID: {voter_id}"))
        
        # Re-enable all buttons after successful vote
        self.root.after(0, lambda: self.auth_btn.config(state="normal"))
        self.root.after(0, lambda: self.admin_btn.config(state="normal"))
        self.root.after(0, lambda: self.reset_btn.config(state="normal"))
        self.root.after(0, lambda: self.voter_entry.delete(0, tk.END))

    def display_final_tally(self, c1, c2, c3, esp_total):
        cursor = self.conn.cursor()
        cursor.execute("SELECT COUNT(*) FROM voters WHERE voted=1")
        db_total = cursor.fetchone()[0]
        
        # The ultimate integrity check
        if int(esp_total) == db_total:
            match_status = "✅ VALID: Hardware & Database Match"
        else:
            match_status = "❌ WARNING: Discrepancy Detected!"

        result_text = f"""--- OFFICIAL ELECTION TALLY ---

Candidate 1: {c1} votes
Candidate 2: {c2} votes
Candidate 3: {c3} votes

-----------------------------------------
ESP32 Hardware Total : {esp_total}
Laptop Database Total: {db_total}

INTEGRITY CHECK:
{match_status}
"""
        messagebox.showinfo("Secure Results", result_text)

if __name__ == "__main__":
    root = tk.Tk()
    app = EVMControlUnit(root)
    root.mainloop()