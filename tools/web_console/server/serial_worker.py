import asyncio
import os
import sys
import struct
import logging
import serial
import time
import re
from typing import Dict, Set, Optional
from fastapi import WebSocket

# Add SDK to path
sys.path.append(os.path.abspath(os.path.join(os.path.dirname(__file__), "../../host_sdk/python")))
from meshx_api import (
    MeshXSDK, MeshXCtrlMsg, MeshXDataMsg,
    MESHX_MSG_DATA_EVT_RX_NOTIFY,
    MESHX_MSG_CTRL_EVT_COMPOSITION_RSP,
    MESHX_MSG_CTRL_EVT_ELEMENT_STATE_RSP,
    MESHX_MSG_CTRL_EVT_GPIO_SET_LEVEL_RSP,
    MESHX_MSG_CTRL_EVT_GPIO_GET_LEVEL_RSP,
    MESHX_MSG_CTRL_EVT_GPIO_TOGGLE_RSP,
    MESHX_MSG_CTRL_EVT_GPIO_SET_PWM_DUTY_RSP,
    MESHX_MSG_CTRL_EVT_GPIO_SET_PWM_FREQ_RSP,
    MESHX_MSG_CTRL_EVT_GPIO_INTR_ENABLE_RSP,
    MESHX_MSG_CTRL_EVT_GPIO_INTR_DISABLE_RSP,
    MESHX_MSG_CTRL_EVT_GPIO_GET_CONFIG_RSP,
    MESHX_MSG_CTRL_EVT_GPIO_GET_STATE_RSP,
    MESHX_MSG_CTRL_EVT_GPIO_ASYNC,
    MESHX_MSG_CTRL_EVT_GPIO_ERROR
)

# Add server and scripts directories to path to import local modules
sys.path.append(os.path.dirname(os.path.abspath(__file__)))
from demux import StreamDemultiplexer

# Try importing ELF decoder from tools/scripts
sys.path.append(os.path.abspath(os.path.join(os.path.dirname(__file__), "..", "..", "scripts")))
try:
    from host_decoder import MeshXLogDecoder
except ImportError:
    MeshXLogDecoder = None

logger = logging.getLogger("MeshXConsole.Serial")

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

def get_current_bsp() -> str:
    elf_path = find_elf()
    if elf_path:
        parts = elf_path.split(os.sep)
        if "build" in parts:
            idx = parts.index("build")
            if idx + 1 < len(parts):
                return parts[idx + 1]
    return current_bsp

def get_current_product() -> str:
    elf_path = find_elf()
    if elf_path:
        parts = elf_path.split(os.sep)
        if "build" in parts:
            idx = parts.index("build")
            # build/[bsp]/[build_type]/[product_name]
            if idx + 3 < len(parts):
                return parts[idx + 3]
    return current_product

class SDKSerialWrapper:
    def __init__(self, worker):
        self.worker = worker
    def write(self, data: bytes):
        if len(data) >= 2:
            msg_id = struct.unpack('<H', data[:2])[0]
            payload = data[2:]
            self.worker.send_cmd(msg_id, payload)

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

        # Init SDK
        self.sdk = MeshXSDK(serial_port=SDKSerialWrapper(self))
        self.sdk.register_data_cb(self.on_meshx_data)
        self.sdk.register_ctrl_cb(self.on_meshx_ctrl)

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
            self.sdk.ctrl_send(MeshXCtrlMsg(msg_id=0x01, payload=bytes([0x01])))
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

    def on_meshx_data(self, msg: MeshXDataMsg):
        if msg.msg_id == MESHX_MSG_DATA_EVT_RX_NOTIFY:
            val = 0
            if msg.element_type in (0, 2, 4): # Relay/CWWW/RGB Servers
                if msg.func_id == 0 and len(msg.payload) >= 1:
                    val = msg.payload[0]
            elif msg.element_type == 6: # Sensor Server
                if msg.func_id == 0 and len(msg.payload) >= 2:
                    val = msg.payload[0] | (msg.payload[1] << 8)
            elif msg.element_type == 1: # Relay Client (no padding)
                if msg.func_id == 0 and len(msg.payload) >= 2:
                    val = msg.payload[1]
            elif msg.element_type in (3, 5): # CWWW/RGB Clients
                if msg.func_id == 0 and len(msg.payload) >= 3:
                    val = msg.payload[2]
            elif msg.element_type == 7: # Sensor Client
                if msg.func_id == 0 and len(msg.payload) >= 4:
                    val = msg.payload[2] | (msg.payload[3] << 8)

            address = f"0x00{msg.element_id:02X}"

            if address in self.state_cache["nodes"]:
                self.state_cache["nodes"][address]["value"] = val
            else:
                variant_map = {
                    0: "Relay Server", 1: "Relay Client", 2: "CWWW Light", 3: "CWWW Client",
                    4: "RGB Light", 5: "RGB Client", 6: "Sensor Server", 7: "Sensor Client"
                }
                self.state_cache["nodes"][address] = {
                    "name": f"Dynamic {variant_map.get(msg.element_type, 'Element')} {msg.element_id}",
                    "type": variant_map.get(msg.element_type, "Generic"),
                    "value": val,
                    "element_idx": msg.element_id,
                    "element_type": msg.element_type
                }

            self.broadcast({
                "type": "node_state_update",
                "address": address,
                "value": val,
                "element_idx": msg.element_id,
                "element_type": msg.element_type,
                "func_id": msg.func_id,
                "data_hex": msg.payload.hex().upper()
            })

    def on_meshx_ctrl(self, msg: MeshXCtrlMsg):
        EVT_GPIO_RSP_IDS = {
            MESHX_MSG_CTRL_EVT_GPIO_SET_LEVEL_RSP,
            MESHX_MSG_CTRL_EVT_GPIO_GET_LEVEL_RSP,
            MESHX_MSG_CTRL_EVT_GPIO_TOGGLE_RSP,
            MESHX_MSG_CTRL_EVT_GPIO_SET_PWM_DUTY_RSP,
            MESHX_MSG_CTRL_EVT_GPIO_SET_PWM_FREQ_RSP,
            MESHX_MSG_CTRL_EVT_GPIO_INTR_ENABLE_RSP,
            MESHX_MSG_CTRL_EVT_GPIO_INTR_DISABLE_RSP,
            MESHX_MSG_CTRL_EVT_GPIO_GET_CONFIG_RSP,
            MESHX_MSG_CTRL_EVT_GPIO_GET_STATE_RSP,
        }
        EVT_GPIO_RSP_WITH_DATA = {
            MESHX_MSG_CTRL_EVT_GPIO_GET_LEVEL_RSP,
            MESHX_MSG_CTRL_EVT_GPIO_TOGGLE_RSP,
            MESHX_MSG_CTRL_EVT_GPIO_GET_CONFIG_RSP,
            MESHX_MSG_CTRL_EVT_GPIO_GET_STATE_RSP,
        }
        if msg.msg_id in EVT_GPIO_RSP_IDS:
            try:
                status, pin = struct.unpack("<BB", msg.payload[:2])
                val = 0
                if msg.msg_id in EVT_GPIO_RSP_WITH_DATA and len(msg.payload) > 2:
                    val = msg.payload[2]
                self.state_cache["gpio"][pin] = {
                    "level": val,
                    "status": status,
                    "cmd": msg.msg_id
                }
                self.broadcast({
                    "type": "gpio_update",
                    "pin": pin,
                    "level": val,
                    "pwm_duty": val if msg.msg_id == MESHX_MSG_CTRL_EVT_GPIO_SET_PWM_DUTY_RSP else None
                })
            except Exception as e:
                logger.error(f"Error parsing GPIO RSP: {e}")
        elif msg.msg_id == MESHX_MSG_CTRL_EVT_GPIO_ASYNC:
            try:
                evt_type, pin, val, _, ts = struct.unpack("<BBBBI", msg.payload)
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
        elif msg.msg_id == MESHX_MSG_CTRL_EVT_COMPOSITION_RSP:
            if len(msg.payload) >= 1:
                try:
                    num_elements = msg.payload[0]
                    offset = 1
                    discovered_nodes = {}
                    variant_map = {
                        0: "Relay Server", 1: "Relay Client", 2: "CWWW Light", 3: "CWWW Client",
                        4: "RGB Light", 5: "RGB Client", 6: "Sensor Server", 7: "Sensor Client"
                    }
                    for _ in range(num_elements):
                        if offset + 6 > len(msg.payload):
                            break
                        idx, variant, el_type = struct.unpack("<HHH", msg.payload[offset:offset+6])
                        offset += 6

                        end = offset
                        while end < len(msg.payload) and msg.payload[end] != 0:
                            end += 1
                        name = msg.payload[offset:end].decode("utf-8", errors="replace")
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
        elif msg.msg_id == MESHX_MSG_CTRL_EVT_ELEMENT_STATE_RSP:
            if len(msg.payload) >= 1:
                try:
                    num_elements = msg.payload[0]
                    offset = 1
                    for _ in range(num_elements):
                        if offset + 8 > len(msg.payload):
                            break
                        idx, variant, ctx_size, telemetry_size = struct.unpack("<HHHH", msg.payload[offset:offset+8])
                        offset += 8

                        data = bytes()
                        if ctx_size > 0 and offset + ctx_size <= len(msg.payload):
                            data = msg.payload[offset:offset+ctx_size]
                        offset += ctx_size

                        address = f"0x00{idx:02X}"

                        if telemetry_size > 0 and offset + telemetry_size <= len(msg.payload):
                            tel_data = msg.payload[offset:offset+telemetry_size]
                            offset += telemetry_size

                            val = 0
                            if variant in (0, 2, 4):
                                if len(tel_data) >= 1: val = tel_data[0]
                            elif variant == 6:
                                if len(tel_data) >= 2: val = tel_data[0] | (tel_data[1] << 8)
                            elif variant == 1:
                                if len(tel_data) >= 2: val = tel_data[1]
                            elif variant in (3, 5):
                                if len(tel_data) >= 3: val = tel_data[2]
                            elif variant == 7:
                                if len(tel_data) >= 4: val = tel_data[2] | (tel_data[3] << 8)

                            if address in self.state_cache["nodes"]:
                                self.state_cache["nodes"][address]["value"] = val
                                self.state_cache["nodes"][address]["timestamp"] = time.time()

                                self.broadcast({
                                    "type": "telemetry_update",
                                    "element_id": idx,
                                    "element_type": variant,
                                    "func_id": 0,
                                    "value": val
                                })
                except Exception as e:
                    logger.error(f"Error parsing Element State Response: {e}")

    def handle_mxsp(self, msg_type: int, payload: bytes):
        self.broadcast({
            "type": "mxsp",
            "msg_type": msg_type,
            "payload": payload.hex()
        })
        try:
            full_frame = struct.pack('<H', msg_type) + payload
            self.sdk.handle_rx(full_frame)
        except Exception as e:
            logger.error(f"SDK Handle Error: {e}")

    def send_cmd(self, msg_type: int, payload: bytes):
        """Build MXSP frame and write directly to physical port."""
        if not self.ser or not self.ser.is_open:
            return

        length = len(payload)
        frame = bytearray([0xFE, length])
        frame.append(msg_type & 0xFF)
        frame.append((msg_type >> 8) & 0xFF)
        frame.extend(payload)

        # XOR Checksum
        checksum = length ^ (msg_type & 0xFF) ^ ((msg_type >> 8) & 0xFF)
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
            self.sdk.ctrl_send(MeshXCtrlMsg(msg_id=0x01, payload=bytes([0x01])))
            logger.info(f"Serial port {self.port} resumed successfully.")
        except Exception as e:
            logger.error(f"Failed to resume serial port {self.port}: {e}")
            self.running = False
            return

        self.read_task = asyncio.create_task(self.run_loop())
