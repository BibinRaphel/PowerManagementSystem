import sqlite3
import tkinter as tk
from tkinter import ttk, messagebox

def fetch_latest_data():
    try:
        conn = sqlite3.connect(".build/energy.db")
        cursor = conn.cursor()

        query = """
            SELECT timestamp, appliance_id, power_consumption, cumulative_energy, status
            FROM energy_data
            WHERE id IN (
                SELECT MAX(id) FROM energy_data GROUP BY appliance_id
            ) ORDER BY appliance_id;
        """
        cursor.execute(query)
        rows = cursor.fetchall()
        return rows

    except sqlite3.Error as e:
        messagebox.showerror("Database Error", f"Unable to query database:\n{e}")
        return []

    finally:
        if conn:
            conn.close()

def populate_table():
    for row in tree.get_children():
        tree.delete(row)

    data = fetch_latest_data()
    for entry in data:
        tree.insert("", "end", values=entry)

# Create main window
root = tk.Tk()
root.title("Latest Appliance Data")
root.geometry("750x400")

# Table setup
columns = ("Timestamp", "Appliance ID", "Power (W)", "Energy (kWh)", "Status")
tree = ttk.Treeview(root, columns=columns, show="headings")

for col in columns:
    tree.heading(col, text=col)
    tree.column(col, width=150, anchor="center")

tree.pack(pady=20, fill=tk.BOTH, expand=True)

# Refresh button
refresh_btn = ttk.Button(root, text="Refresh", command=populate_table)
refresh_btn.pack(pady=10)

# Populate initially
populate_table()

root.mainloop()
