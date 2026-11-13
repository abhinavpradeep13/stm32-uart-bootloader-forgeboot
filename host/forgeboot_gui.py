
import tkinter as tk
from tkinter import ttk, messagebox, filedialog

import serial
import serial.tools.list_ports

import threading
import queue
import time
import pathlib

import forgeboot_backend as backend


# ============================================================
# FORGEBOOT GUI CONFIGURATION
# ============================================================

APP_TITLE = "ForgeBoot v0.2"

DEFAULT_BAUDRATE = 115200

WINDOW_WIDTH = 1100
WINDOW_HEIGHT = 800


# ============================================================
# MAIN GUI APPLICATION
# ============================================================

class ForgeBootGUI:

    def __init__(self, root):

        self.root = root

        self.root.title(APP_TITLE)
        self.root.geometry(f"{WINDOW_WIDTH}x{WINDOW_HEIGHT}")
        self.root.minsize(900, 650)

        # ----------------------------------------------------
        # SERIAL CONNECTION
        # ----------------------------------------------------

        self.ser = None
        self.serial_lock = threading.Lock()
        self.connected = False

        # ----------------------------------------------------
        # SERIAL MONITOR THREAD
        # ----------------------------------------------------

        self.monitor_thread = None

        self.monitor_running = threading.Event()
        self.monitor_enabled = threading.Event()

        # ----------------------------------------------------
        # FIRMWARE UPLOAD
        # ----------------------------------------------------

        self.firmware_path = None

        self.upload_thread = None
        self.upload_active = False

        # Signals cancellation to the backend.
        self.upload_cancel_event = threading.Event()

        # ----------------------------------------------------
        # GUI EVENT QUEUE
        # ----------------------------------------------------

        self.gui_queue = queue.Queue()

        # ----------------------------------------------------
        # GUI INITIALIZATION
        # ----------------------------------------------------

        self.setup_styles()
        self.create_widgets()

        self.refresh_ports()

        self.root.after(50, self.process_gui_queue)

        self.root.protocol(
            "WM_DELETE_WINDOW",
            self.on_close
        )


    # ========================================================
    # GUI STYLING
    # ========================================================

    def setup_styles(self):

        style = ttk.Style()

        style.theme_use("clam")

        style.configure(
            "Title.TLabel",
            font=("Segoe UI", 20, "bold")
        )

        style.configure(
            "Subtitle.TLabel",
            font=("Segoe UI", 10)
        )

        style.configure(
            "Section.TLabel",
            font=("Segoe UI", 12, "bold")
        )

        style.configure(
            "Status.TLabel",
            font=("Segoe UI", 10, "bold")
        )

        style.configure(
            "TButton",
            font=("Segoe UI", 10),
            padding=6
        )

        style.configure(
            "Horizontal.TProgressbar",
            thickness=18
        )


    # ========================================================
    # MAIN WINDOW LAYOUT
    # ========================================================

    def create_widgets(self):

        main_frame = ttk.Frame(
            self.root,
            padding=15
        )

        main_frame.pack(
            fill="both",
            expand=True
        )

        # ----------------------------------------------------
        # HEADER
        # ----------------------------------------------------

        header = ttk.Frame(main_frame)

        header.pack(
            fill="x",
            pady=(0, 15)
        )

        ttk.Label(
            header,
            text="FORGEBOOT",
            style="Title.TLabel"
        ).pack(side="left")

        ttk.Label(
            header,
            text="STM32 UART Firmware Management | v0.2",
            style="Subtitle.TLabel"
        ).pack(
            side="right",
            pady=8
        )

        # ----------------------------------------------------
        # SERIAL CONNECTION PANEL
        # ----------------------------------------------------

        connection_frame = ttk.LabelFrame(
            main_frame,
            text=" Serial Connection ",
            padding=12
        )

        connection_frame.pack(
            fill="x",
            pady=(0, 12)
        )

        ttk.Label(
            connection_frame,
            text="COM Port:"
        ).grid(
            row=0,
            column=0,
            padx=5,
            pady=5
        )

        self.port_combo = ttk.Combobox(
            connection_frame,
            width=15,
            state="readonly"
        )

        self.port_combo.grid(
            row=0,
            column=1,
            padx=5,
            pady=5
        )

        self.refresh_button = ttk.Button(
            connection_frame,
            text="Refresh",
            command=self.refresh_ports
        )

        self.refresh_button.grid(
            row=0,
            column=2,
            padx=5
        )

        ttk.Label(
            connection_frame,
            text="Baud Rate:"
        ).grid(
            row=0,
            column=3,
            padx=(20, 5)
        )

        self.baud_combo = ttk.Combobox(
            connection_frame,
            width=12,
            state="readonly",
            values=[
                "9600",
                "19200",
                "38400",
                "57600",
                "115200"
            ]
        )

        self.baud_combo.set(
            str(DEFAULT_BAUDRATE)
        )

        self.baud_combo.grid(
            row=0,
            column=4,
            padx=5
        )

        self.connect_button = ttk.Button(
            connection_frame,
            text="Connect",
            command=self.connect_serial
        )

        self.connect_button.grid(
            row=0,
            column=5,
            padx=10
        )

        self.connection_status = ttk.Label(
            connection_frame,
            text="DISCONNECTED",
            foreground="red",
            style="Status.TLabel"
        )

        self.connection_status.grid(
            row=0,
            column=6,
            padx=10
        )

        # ----------------------------------------------------
        # FIRMWARE MANAGEMENT PANEL
        # ----------------------------------------------------

        firmware_frame = ttk.LabelFrame(
            main_frame,
            text=" Firmware Management ",
            padding=12
        )

        firmware_frame.pack(
            fill="x",
            pady=(0, 12)
        )

        firmware_frame.columnconfigure(
            1,
            weight=1
        )

        ttk.Label(
            firmware_frame,
            text="Firmware File:"
        ).grid(
            row=0,
            column=0,
            padx=5,
            pady=5
        )

        self.firmware_entry = ttk.Entry(
            firmware_frame,
            state="readonly"
        )

        self.firmware_entry.grid(
            row=0,
            column=1,
            sticky="ew",
            padx=5,
            pady=5
        )

        self.browse_button = ttk.Button(
            firmware_frame,
            text="Browse",
            command=self.browse_firmware
        )

        self.browse_button.grid(
            row=0,
            column=2,
            padx=5
        )

        self.firmware_info = ttk.Label(
            firmware_frame,
            text="No firmware selected."
        )

        self.firmware_info.grid(
            row=1,
            column=1,
            sticky="w",
            padx=5,
            pady=5
        )

        ttk.Label(
            firmware_frame,
            text="Upload Progress:"
        ).grid(
            row=2,
            column=0,
            padx=5,
            pady=10
        )

        self.upload_progress = ttk.Progressbar(
            firmware_frame,
            orient="horizontal",
            mode="determinate",
            maximum=100
        )

        self.upload_progress.grid(
            row=2,
            column=1,
            sticky="ew",
            padx=5,
            pady=10
        )

        self.progress_label = ttk.Label(
            firmware_frame,
            text="0%"
        )

        self.progress_label.grid(
            row=2,
            column=2,
            padx=5
        )

        # Upload and abort controls.

        button_frame = ttk.Frame(firmware_frame)

        button_frame.grid(
            row=3,
            column=1,
            sticky="w",
            padx=5,
            pady=5
        )

        self.upload_button = ttk.Button(
            button_frame,
            text="Upload Firmware",
            command=self.start_upload,
            state="disabled"
        )

        self.upload_button.pack(
            side="left",
            padx=(0, 8)
        )

        self.abort_button = ttk.Button(
            button_frame,
            text="Abort",
            command=self.cancel_upload,
            state="disabled"
        )

        self.abort_button.pack(
            side="left"
        )

        # ----------------------------------------------------
        # UART MONITOR PANEL
        # ----------------------------------------------------

        monitor_frame = ttk.LabelFrame(
            main_frame,
            text=" Live UART Monitor ",
            padding=10
        )

        monitor_frame.pack(
            fill="both",
            expand=True,
            pady=(0, 12)
        )

        monitor_toolbar = ttk.Frame(monitor_frame)

        monitor_toolbar.pack(
            fill="x",
            pady=(0, 8)
        )

        ttk.Label(
            monitor_toolbar,
            text="USART1 | 8-N-1",
            style="Section.TLabel"
        ).pack(side="left")

        ttk.Button(
            monitor_toolbar,
            text="Clear",
            command=self.clear_monitor
        ).pack(side="right")

        self.monitor_text = tk.Text(
            monitor_frame,
            height=12,
            bg="#101820",
            fg="#00FF9C",
            insertbackground="white",
            font=("Consolas", 10),
            wrap="word",
            state="disabled"
        )

        monitor_scrollbar = ttk.Scrollbar(
            monitor_frame,
            orient="vertical",
            command=self.monitor_text.yview
        )

        self.monitor_text.configure(
            yscrollcommand=monitor_scrollbar.set
        )

        self.monitor_text.pack(
            side="left",
            fill="both",
            expand=True
        )

        monitor_scrollbar.pack(
            side="right",
            fill="y"
        )

        # ----------------------------------------------------
        # UPLOAD ACTIVITY LOG
        # ----------------------------------------------------

        log_frame = ttk.LabelFrame(
            main_frame,
            text=" Upload Activity ",
            padding=10
        )

        log_frame.pack(
            fill="both",
            expand=True
        )

        self.activity_text = tk.Text(
            log_frame,
            height=8,
            bg="#18232E",
            fg="#E0E0E0",
            insertbackground="white",
            font=("Consolas", 9),
            wrap="word",
            state="disabled"
        )

        activity_scrollbar = ttk.Scrollbar(
            log_frame,
            orient="vertical",
            command=self.activity_text.yview
        )

        self.activity_text.configure(
            yscrollcommand=activity_scrollbar.set
        )

        self.activity_text.pack(
            side="left",
            fill="both",
            expand=True
        )

        activity_scrollbar.pack(
            side="right",
            fill="y"
        )

        # ----------------------------------------------------
        # INITIAL STATUS
        # ----------------------------------------------------

        self.append_monitor(
            "[FORGEBOOT GUI] Application initialized.\n"
        )

        self.append_monitor(
            "[FORGEBOOT GUI] Select a COM port and connect.\n"
        )

        self.append_activity(
            "[SYSTEM] ForgeBoot v0.2 GUI ready.\n"
        )


    # ========================================================
    # SERIAL PORT DISCOVERY
    # ========================================================

    def refresh_ports(self):

        ports = serial.tools.list_ports.comports()

        port_names = [
            port.device for port in ports
        ]

        self.port_combo["values"] = port_names

        if port_names:

            if self.port_combo.get() not in port_names:
                self.port_combo.current(0)

            self.append_monitor(
                f"[SYSTEM] Found {len(port_names)} serial port(s).\n"
            )

        else:

            self.port_combo.set("")

            self.append_monitor(
                "[SYSTEM] No serial ports detected.\n"
            )


    # ========================================================
    # SERIAL CONNECTION
    # ========================================================

    def connect_serial(self):

        if self.connected:

            if self.upload_active:
                return

            self.disconnect_serial()
            return

        port = self.port_combo.get()

        if not port:

            messagebox.showwarning(
                "No COM Port",
                "Please select a serial port."
            )

            return

        try:

            baudrate = int(self.baud_combo.get())

            self.ser = serial.Serial(
                port=port,
                baudrate=baudrate,
                bytesize=serial.EIGHTBITS,
                parity=serial.PARITY_NONE,
                stopbits=serial.STOPBITS_ONE,
                timeout=0.1,
                write_timeout=5
            )

            time.sleep(0.2)

            self.ser.reset_input_buffer()

            self.connected = True

            self.connection_status.config(
                text="CONNECTED",
                foreground="green"
            )

            self.connect_button.config(
                text="Disconnect"
            )

            self.monitor_running.set()
            self.monitor_enabled.set()

            self.monitor_thread = threading.Thread(
                target=self.serial_monitor_worker,
                daemon=True
            )

            self.monitor_thread.start()

            self.append_monitor(
                f"[SYSTEM] Connected to {port} at {baudrate} baud.\n"
            )

            self.append_activity(
                f"[SYSTEM] Serial connection established: {port}\n"
            )

            self.update_upload_button_state()

        except (serial.SerialException, ValueError) as error:

            self.ser = None

            messagebox.showerror(
                "Connection Error",
                str(error)
            )

            self.append_monitor(
                f"[ERROR] Connection failed: {error}\n"
            )


    def disconnect_serial(self):

        if self.upload_active:
            return

        self.monitor_running.clear()
        self.monitor_enabled.set()

        if self.monitor_thread is not None:

            self.monitor_thread.join(timeout=1)

        if self.ser is not None:

            try:
                self.ser.close()

            except serial.SerialException:
                pass

        self.ser = None
        self.connected = False

        self.connection_status.config(
            text="DISCONNECTED",
            foreground="red"
        )

        self.connect_button.config(
            text="Connect"
        )

        self.append_monitor(
            "[SYSTEM] Serial connection closed.\n"
        )

        self.append_activity(
            "[SYSTEM] Serial connection closed.\n"
        )

        self.update_upload_button_state()


    # ========================================================
    # FIRMWARE FILE SELECTION
    # ========================================================

    def browse_firmware(self):

        if self.upload_active:
            return

        file_path = filedialog.askopenfilename(
            title="Select ForgeBoot Firmware",
            filetypes=[
                ("Binary Firmware", "*.bin"),
                ("All Files", "*.*")
            ]
        )

        if not file_path:
            return

        firmware = pathlib.Path(file_path)

        if firmware.suffix.lower() != ".bin":

            messagebox.showwarning(
                "Invalid Firmware",
                "Please select a .bin firmware file."
            )

            return

        if not firmware.is_file() or firmware.stat().st_size == 0:

            messagebox.showwarning(
                "Invalid Firmware",
                "The selected firmware file is empty or unavailable."
            )

            return

        self.firmware_path = str(firmware)

        file_size = firmware.stat().st_size

        self.firmware_entry.config(
            state="normal"
        )

        self.firmware_entry.delete(
            0,
            "end"
        )

        self.firmware_entry.insert(
            0,
            str(firmware)
        )

        self.firmware_entry.config(
            state="readonly"
        )

        self.firmware_info.config(
            text=f"{firmware.name} | {file_size:,} bytes"
        )

        self.upload_progress["value"] = 0
        self.progress_label.config(text="0%")

        self.append_monitor(
            f"[SYSTEM] Firmware selected: {firmware.name}\n"
        )

        self.append_activity(
            f"[SYSTEM] Selected firmware: {firmware.name}\n"
        )

        self.update_upload_button_state()


    # ========================================================
    # UPLOAD BUTTON STATE
    # ========================================================

    def update_upload_button_state(self):

        if (
            self.connected
            and self.firmware_path
            and not self.upload_active
        ):

            self.upload_button.config(
                state="normal"
            )

        else:

            self.upload_button.config(
                state="disabled"
            )


    # ========================================================
    # START FIRMWARE UPLOAD
    # ========================================================

    def start_upload(self):

        if not self.connected or self.ser is None:

            messagebox.showwarning(
                "Not Connected",
                "Connect to the STM32 before uploading."
            )

            return

        if not self.firmware_path:

            messagebox.showwarning(
                "No Firmware",
                "Select a firmware BIN file first."
            )

            return

        if self.upload_active:
            return

        if not pathlib.Path(self.firmware_path).is_file():

            messagebox.showerror(
                "Firmware Error",
                "The selected firmware file is no longer available."
            )

            return

        confirm = messagebox.askyesno(
            "Confirm Firmware Upload",
            "Make sure the STM32 is in ForgeBoot recovery mode.\n\n"
            "Starting an update may erase the existing application.\n\n"
            "Do you want to continue?"
        )

        if not confirm:
            return

        self.upload_active = True

        # Reset cancellation state for this upload.
        self.upload_cancel_event.clear()

        self.upload_progress["value"] = 0
        self.progress_label.config(text="0%")

        self.upload_button.config(
            state="disabled"
        )

        self.browse_button.config(
            state="disabled"
        )

        self.connect_button.config(
            state="disabled"
        )

        self.abort_button.config(
            state="normal"
        )

        self.append_activity(
            "\n========== FIRMWARE UPLOAD ==========\n"
        )

        self.append_monitor(
            "[SYSTEM] Firmware upload started.\n"
        )

        # Pause UART monitor before the recovery check.
        # The worker will own the serial receive stream.

        self.monitor_enabled.clear()

        # Wait until any active monitor read finishes.

        with self.serial_lock:
            pass

        self.upload_thread = threading.Thread(
            target=self.upload_worker,
            args=(self.ser, self.firmware_path),
            daemon=True
        )

        self.upload_thread.start()


    # ========================================================
    # REQUEST UPLOAD CANCELLATION
    # ========================================================

    def cancel_upload(self):

        if not self.upload_active:
            return

        if self.upload_cancel_event.is_set():
            return

        self.upload_cancel_event.set()

        self.abort_button.config(
            state="disabled"
        )

        self.append_activity(
            "\n[GUI] Cancellation requested. Waiting for backend...\n"
        )

        self.append_monitor(
            "[SYSTEM] Firmware upload cancellation requested.\n"
        )


    # ========================================================
    # BACKGROUND FIRMWARE UPLOAD WORKER
    # ========================================================

    def upload_worker(self, ser, firmware_path):

        try:

            # ------------------------------------------------
            # STEP 1: RECOVERY MODE PRE-FLIGHT CHECK
            # ------------------------------------------------

            self.gui_queue.put(
                (
                    "activity",
                    "\n[CHECK] Verifying STM32 recovery mode...\n"
                )
            )

            recovery_detected = backend.check_recovery_mode(
                ser,
                log_callback=self.upload_log_callback
            )

            if not recovery_detected:

                # Do not send BEGIN or any firmware data.
                # The application Flash remains untouched.

                self.gui_queue.put(
                    ("recovery_check_failed", None)
                )

                return

            # ------------------------------------------------
            # STEP 2: FIRMWARE UPLOAD
            # ------------------------------------------------

            self.gui_queue.put(
                (
                    "activity",
                    "\n[CHECK] Recovery mode verified. Starting upload.\n"
                )
            )

            backend.upload_firmware(
                ser,
                firmware_path,
                progress_callback=self.upload_progress_callback,
                log_callback=self.upload_log_callback,
                cancel_event=self.upload_cancel_event,
                packet_delay=0.0
            )

            self.gui_queue.put(
                ("upload_complete", None)
            )

        except InterruptedError as error:

            self.gui_queue.put(
                ("upload_cancelled", str(error))
            )

        except Exception as error:

            self.gui_queue.put(
                ("upload_failed", str(error))
            )

        finally:

            # Resume monitor after the check or upload finishes.

            self.monitor_enabled.set()


    # ========================================================
    # UPLOAD CALLBACKS
    # ========================================================

    def upload_progress_callback(
        self,
        completed,
        total,
        percentage,
        command,
        sequence
    ):

        self.gui_queue.put(
            (
                "progress",
                {
                    "completed": completed,
                    "total": total,
                    "percentage": percentage,
                    "command": command,
                    "sequence": sequence
                }
            )
        )


    def upload_log_callback(self, message):

        self.gui_queue.put(
            ("activity", message + "\n")
        )


    # ========================================================
    # GUI QUEUE PROCESSING
    # ========================================================

    def process_gui_queue(self):

        try:

            while True:

                event, data = self.gui_queue.get_nowait()

                if event == "uart":

                    self.append_monitor(data)

                elif event == "error":

                    self.append_monitor(
                        f"\n[ERROR] {data}\n"
                    )

                elif event == "activity":

                    self.append_activity(data)

                elif event == "progress":

                    percentage = data["percentage"]

                    self.upload_progress["value"] = percentage

                    self.progress_label.config(
                        text=f"{percentage:.1f}%"
                    )

                elif event == "recovery_check_failed":

                    self.upload_active = False

                    self.append_activity(
                        "\n[BLOCKED] Recovery mode not detected. "
                        "Firmware upload was not started.\n"
                    )

                    self.append_monitor(
                        "[SYSTEM] Upload blocked: STM32 did not respond "
                        "to the recovery-mode PING.\n"
                    )

                    self.restore_controls()

                    messagebox.showerror(
                        "Recovery Mode Not Detected",
                        "The STM32 did not respond to the ForgeBoot PING.\n\n"
                        "No firmware was uploaded and no application "
                        "Flash was erased.\n\n"
                        "Enter recovery mode using the PB12 button, "
                        "then try again."
                    )

                elif event == "upload_complete":

                    self.upload_active = False

                    self.upload_progress["value"] = 100

                    self.progress_label.config(
                        text="100%"
                    )

                    self.append_activity(
                        "\n[SUCCESS] Firmware upload completed.\n"
                    )

                    self.append_monitor(
                        "[SYSTEM] Firmware upload completed successfully.\n"
                    )

                    self.restore_controls()

                    messagebox.showinfo(
                        "Upload Successful",
                        "Firmware uploaded successfully.\n"
                        "The STM32 should now launch the application."
                    )

                elif event == "upload_cancelled":

                    self.upload_active = False

                    self.append_activity(
                        "\n[CANCELLED] Firmware update interrupted.\n"
                    )

                    self.append_monitor(
                        "[SYSTEM] Upload cancelled. Check the ABORT acknowledgement in the activity log.\n"
                    )

                    self.restore_controls()

                    messagebox.showinfo(
                        "Upload Cancelled",
                        "Upload cancellation processed.\n\n"
                        "Check the activity log for ABORT acknowledgement."
                    )

                elif event == "upload_failed":

                    self.upload_active = False

                    self.append_activity(
                        f"\n[UPLOAD ERROR] {data}\n"
                    )

                    self.append_monitor(
                        f"[ERROR] Firmware upload failed: {data}\n"
                    )

                    self.restore_controls()

                    messagebox.showerror(
                        "Upload Failed",
                        str(data)
                    )

        except queue.Empty:
            pass

        self.root.after(
            50,
            self.process_gui_queue
        )


    # ========================================================
    # RESTORE GUI CONTROLS AFTER UPLOAD
    # ========================================================

    def restore_controls(self):

        self.browse_button.config(
            state="normal"
        )

        self.connect_button.config(
            state="normal",
            text="Disconnect"
        )

        self.abort_button.config(
            state="disabled"
        )

        self.update_upload_button_state()


    # ========================================================
    # BACKGROUND SERIAL MONITOR
    # ========================================================

    def serial_monitor_worker(self):

        while self.monitor_running.is_set():

            if not self.monitor_enabled.is_set():

                time.sleep(0.01)
                continue

            try:

                with self.serial_lock:

                    # Recheck after acquiring the lock.
                    # Prevents a new monitor read from starting
                    # after the uploader pauses monitoring.

                    if not self.monitor_enabled.is_set():
                        continue

                    if (
                        self.ser is None
                        or not self.ser.is_open
                    ):
                        break

                    data = self.ser.read(
                        self.ser.in_waiting or 1
                    )

                if data:

                    text = data.decode(
                        "utf-8",
                        errors="replace"
                    )

                    self.gui_queue.put(
                        ("uart", text)
                    )

            except serial.SerialException as error:

                self.gui_queue.put(
                    ("error", str(error))
                )

                break

            except Exception as error:

                self.gui_queue.put(
                    ("error", str(error))
                )

                break


    # ========================================================
    # UART MONITOR DISPLAY
    # ========================================================

    def append_monitor(self, message):

        self.monitor_text.config(
            state="normal"
        )

        self.monitor_text.insert(
            "end",
            message
        )

        self.monitor_text.see("end")

        self.monitor_text.config(
            state="disabled"
        )


    def clear_monitor(self):

        self.monitor_text.config(
            state="normal"
        )

        self.monitor_text.delete(
            "1.0",
            "end"
        )

        self.monitor_text.config(
            state="disabled"
        )


    # ========================================================
    # UPLOAD ACTIVITY DISPLAY
    # ========================================================

    def append_activity(self, message):

        self.activity_text.config(
            state="normal"
        )

        self.activity_text.insert(
            "end",
            message
        )

        self.activity_text.see("end")

        self.activity_text.config(
            state="disabled"
        )


    # ========================================================
    # APPLICATION SHUTDOWN
    # ========================================================

    def on_close(self):

        if self.upload_active:

            messagebox.showwarning(
                "Upload in Progress",
                "Wait for the firmware upload or cancellation to finish before closing."
            )

            return

        if self.connected:

            self.disconnect_serial()

        self.root.destroy()


# ============================================================
# APPLICATION ENTRY POINT
# ============================================================

if __name__ == "__main__":

    root = tk.Tk()

    app = ForgeBootGUI(root)

    root.mainloop()
