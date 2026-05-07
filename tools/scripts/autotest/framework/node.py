import serial
import time
import threading
import re
import logging
import os
import sys

class MeshXNode:
    """
    Low-level interface for a MeshX hardware node via Serial.
    Handles thread-safe log collection and pattern matching.
    """
    def __init__(self, port, baud=115200, name=None, log_file=None):
        self.port = port
        self.baud = baud
        self.name = name or port
        self.log_file = log_file
        self.f_log = None
        self.serial = None
        self.running = False
        self.logs = []
        self.thread = None
        self.lock = threading.Lock()
        self.prompt_received = threading.Event()
        self.logger = logging.getLogger(f"Node-{self.name}")

    def connect(self):
        try:
            # Open log file in append mode if it's not open
            if self.log_file and not self.f_log:
                self.f_log = open(self.log_file, "a", encoding='utf-8', errors='ignore')

            self.serial = serial.Serial(self.port, self.baud, timeout=1)
            self.running = True
            self.thread = threading.Thread(target=self._read_loop, daemon=True)
            self.thread.start()
            self.logger.info(f"Connected to {self.port}")
            return True
        except Exception as e:
            self.logger.error(f"Failed to connect to {self.port}: {e}")
            return False

    def disconnect(self):
        self.running = False
        if self.serial:
            try:
                self.serial.close()
            except:
                pass
            self.serial = None
            
        if self.thread:
            self.thread.join(timeout=2)
            self.thread = None
            
        if self.f_log:
            try:
                self.f_log.flush()
                self.f_log.close()
            except:
                pass
            self.f_log = None
        self.logger.info(f"Disconnected from {self.port}")

    def hard_reset(self, bsp):
        """Triggers a hardware reset via meshx.py."""
        import subprocess
        script_path = os.path.abspath(os.path.join(os.path.dirname(__file__), '..', '..', 'meshx.py'))
        cmd = [sys.executable, script_path, "-HR", "-P", self.port, "-B", bsp, "-N", "all_in_one"]
        try:
            # We must close serial before resetting as esptool needs the port
            is_connected = self.serial is not None
            if is_connected:
                self.disconnect()
            
            subprocess.run(cmd, check=True, capture_output=True)
            self.logger.info(f"Hardware reset successful on {self.port}")
            
            if is_connected:
                time.sleep(2) # Wait for boot
                return self.connect()
            return True
        except Exception as e:
            self.logger.error(f"Hardware reset failed: {e}")
            return False

    def _read_loop(self):
        buffer = ""
        while self.running and self.serial:
            try:
                if self.serial.in_waiting > 0:
                    data = self.serial.read(self.serial.in_waiting).decode('utf-8', errors='ignore')
                    buffer += data
                    
                    if "MeshX> " in buffer:
                        self.prompt_received.set()
                    
                    if "\n" in buffer:
                        lines = buffer.split("\n")
                        for line in lines[:-1]:
                            clean_line = line.strip()
                            if clean_line:
                                with self.lock:
                                    self.logs.append(clean_line)
                                if self.f_log:
                                    self.f_log.write(clean_line + "\n")
                                    self.f_log.flush()
                        buffer = lines[-1]
                else:
                    time.sleep(0.01)
            except Exception as e:
                if self.running:
                    self.logger.error(f"Read error: {e}")
                break

    def send_command(self, cmd, wait_for_prompt=True, timeout=5):
        if not self.serial:
            return False
        
        self.prompt_received.clear()
        full_cmd = cmd + "\n"
        try:
            self.serial.write(full_cmd.encode())
        except Exception as e:
            self.logger.error(f"Write error: {e}")
            return False
        
        if wait_for_prompt:
            return self.prompt_received.wait(timeout=timeout)
        return True

    def expect(self, pattern, timeout=10):
        start_time = time.time()
        regex = re.compile(pattern)
        
        while time.time() - start_time < timeout:
            with self.lock:
                for log in reversed(self.logs):
                    if regex.search(log):
                        return True
            time.sleep(0.1)
        return False

    def clear_logs(self):
        with self.lock:
            self.logs = []

    def get_last_error(self):
        regex = re.compile(r"Command returned error: (-?\d+)")
        with self.lock:
            for log in reversed(self.logs):
                match = regex.search(log)
                if match:
                    return int(match.group(1))
        return 0
