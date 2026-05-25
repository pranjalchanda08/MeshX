import asyncio
import os
import sys
import yaml
import struct
import json
import logging
import serial
import serial.tools.list_ports
from typing import Dict, Set, Optional
from fastapi.responses import FileResponse
from fastapi.middleware.cors import CORSMiddleware
from fastapi import FastAPI, WebSocket, WebSocketDisconnect, Query, HTTPException

# Add server and scripts directories to path to import local modules
sys.path.append(os.path.dirname(os.path.abspath(__file__)))
from demux import StreamDemultiplexer

# Try importing ELF decoder from tools/scripts
sys.path.append(os.path.abspath(os.path.join(os.path.dirname(__file__), "..", "..", "scripts")))
try:
    from host_decoder import MeshXLogDecoder
except ImportError:
    MeshXLogDecoder = None

logging.basicConfig(level=logging.INFO)
logger = logging.getLogger("MeshXConsole")

app = FastAPI(title="MeshX Premium Web-Console Gateway")

# Enable CORS for frontend integration
app.add_middleware(
    CORSMiddleware,
    allow_origins=["*"],
    allow_credentials=True,
    allow_methods=["*"],
    allow_headers=["*"],
)

current_bsp = "xiao_c3"
current_product = "all_in_one"

def find_elf() -> Optional[str]:
    """Dynamically search build/ directory for compile ELF artifacts."""
    env_path = os.environ.get("MESHX_ELF_PATH")
    if env_path and os.path.exists(env_path):
        return env_path

    build_dir = os.path.abspath(os.path.join(os.path.dirname(__file__), "../../../build"))

    # Prioritize the build-path resolved ELF based on active BSP and product
    resolved_elf = os.path.join(build_dir, current_bsp, "Debug", current_product, f"meshx_build_{current_bsp}.elf")
    if os.path.exists(resolved_elf):
        return resolved_elf

    if os.path.exists(build_dir):
        for root, _, files in os.walk(build_dir):
            for f in files:
                if f.endswith(".elf") and "meshx_build" in f:
                    return os.path.join(root, f)
    return None

class AsyncSerialWorker:
    """
    Handles serial connection, sliding-window stream parsing,
    real-time event caching, and client subscriptions.
    """
    def __init__(self, port: str, baudrate: int = 115200):
        self.port = port
        self.baudrate = baudrate
        self.ser: Optional[serial.Serial] = None
        self.subscriptions: Set[WebSocket] = set()
        self.demux = StreamDemultiplexer()
        self.running = False
        self.read_task: Optional[asyncio.Task] = None

        # Hydration Cache
        self.state_cache = {
            "nodes": {},         # Address -> Element mapping
            "gpio": {},          # Pin -> Level/State
            "logs": [],          # Recent parsed logs (max 200)
            "raw_text": ""       # Recent console buffer (max 1000 chars)
        }

        # Initialize ELF Decoder
        elf_path = find_elf()
        if elf_path and MeshXLogDecoder:
            logger.info(f"Loaded ELF file for dynamic logging: {elf_path}")
            self.decoder = MeshXLogDecoder(elf_path)
        else:
            logger.warning("ELF log decoder unavailable, streaming raw hex address maps.")
            self.decoder = None

    def set_active_bsp_and_product(self, bsp: str, product: str):
        """Dynamically load/reload the ELF file when a new target configuration is selected in UI."""
        build_dir = os.path.abspath(os.path.join(os.path.dirname(__file__), "../../../build"))
        target_elf = os.path.join(build_dir, bsp, "Debug", product, f"meshx_build_{bsp}.elf")
        if os.path.exists(target_elf):
            if MeshXLogDecoder:
                logger.info(f"Dynamically loading selected ELF: {target_elf}")
                self.decoder = MeshXLogDecoder(target_elf)
                logger.info("Successfully loaded new ELF decoder.")
            else:
                logger.warning("MeshXLogDecoder class is not available")
        else:
            logger.warning(f"Target ELF not found at resolved path: {target_elf}")
            self.decoder = None

    async def start(self):
        if self.running:
            return
        self.running = True
        self.loop = asyncio.get_running_loop()
        try:
            # Run blocking Serial operations in executor
            self.ser = await self.loop.run_in_executor(
                None, lambda: serial.Serial(self.port, self.baudrate, timeout=0.1)
            )
            # Send ASCII CLI command to shift the MXCP channel over the active console log channel
            await self.loop.run_in_executor(None, self.ser.write, b"ut 8 1 1 1\n")
            await asyncio.sleep(0.2)
            # Send dynamic Hosted switch activation binary frame using structured helper
            self.send_cmd(0x01, bytes([0x01]))
            logger.info(f"Serial port {self.port} opened successfully.")
        except Exception as e:
            logger.error(f"Failed to open serial port {self.port}: {e}")
            self.running = False
            return

        self.read_task = asyncio.create_task(self.run_loop())

    async def run_loop(self):
        loop = asyncio.get_running_loop()
        while self.running and self.ser and self.ser.is_open:
            try:
                # Read serial in a non-blocking way
                data = await loop.run_in_executor(None, self.ser.read, 1024)
                if data:
                    self.demux.feed(
                        data,
                        on_log=self.handle_log,
                        on_mxsp=self.handle_mxsp,
                        on_text=self.handle_text
                    )
            except Exception as e:
                logger.error(f"Serial worker read error on port {self.port}: {e}")
                break
            await asyncio.sleep(0.01)
        self.running = False

    def handle_log(self, raw_pkt: bytes):
        level = "INF"
        msg = "Log resolution failed"
        addr = 0

        if len(raw_pkt) >= 13:  # sync(2) + len(1) + level(1) + module(1) + ts(4) + fmt_addr(4)
            # Payload starts at offset 3
            level_val, module_val, ts, fmt_addr = struct.unpack("<BBII", raw_pkt[3:13])
            addr = fmt_addr
            msg = f"Log resolved at RAM: 0x{addr:08X}"

        # If ELF available, try full decoding
        if self.decoder:
            try:
                res = self.decoder.decode_packet(raw_pkt)
                if res:
                    # Strip ANSI escape sequences for premium glassmorphic Web console
                    import re
                    clean_msg = re.sub(r'\x1b\[[0-9;]*m', '', res)
                    clean_msg = re.sub(r'\033\[[0-9;]*m', '', clean_msg)
                    msg = clean_msg

                    if "[E]" in clean_msg: level = "ERR"
                    elif "[W]" in clean_msg: level = "WRN"
                    elif "[D]" in clean_msg: level = "DBG"
            except Exception as e:
                logger.error(f"Failed to decode log packet: {e}")

        event = {
            "type": "log",
            "message": msg,
            "level": level,
            "address": f"0x{addr:08X}"
        }
        self.state_cache["logs"].append(event)
        if len(self.state_cache["logs"]) > 200:
            self.state_cache["logs"].pop(0)

        self.broadcast(event)

    def handle_text(self, data: bytes):
        text_str = data.decode("utf-8", errors="replace")
        self.state_cache["raw_text"] += text_str
        if len(self.state_cache["raw_text"]) > 1000:
            self.state_cache["raw_text"] = self.state_cache["raw_text"][-1000:]

        self.broadcast({
            "type": "text",
            "data": text_str
        })

    def handle_mxsp(self, msg_type: int, payload: bytes):
        # Broadcast raw MXSP stream
        self.broadcast({
            "type": "mxsp",
            "msg_type": msg_type,
            "payload": payload.hex()
        })

        # Parse standard GPIO states automatically
        EVT_GPIO_RSP_IDS = {0xA1, 0xA2, 0xA3, 0xA4, 0xA5, 0xA6, 0xA7, 0xA8, 0xA9}
        EVT_GPIO_RSP_WITH_DATA = {0xA2, 0xA3, 0xA8, 0xA9}
        if msg_type in EVT_GPIO_RSP_IDS:
            try:
                status, pin = struct.unpack("<BB", payload[:2])
                val = 0
                if msg_type in EVT_GPIO_RSP_WITH_DATA and len(payload) > 2:
                    val = payload[2]
                self.state_cache["gpio"][pin] = {
                    "level": val,
                    "status": status,
                    "cmd": msg_type
                }
                self.broadcast({
                    "type": "gpio_update",
                    "pin": pin,
                    "level": val,
                    "pwm_duty": val if msg_type == 0xA4 else None
                })
            except Exception as e:
                logger.error(f"Error parsing GPIO RSP: {e}")
        elif msg_type == 0xBE:
            try:
                evt_type, pin, val, _, ts = struct.unpack("<BBBBI", payload)
                self.state_cache["gpio"][pin] = {
                    "level": val,
                    "event_type": evt_type,
                    "timestamp": ts
                }
                self.broadcast({
                    "type": "gpio_update",
                    "pin": pin,
                    "level": val,
                    "pwm_duty": val if evt_type == 4 else None
                })
            except Exception as e:
                logger.error(f"Error parsing GPIO EVT: {e}")
        elif msg_type == 0x86:
            if len(payload) >= 1:
                try:
                    num_elements = payload[0]
                    offset = 1
                    discovered_nodes = {}
                    variant_map = {
                        0: "Relay Server",
                        1: "Relay Client",
                        2: "CWWW Light",
                        3: "CWWW Client",
                        4: "RGB Light",
                        5: "RGB Client",
                        6: "Sensor Server",
                        7: "Sensor Client"
                    }
                    for _ in range(num_elements):
                        if offset + 6 > len(payload):
                            break
                        idx, variant, el_type = struct.unpack("<HHH", payload[offset:offset+6])
                        offset += 6

                        end = offset
                        while end < len(payload) and payload[end] != 0:
                            end += 1
                        name = payload[offset:end].decode("utf-8", errors="replace")
                        offset = end + 1

                        type_badge = variant_map.get(variant, "Generic")
                        address = f"0x00{idx:02X}"

                        discovered_nodes[address] = {
                            "name": name,
                            "type": type_badge,
                            "value": 0,
                            "element_idx": idx,
                            "element_type": variant
                        }

                    self.state_cache["nodes"].update(discovered_nodes)

                    self.broadcast({
                        "type": "nodes_discovered",
                        "nodes": discovered_nodes
                    })
                except Exception as e:
                    logger.error(f"Error parsing Dynamic Composition response: {e}")
        elif msg_type == 0x87:
            if len(payload) >= 1:
                try:
                    num_elements = payload[0]
                    offset = 1
                    for _ in range(num_elements):
                        if offset + 8 > len(payload):
                            break
                        idx, variant, ctx_size, telemetry_size = struct.unpack("<HHHH", payload[offset:offset+8])
                        offset += 8

                        data = bytes()
                        if ctx_size > 0 and offset + ctx_size <= len(payload):
                            data = payload[offset:offset+ctx_size]
                        offset += ctx_size

                        address = f"0x00{idx:02X}"
                        
                        # Process telemetry data if available
                        if telemetry_size > 0 and offset + telemetry_size <= len(payload):
                            tel_data = payload[offset:offset+telemetry_size]
                            offset += telemetry_size
                            
                            val = 0
                            if variant in (0, 2, 4): # Relay/CWWW/RGB Servers
                                if len(tel_data) >= 1:
                                    val = tel_data[0]
                            elif variant == 6: # Sensor Server
                                if len(tel_data) >= 2:
                                    val = tel_data[0] | (tel_data[1] << 8)
                            elif variant == 1: # Relay Client (no padding)
                                if len(tel_data) >= 2:
                                    val = tel_data[1]
                            elif variant in (3, 5): # CWWW/RGB Clients (padding at data[1])
                                if len(tel_data) >= 3:
                                    val = tel_data[2]
                            elif variant == 7: # Sensor Client
                                if len(tel_data) >= 4:
                                    val = tel_data[2] | (tel_data[3] << 8)
                                    
                            if address in self.state_cache["nodes"]:
                                self.state_cache["nodes"][address]["value"] = val
                                self.state_cache["nodes"][address]["timestamp"] = time.time()
                                
                                # Broadcast the initial telemetry update
                                self.broadcast({
                                    "type": "telemetry_update",
                                    "element_id": idx,
                                    "element_type": variant,
                                    "func_id": 0,
                                    "value": val
                                })
                except Exception as e:
                    logger.error(f"Error parsing Element State Response: {e}")
        elif msg_type == 0x90:
            if len(payload) >= 8:
                try:
                    el_id, el_type, func_id, msg_len = struct.unpack("<HHHH", payload[:8])
                    data = payload[8:]

                    val = 0
                    if el_type in (0, 2, 4): # Relay/CWWW/RGB Servers
                        if func_id == 0 and len(data) >= 1:
                            val = data[0]
                    elif el_type == 6: # Sensor Server
                        if func_id == 0 and len(data) >= 2:
                            val = data[0] | (data[1] << 8)
                    elif el_type == 1: # Relay Client (no padding)
                        if func_id == 0 and len(data) >= 2:
                            val = data[1]
                    elif el_type in (3, 5): # CWWW/RGB Clients (padding at data[1])
                        if func_id == 0 and len(data) >= 3:
                            val = data[2]
                    elif el_type == 7: # Sensor Client
                        if func_id == 0 and len(data) >= 4:
                            val = data[2] | (data[3] << 8)

                    address = f"0x00{el_id:02X}"

                    # Update local state cache
                    if address in self.state_cache["nodes"]:
                        self.state_cache["nodes"][address]["value"] = val
                    else:
                        variant_map = {
                            0: "Relay Server",
                            1: "Relay Client",
                            2: "CWWW Light",
                            3: "CWWW Client",
                            4: "RGB Light",
                            5: "RGB Client",
                            6: "Sensor Server",
                            7: "Sensor Client"
                        }
                        self.state_cache["nodes"][address] = {
                            "name": f"Dynamic {variant_map.get(el_type, 'Element')} {el_id}",
                            "type": variant_map.get(el_type, "Generic"),
                            "value": val,
                            "element_idx": el_id,
                            "element_type": el_type
                        }

                    self.broadcast({
                        "type": "node_state_update",
                        "address": address,
                        "value": val,
                        "element_idx": el_id,
                        "element_type": el_type,
                        "func_id": func_id,
                        "data_hex": data.hex().upper()
                    })
                except Exception as e:
                    logger.error(f"Error parsing DATA_EVT_NOTIFY payload: {e}")

    def send_cmd(self, msg_type: int, payload: bytes):
        """Build MXSP frame and write directly to physical port."""
        if not self.ser or not self.ser.is_open:
            return

        length = len(payload)
        frame = bytearray([0xFE, length, msg_type])
        frame.extend(payload)

        # XOR Checksum
        checksum = length ^ msg_type
        for b in payload:
            checksum ^= b

        frame.append(checksum)
        frame.append(0xEF)

        self.ser.write(bytes(frame))

        # Broadcast outgoing MXSP command packet to web clients
        self.handle_mxsp(msg_type, payload)

    def send_text(self, text: str):
        if self.ser and self.ser.is_open:
            self.ser.write(text.encode() + b'\n')

    def subscribe(self, websocket: WebSocket):
        self.subscriptions.add(websocket)

    def unsubscribe(self, websocket: WebSocket):
        self.subscriptions.discard(websocket)

    def broadcast(self, event: dict):
        if hasattr(self, "loop") and self.loop and self.loop.is_running():
            try:
                current_loop = asyncio.get_running_loop()
                if current_loop == self.loop:
                    self.loop.create_task(self._async_broadcast(event))
                    return
            except RuntimeError:
                pass
            asyncio.run_coroutine_threadsafe(self._async_broadcast(event), self.loop)
        else:
            try:
                loop = asyncio.get_running_loop()
                loop.create_task(self._async_broadcast(event))
            except RuntimeError:
                logger.error("Cannot broadcast event: no running event loop found.")

    async def _async_broadcast(self, event: dict):
        disconnected = []
        for ws in self.subscriptions:
            try:
                await ws.send_json(event)
            except Exception:
                disconnected.append(ws)
        for ws in disconnected:
            self.subscriptions.discard(ws)

    def get_cached_state(self) -> dict:
        return self.state_cache

    def close(self):
        self.running = False
        if self.read_task:
            self.read_task.cancel()
        if self.ser:
            self.ser.close()

    def pause(self):
        """Release the serial port temporarily so other tools can use it."""
        self.running = False
        if self.read_task:
            self.read_task.cancel()
            self.read_task = None
        if self.ser:
            self.ser.close()
            self.ser = None
        logger.info(f"Serial port {self.port} paused.")

    async def resume(self):
        """Reopen the serial port and resume the read loop."""
        if self.running:
            return
        self.running = True
        self.loop = asyncio.get_running_loop()
        try:
            self.ser = await self.loop.run_in_executor(
                None, lambda: serial.Serial(self.port, self.baudrate, timeout=0.1)
            )
            # Send ASCII CLI command to shift the MXCP channel over the active console log channel
            await self.loop.run_in_executor(None, self.ser.write, b"ut 8 1 1 1\n")
            await asyncio.sleep(0.2)
            # Send dynamic Hosted switch activation binary frame using structured helper
            self.send_cmd(0x01, bytes([0x01]))
            logger.info(f"Serial port {self.port} resumed successfully.")
        except Exception as e:
            logger.error(f"Failed to resume serial port {self.port}: {e}")
            self.running = False
            return

        self.read_task = asyncio.create_task(self.run_loop())


# Store references to background tasks to prevent garbage collection
background_tasks = set()
active_workers: Dict[str, AsyncSerialWorker] = {}

@app.get("/api/ports")
def get_available_ports():
    ports = serial.tools.list_ports.comports()
    return [{"port": p.device, "desc": p.description} for p in ports]

def get_current_bsp() -> str:
    elf_path = find_elf()
    if elf_path:
        parts = elf_path.split(os.sep)
        if "build" in parts:
            idx = parts.index("build")
            if idx + 1 < len(parts):
                return parts[idx + 1]
    return "xiao_c3"

def get_current_product() -> str:
    elf_path = find_elf()
    if elf_path:
        parts = elf_path.split(os.sep)
        if "build" in parts:
            idx = parts.index("build")
            # build/[bsp]/[build_type]/[product_name]
            if idx + 3 < len(parts):
                return parts[idx + 3]
    return "all_in_one"

async def run_flash_subprocess(worker: AsyncSerialWorker, port: str, bsp: str, product: str, erase: bool):
    try:
        worker.pause()
        worker.broadcast({
            "type": "text",
            "data": f"\n\n[System] >>> Starting flash pipeline on target BSP: '{bsp}', Product: '{product}' over port '{port}'...\n"
        })

        export_path = "/run/media/pchanda/workspace/wsl/esp/v5.4/esp-idf/export.sh"
        if not os.path.exists(export_path):
            # Try a couple of common fallback paths just in case
            fallbacks = [
                os.path.expanduser("~/esp/esp-idf/export.sh"),
                "/run/media/pchanda/workspace/wsl/esp/v5.4/esp-idf/export.sh"
            ]
            for fb in fallbacks:
                if os.path.exists(fb):
                    export_path = fb
                    break

        meshx_script = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", "..", "scripts", "meshx.py"))

        cmds = []
        if erase:
            worker.broadcast({
                "type": "text",
                "data": "[System] >>> Step 1: Performing Full Chip Erase...\n"
            })
            cmds.append(f"python3 {meshx_script} -B {bsp} -N {product} -P {port} -E chip")

        worker.broadcast({
            "type": "text",
            "data": "[System] >>> Step 2: Flashing Firmware...\n"
        })
        cmds.append(f"python3 {meshx_script} -B {bsp} -N {product} -P {port} -F firmware")

        # Combine into a single bash execution string with env sourced
        full_cmd = f"source tools/scripts/env.sh source {export_path} && " + " && ".join(cmds)

        worker.broadcast({
            "type": "text",
            "data": f"[Command] Running: {full_cmd}\n"
        })

        # Multi-layered fail-safe: add common .espressif bin paths to PATH
        custom_env = os.environ.copy()
        espressif_bin_paths = []
        espressif_dir = os.path.expanduser("~/.espressif/python_env")
        if os.path.exists(espressif_dir):
            for d in os.listdir(espressif_dir):
                bin_dir = os.path.join(espressif_dir, d, "bin")
                if os.path.isdir(bin_dir):
                    espressif_bin_paths.append(bin_dir)

        if espressif_bin_paths:
            custom_env["PATH"] = ":".join(espressif_bin_paths) + ":" + custom_env.get("PATH", "")

        process = await asyncio.create_subprocess_exec(
            "/bin/bash", "-c", full_cmd,
            stdout=asyncio.subprocess.PIPE,
            stderr=asyncio.subprocess.STDOUT,
            cwd=os.path.abspath(os.path.join(os.path.dirname(__file__), "../../..")),
            env=custom_env
        )

        while True:
            line = await process.stdout.readline()
            if not line:
                break
            decoded_line = line.decode("utf-8", errors="replace")
            worker.broadcast({
                "type": "text",
                "data": decoded_line
            })

        await process.wait()
        if process.returncode != 0:
            worker.broadcast({
                "type": "text",
                "data": f"\n[System] ERROR: Flash command failed with exit code {process.returncode}!\n"
            })
            return

        worker.broadcast({
            "type": "text",
            "data": "\n[System] SUCCESS: Flashing completed successfully!\n"
        })
        # Reload the ELF decoder since the build process likely updated the ELF binary
        worker.set_active_bsp_and_product(bsp, product)
    except Exception as e:
        logger.error(f"Error in flashing process: {e}")
        worker.broadcast({
            "type": "text",
            "data": f"\n[System] ERROR: Flashing process failed: {e}\n"
        })
    finally:
        worker.broadcast({
            "type": "text",
            "data": "[System] >>> Resuming serial monitoring...\n\n"
        })
        await worker.resume()

@app.get("/api/config-metadata")
def get_config_metadata():
    bsp_root = os.path.abspath(os.path.join(os.path.dirname(__file__), "../../../port/bsp"))
    bsps = {}
    if os.path.exists(bsp_root):
        for bsp in os.listdir(bsp_root):
            bsp_dir = os.path.join(bsp_root, bsp)
            if os.path.isdir(bsp_dir) and not bsp.startswith('.'):
                yaml_path = os.path.join(bsp_dir, "prod_profile.yml")
                if os.path.exists(yaml_path):
                    try:
                        with open(yaml_path, 'r') as f:
                            data = yaml.safe_load(f)
                            products = data.get("prod", {}).get("products", [])
                            product_names = [p.get("name") for p in products if p.get("name")]
                            bsps[bsp] = product_names
                    except Exception as e:
                        logger.error(f"Error parsing prod_profile.yml for {bsp}: {e}")
    return {"bsps": bsps}

@app.post("/api/port/set-bsp-profile")
def set_port_bsp_profile(port: str, bsp: str, product: str):
    global current_bsp, current_product
    current_bsp = bsp
    current_product = product
    if port in active_workers:
        worker = active_workers[port]
        worker.set_active_bsp_and_product(bsp, product)
    return {"status": "success", "message": f"Active BSP set to {bsp} and Profile to {product}"}

@app.post("/api/flash")
async def flash_device(port: str, bsp: str, product: str, erase: bool = False):
    if port not in active_workers:
        raise HTTPException(status_code=404, detail="Serial port not connected")

    worker = active_workers[port]

    global current_bsp, current_product
    current_bsp = bsp
    current_product = product
    worker.set_active_bsp_and_product(bsp, product)

    try:
        await run_flash_subprocess(worker, port, bsp, product, erase)
        return {"status": "success", "message": "Flashing process completed."}
    except Exception as e:
        raise HTTPException(status_code=500, detail=str(e))

@app.post("/api/flash/config")
async def flash_custom_config(port: str, bsp: str, product: str):
    if port not in active_workers:
        raise HTTPException(status_code=404, detail="Serial port not connected")

    worker = active_workers[port]

    global current_bsp, current_product
    current_bsp = bsp
    current_product = product
    worker.set_active_bsp_and_product(bsp, product)

    async def run_cfg_flash():
        try:
            worker.pause()
            worker.broadcast({
                "type": "text",
                "data": f"\n\n[System] >>> Starting flash pipeline for CONFIG profile '{product}' (BSP '{bsp}') over port '{port}'...\n"
            })

            export_path = "/run/media/pchanda/workspace/wsl/esp/v5.4/esp-idf/export.sh"
            if not os.path.exists(export_path):
                fallbacks = [
                    os.path.expanduser("~/esp/esp-idf/export.sh"),
                    "/run/media/pchanda/workspace/wsl/esp/v5.4/esp-idf/export.sh"
                ]
                for fb in fallbacks:
                    if os.path.exists(fb):
                        export_path = fb
                        break

            custom_env = os.environ.copy()
            espressif_bin_paths = []
            espressif_dir = os.path.expanduser("~/.espressif/python_env")
            if os.path.exists(espressif_dir):
                for d in os.listdir(espressif_dir):
                    bin_dir = os.path.join(espressif_dir, d, "bin")
                    if os.path.isdir(bin_dir):
                        espressif_bin_paths.append(bin_dir)

            if espressif_bin_paths:
                custom_env["PATH"] = ":".join(espressif_bin_paths) + ":" + custom_env.get("PATH", "")

            meshx_script = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", "..", "scripts", "meshx.py"))
            full_cmd = f"source tools/scripts/env.sh source {export_path} && python3 {meshx_script} -B {bsp} -N {product} -P {port} -F cfg"

            worker.broadcast({
                "type": "text",
                "data": f"[Command] Running: {full_cmd}\n\n"
            })

            process = await asyncio.create_subprocess_exec(
                "/bin/bash", "-c", full_cmd,
                stdout=asyncio.subprocess.PIPE,
                stderr=asyncio.subprocess.PIPE,
                cwd=os.path.abspath(os.path.join(os.path.dirname(__file__), "..", "..", "..")),
                env=custom_env
            )

            async def stream_output(stream):
                while True:
                    line = await stream.readline()
                    if not line:
                        break
                    worker.broadcast({
                        "type": "text",
                        "data": line.decode("utf-8", errors="replace")
                    })

            await asyncio.gather(
                stream_output(process.stdout),
                stream_output(process.stderr)
            )

            await process.wait()
            if process.returncode == 0:
                worker.broadcast({
                    "type": "text",
                    "data": "\n[System] SUCCESS: Configuration flashed successfully!\n"
                })
                # Reload the ELF decoder since the build process likely updated the ELF binary
                worker.set_active_bsp_and_product(bsp, product)
            else:
                worker.broadcast({
                    "type": "text",
                    "data": f"\n[System] ERROR: Configuration flashing failed with exit code {process.returncode}\n"
                })
        except Exception as e:
            logger.error(f"Error flashing custom config: {e}")
            worker.broadcast({
                "type": "text",
                "data": f"\n[System] ERROR: Flashing failed: {e}\n"
            })
            raise HTTPException(status_code=500, detail=str(e))
        finally:
            worker.broadcast({
                "type": "text",
                "data": "[System] >>> Resuming serial monitoring...\n\n"
            })
            await worker.resume()

    await run_cfg_flash()
    return {"status": "success", "message": "Configuration flashing completed."}

@app.post("/api/port/connect")
async def connect_port(port: str):
    if port not in active_workers:
        worker = AsyncSerialWorker(port)
        await worker.start()
        active_workers[port] = worker
    return {"status": "success", "port": port}

@app.post("/api/gpio/command")
async def send_gpio_command(port: str, pin: int, cmd: int, value: int = 0):
    if port not in active_workers:
        raise HTTPException(status_code=404, detail="Serial port not connected")

    worker = active_workers[port]
    CMD_GPIO_SET_LEVEL    = 0x21
    CMD_GPIO_GET_LEVEL    = 0x22
    CMD_GPIO_TOGGLE       = 0x23
    CMD_GPIO_SET_PWM_DUTY = 0x24
    CMD_GPIO_SET_PWM_FREQ = 0x25
    CMD_GPIO_INTR_ENABLE  = 0x26
    CMD_GPIO_GET_CONFIG   = 0x28
    CMD_GPIO_GET_STATE    = 0x29

    GPIO_CMD_MAP = {1: CMD_GPIO_SET_LEVEL, 2: CMD_GPIO_GET_LEVEL, 3: CMD_GPIO_TOGGLE,
                    4: CMD_GPIO_SET_PWM_DUTY, 5: CMD_GPIO_SET_PWM_FREQ,
                    6: CMD_GPIO_INTR_ENABLE, 8: CMD_GPIO_GET_CONFIG, 9: CMD_GPIO_GET_STATE}

    cmd_type = GPIO_CMD_MAP.get(cmd, CMD_GPIO_SET_LEVEL)
    payload = struct.pack("<BB", pin, value)
    worker.send_cmd(cmd_type, bytes(payload))
    return {"status": "success"}

@app.post("/api/cli/send")
async def send_cli_command(port: str, command: str):
    if port not in active_workers:
        raise HTTPException(status_code=404, detail="Serial port not connected")

    worker = active_workers[port]
    worker.send_text(command)
    return {"status": "success"}

@app.post("/api/mxcp/enable")
async def enable_mxcp(port: str):
    if port not in active_workers:
        raise HTTPException(status_code=404, detail="Serial port not connected")

    worker = active_workers[port]
    if worker.ser and worker.ser.is_open:
        # 1. Send ASCII CLI command
        worker.ser.write(b"ut 8 1 1 1\n")
        await asyncio.sleep(0.25)
        # 2. Send dynamic Hosted switch activation binary frame using structured helper
        worker.send_cmd(0x01, bytes([0x01]))

        await asyncio.sleep(0.15)
        worker.send_cmd(0x03, b'')

        return {"status": "success", "message": "MXCP Routing enabled and Element Composition requested."}
    else:
        raise HTTPException(status_code=500, detail="Serial port is closed or invalid")

@app.post("/api/composition/request")
async def request_composition(port: str):
    if port not in active_workers:
        raise HTTPException(status_code=404, detail="Serial port not connected")

    worker = active_workers[port]
    worker.send_cmd(0x03, b'')
    return {"status": "success"}

@app.websocket("/ws/events")
async def websocket_endpoint(websocket: WebSocket, port: str = Query(...)):
    await websocket.accept()

    if port not in active_workers:
        worker = AsyncSerialWorker(port)
        await worker.start()
        active_workers[port] = worker
    else:
        worker = active_workers[port]

    worker.subscribe(websocket)

    # Send current state hydration instantly
    await websocket.send_json({
        "type": "hydration",
        "state": worker.get_cached_state()
    })

    # Request dynamic composition query automatically on connection
    try:
        worker.send_cmd(0x03, b'')
    except Exception as e:
        logger.error(f"Failed to auto-send composition request: {e}")

    try:
        while True:
            # Keep socket alive and listen for browser client commands
            data = await websocket.receive_text()
            try:
                js = json.loads(data)
                if js.get("type") == "cli":
                    worker.send_text(js.get("command", ""))
                elif js.get("type") == "gpio":
                    pin = js.get("pin")
                    cmd = js.get("cmd")
                    val = js.get("value", 0)
                    CMD_GPIO_SET_LEVEL    = 0x21
                    CMD_GPIO_GET_LEVEL    = 0x22
                    CMD_GPIO_TOGGLE       = 0x23
                    CMD_GPIO_SET_PWM_DUTY = 0x24
                    CMD_GPIO_SET_PWM_FREQ = 0x25
                    CMD_GPIO_INTR_ENABLE  = 0x26
                    CMD_GPIO_GET_CONFIG   = 0x28
                    CMD_GPIO_GET_STATE    = 0x29

                    GPIO_CMD_MAP = {1: CMD_GPIO_SET_LEVEL, 2: CMD_GPIO_GET_LEVEL, 3: CMD_GPIO_TOGGLE,
                                    4: CMD_GPIO_SET_PWM_DUTY, 5: CMD_GPIO_SET_PWM_FREQ,
                                    6: CMD_GPIO_INTR_ENABLE, 8: CMD_GPIO_GET_CONFIG, 9: CMD_GPIO_GET_STATE}

                    cmd_type = GPIO_CMD_MAP.get(cmd, CMD_GPIO_SET_LEVEL)
                    payload = struct.pack("<BB", pin, val)
                    worker.send_cmd(cmd_type, bytes(payload))
                elif js.get("type") == "el_cmd":
                    # Pack meshx_app_element_msg_header_t
                    # element_id(2), element_type(2), func_id(2), msg_len(2)
                    el_id = js.get("element_idx")
                    el_type = js.get("element_type")
                    func_id = js.get("func_id")
                    value = js.get("value")

                    payload = struct.pack("<HHHHB", el_id, el_type, func_id, 1, value)
                    worker.send_cmd(0x10, payload)
                elif js.get("type") == "el_cmd_cwww":
                    el_id = js.get("element_idx")
                    el_type = js.get("element_type")
                    func_id = js.get("func_id")
                    lightness = js.get("lightness")
                    temp = js.get("temperature")
                    payload = struct.pack("<HHHHHH", el_id, el_type, func_id, 4, lightness, temp)
                    worker.send_cmd(0x10, payload)
                elif js.get("type") == "el_cmd_hsl":
                    el_id = js.get("element_idx")
                    el_type = js.get("element_type")
                    func_id = js.get("func_id")
                    h = js.get("hue")
                    s = js.get("saturation")
                    l = js.get("lightness")
                    payload = struct.pack("<HHHHHHH", el_id, el_type, func_id, 6, h, s, l)
                    worker.send_cmd(0x10, payload)
                elif js.get("type") == "el_cmd_sensor":
                    el_id = js.get("element_idx")
                    el_type = js.get("element_type")
                    func_id = js.get("func_id")
                    payload = struct.pack("<HHHH", el_id, el_type, func_id, 0)
                    worker.send_cmd(0x10, payload)
            except json.JSONDecodeError:
                pass
    except WebSocketDisconnect:
        pass
    finally:
        worker.unsubscribe(websocket)
        # Clean up worker and release the port if no clients remain
        if len(worker.subscriptions) == 0:
            worker.close()
            if port in active_workers:
                del active_workers[port]
            logger.info(f"Released serial port {port} as all clients disconnected.")

frontend_dir = os.path.abspath(os.path.join(os.path.dirname(__file__), "../frontend"))
if os.path.exists(frontend_dir):
    @app.get("/")
    def get_index():
        return FileResponse(os.path.join(frontend_dir, "index.html"), headers={"Cache-Control": "no-cache"})

    @app.get("/index.css")
    def get_css():
        return FileResponse(os.path.join(frontend_dir, "index.css"), headers={"Cache-Control": "no-cache"})

    @app.get("/app.js")
    def get_js():
        return FileResponse(os.path.join(frontend_dir, "app.js"), headers={"Cache-Control": "no-cache, no-store, must-revalidate"})
