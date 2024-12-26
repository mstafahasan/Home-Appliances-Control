import tkinter as tk
from tkinter import ttk
import serial
import threading
import time

# Initialize serial communication
SERIAL_PORT = 'COM8'  # Replace with your COM port
BAUD_RATE = 9600
try:
    serial_conn = serial.Serial(SERIAL_PORT, BAUD_RATE, timeout=1)
    print(f"Initialized serial communication on {SERIAL_PORT} at {BAUD_RATE} baud.")
except Exception as e:
    print(f"Error initializing serial communication: {e}")
    serial_conn = None

# Global variables
door_status = "Unknown"
temperature = "Unknown"
alarm_status = "Inactive"
update_interval = 0.25  # Interval in seconds to poll data
door_status_previous = "Unknown"  # To detect door state changes

def update_data():
    """
    Function to send commands to the microcontroller and update GUI values.
    Runs in a separate thread.
    """
    global door_status, temperature, alarm_status, door_status_previous

    while True:
        if serial_conn:
            try:
                # Request door status
                serial_conn.write(b'd')
                time.sleep(0.1)  # Delay to handle lag
                if serial_conn.in_waiting:
                    response = serial_conn.read(1).decode('utf-8')
                    if response == 'C':
                        door_status = "Closed"
                    elif response == 'O':
                        door_status = "Open"

                # Check for door state changes
                if door_status != door_status_previous:
                    log_door_status(door_status)
                    door_status_previous = door_status

                # Request temperature
                serial_conn.write(b'T')  # Assuming 'T' requests temperature
                time.sleep(0.1)
                if serial_conn.in_waiting:
                    response = serial_conn.readline().decode('utf-8').strip()
                    if response.startswith("T:"):
                        temperature = response[2:]

                # Update the GUI
                door_status_label.config(text=f"Door Status: {door_status}")
                temperature_label.config(text=f"Temperature: {temperature} °C")

            except Exception as e:
                print(f"Error: {e}")

        time.sleep(update_interval)

def log_door_status(status):
    """
    Log door status changes in the log panel.
    """
    timestamp = time.strftime("%Y-%m-%d %H:%M:%S")
    log_message = f"{timestamp} - Door {status}"
    log_text.insert(tk.END, log_message + "\n")
    log_text.see(tk.END)  # Auto-scroll to the latest log

def send_command(command):
    """
    Function to send a command to the microcontroller.
    """
    try:
        if serial_conn:
            serial_conn.write(command.encode('utf-8'))
            print(f"Sent command: {command.strip()}")
        else:
            print("Serial connection is not initialized.")
    except Exception as e:
        print(f"Error sending command: {e}")

# GUI Initialization
root = tk.Tk()
root.title("Microcontroller Monitor")

# Door Status
door_status_label = ttk.Label(root, text=f"Door Status: {door_status}", font=("Arial", 14))
door_status_label.grid(row=0, column=0, padx=10, pady=10)

# Temperature
temperature_label = ttk.Label(root, text=f"Temperature: {temperature} °C", font=("Arial", 14))
temperature_label.grid(row=1, column=0, padx=10, pady=10)

# Controls
controls_frame = ttk.LabelFrame(root, text="Controls")
controls_frame.grid(row=2, column=0, padx=10, pady=10)

# Buttons
stop_alarm_button = ttk.Button(controls_frame, text="Stop Alarm", command=lambda: send_command("STOP_ALARM\n"))
stop_alarm_button.grid(row=0, column=0, padx=5, pady=5)

red_led_button = ttk.Button(controls_frame, text="Turn LAMP ON", command=lambda: send_command("r"))
red_led_button.grid(row=0, column=1, padx=5, pady=5)

blue_led_button = ttk.Button(controls_frame, text="Turn PLUG On", command=lambda: send_command("b"))
blue_led_button.grid(row=0, column=2, padx=5, pady=5)

led_off_button = ttk.Button(controls_frame, text="Turn ALL Off", command=lambda: send_command("x"))
led_off_button.grid(row=0, column=3, padx=5, pady=5)

# Set Temperature Threshold
threshold_frame = ttk.LabelFrame(root, text="Temperature Threshold")
threshold_frame.grid(row=3, column=0, padx=10, pady=10)

threshold_entry = ttk.Entry(threshold_frame, width=10)
threshold_entry.grid(row=0, column=0, padx=5, pady=5)
threshold_entry.insert(0, "99")

set_threshold_button = ttk.Button(
    threshold_frame,
    text="Set Threshold",
    command=lambda: send_command(threshold_entry.get() + "\n"),
)
set_threshold_button.grid(row=0, column=1, padx=5, pady=5)

# Log Panel
log_frame = ttk.LabelFrame(root, text="Log Panel")
log_frame.grid(row=4, column=0, padx=10, pady=10)

log_text = tk.Text(log_frame, height=10, width=50, state=tk.NORMAL)
log_text.pack(side=tk.LEFT, fill=tk.BOTH, expand=True)

log_scrollbar = ttk.Scrollbar(log_frame, command=log_text.yview)
log_scrollbar.pack(side=tk.RIGHT, fill=tk.Y)
log_text.config(yscrollcommand=log_scrollbar.set)

# Start background thread for updating data
if serial_conn:
    thread = threading.Thread(target=update_data, daemon=True)
    thread.start()

# Run the GUI
try:
    root.mainloop()
except Exception as e:
    print(f"Error running GUI: {e}")

# Close serial connection when done
if serial_conn:
    serial_conn.close()
    print("Serial connection closed.")