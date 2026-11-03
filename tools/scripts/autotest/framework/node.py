import serial
import time
import threading
import re
import logging
import os
import sys
import queue
import struct

# Try importing ELF decoder from tools/scripts
scripts_dir = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
if scripts_dir not in sys.path:
    sys.path.append(scripts_dir)

try:
    from host_decoder import MeshXLogDecoder
except ImportError:
    MeshXLogDecoder = None

try:
    from demux import StreamDemultiplexer
except ImportError:
    server_dir = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", "..", "..", "web_console", "server"))
    if server_dir not in sys.path:
        sys.path.append(server_dir)
    try:
        from demux import StreamDemultiplexer
    except ImportError:
        StreamDemultiplexer = None

class MeshXNode:
    """
    Low-level interface for a MeshX hardware node via Serial.
    Handles thread-safe log collection and pattern matching.
    Supports demultiplexing dynamic binary logs and MXCP packets.
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
        
        # MXCP and Demultiplexer structures
        self.mxcp_queue = queue.Queue()
        self.demux = StreamDemultiplexer() if StreamDemultiplexer else None
        
        # Load ELF and initialize decoder
        self.decoder = None
        if MeshXLogDecoder:
            elf_path = self._find_elf()
            if elf_path:
                try:
                    self.decoder = MeshXLogDecoder(elf_path)
                    self.logger.info(f"Loaded ELF decoder: {elf_path}")
                except Exception as e:
                    self.logger.warning(f"Failed to load ELF decoder: {e}")

    def _find_elf(self):
        env_path = os.environ.get("MESHX_ELF_PATH")
        if env_path and os.path.exists(env_path):
            return env_path
        
        build_dir = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", "..", "..", "..", "build"))
        if os.path.exists(build_dir):
            for root, _, files in os.walk(build_dir):
                for f in files:
                    if f.endswith(".elf") and "meshx_build" in f:
                        return os.path.join(root, f)
        return None

    def _serial_alive(self):
        return self.serial is not None and self.serial.is_open

    def _drop_serial(self):
        """Close and clear a dead serial handle (e.g. Windows invalidated it
        after a hardware EN reset). Log file is left untouched."""
        if self.serial:
            try:
                self.serial.close()
            except Exception:
                pass
            self.serial = None

    def _ensure_connected(self):
        """Open (or re-open) the serial port, retrying while the device
        re-enumerates after a reset. Returns True on success."""
        if self._serial_alive():
            return True
        self._drop_serial()
        while self.running:
            try:
                self.serial = serial.Serial(self.port, self.baud, timeout=1)
                self.logger.info(f"Connected to {self.port}")
                return True
            except Exception as e:
                self.logger.debug(f"Connect retry on {self.port}: {e}")
                time.sleep(0.5)
        return False

    def _serial_write(self, data: bytes) -> bool:
        """Write with one auto-reconnect on a dead handle (Windows OSError 9
        after a USB-CDC re-enumeration)."""
        try:
            with self.lock:
                if not self._serial_alive():
                    return False
                self.serial.write(data)
            return True
        except (OSError, serial.SerialException) as e:
            if self.running:
                self.logger.error(f"Serial write error on {self.port}: {e} (reconnecting)")
            self._drop_serial()
            if not self._ensure_connected():
                return False
            try:
                with self.lock:
                    self.serial.write(data)
                return True
            except Exception as e2:
                self.logger.error(f"Serial write failed after reconnect: {e2}")
                return False
        except Exception as e:
            self.logger.error(f"Serial write error: {e}")
            return False

    def connect(self):
        try:
            # Open log file in append mode if it's not open
            if self.log_file and not self.f_log:
                self.f_log = open(self.log_file, "a", encoding='utf-8', errors='ignore')

            if not self._ensure_connected():
                return False
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
            except Exception:
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
        text_buffer = ""

        def handle_text(data: bytes):
            nonlocal text_buffer
            text_str = data.decode("utf-8", errors="replace")
            text_buffer += text_str
            
            if "MeshX> " in text_buffer:
                self.prompt_received.set()
                
            if "\n" in text_buffer:
                lines = text_buffer.split("\n")
                for line in lines[:-1]:
                    clean_line = line.strip()
                    if clean_line:
                        with self.lock:
                            self.logs.append(clean_line)
                        if self.f_log:
                            self.f_log.write(clean_line + "\n")
                            self.f_log.flush()
                text_buffer = lines[-1]

        def handle_log(raw_pkt: bytes):
            msg = None
            if self.decoder:
                try:
                    res = self.decoder.decode_packet(raw_pkt)
                    if res:
                        # Strip ANSI escape sequences
                        clean_msg = re.sub(r'\x1b\[[0-9;]*m', '', res)
                        clean_msg = re.sub(r'\033\[[0-9;]*m', '', clean_msg)
                        msg = clean_msg
                except Exception as e:
                    self.logger.error(f"Failed to decode log packet: {e}")
            
            if msg is None:
                if len(raw_pkt) >= 13:
                    _, _, _, fmt_addr = struct.unpack("<BBII", raw_pkt[3:13])
                    msg = f"[BIN_LOG] at RAM: 0x{fmt_addr:08X}"
                else:
                    msg = f"[BIN_LOG] raw hex: {raw_pkt.hex()}"
            
            clean_line = msg.strip()
            if clean_line:
                with self.lock:
                    self.logs.append(clean_line)
                if self.f_log:
                    self.f_log.write(clean_line + "\n")
                    self.f_log.flush()

        def handle_mxsp(msg_type: int, payload: bytes):
            self.logger.debug(f"Received MXCP frame: type=0x{msg_type:02X}, len={len(payload)}")
            self.mxcp_queue.put((msg_type, payload))

        while self.running:
            try:
                with self.lock:
                    alive = self._serial_alive()
                if not alive:
                    if not self._ensure_connected():
                        time.sleep(0.5)
                        continue
                with self.lock:
                    waiting = self.serial.in_waiting
                if waiting > 0:
                    with self.lock:
                        chunk = self.serial.read(waiting)
                    if self.demux:
                        self.demux.feed(chunk, on_log=handle_log, on_mxsp=handle_mxsp, on_text=handle_text)
                    else:
                        handle_text(chunk)
                else:
                    time.sleep(0.01)
            except (OSError, serial.SerialException) as e:
                if self.running:
                    self.logger.error(f"Serial I/O error on {self.port}: {e} (reconnecting)")
                self._drop_serial()
                if not self._ensure_connected():
                    time.sleep(0.5)
            except Exception as e:
                if self.running:
                    self.logger.error(f"Read error: {e}")
                break

    def send_command(self, cmd, wait_for_prompt=True, timeout=5):
        if not self.serial:
            return False
        
        self.prompt_received.clear()
        full_cmd = cmd + "\n"
        if not self._serial_write(full_cmd.encode()):
            return False
        
        if wait_for_prompt:
            return self.prompt_received.wait(timeout=timeout)
        return True

    def send_mxcp_frame(self, type_id, payload=b""):
        if not self.serial:
            return False
        
        length = len(payload)
        frame = bytearray([0xFE, length, type_id])
        frame.extend(payload)

        # XOR Checksum
        checksum = length ^ type_id
        for b in payload:
            checksum ^= b

        frame.append(checksum)
        frame.append(0xEF)
        
        return self._serial_write(bytes(frame))

    def wait_for_mxcp_frame(self, expected_type, timeout=5):
        start_time = time.time()
        while time.time() - start_time < timeout:
            try:
                msg_type, payload = self.mxcp_queue.get(timeout=0.1)
                if msg_type == expected_type:
                    return payload
            except queue.Empty:
                continue
        return None

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
