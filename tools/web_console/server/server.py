import asyncio
import os
import sys
import struct
import json
import logging
from typing import Dict, Set, Optional
from fastapi import FastAPI, WebSocket, WebSocketDisconnect, Query, HTTPException
from fastapi.middleware.cors import CORSMiddleware
import serial
import serial.tools.list_ports

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

def find_elf() -> Optional[str]:
    """Dynamically search build/ directory for compile ELF artifacts."""
    env_path = os.environ.get("MESHX_ELF_PATH")
    if env_path and os.path.exists(env_path):
        return env_path
        
    build_dir = os.path.abspath(os.path.join(os.path.dirname(__file__), "../../../build"))
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

    def start(self):
        if self.running:
            return
        self.running = True
        try:
            self.ser = serial.Serial(self.port, self.baudrate, timeout=0.1)
            # Send dynamic Hosted switch activation binary frame
            # 0xFE SOF + 0x01 Len + 0x03 Type + 0x01 Payload (hosted on) + 0x03 Checksum + 0xEF EOF
            hosted_enable_frame = bytes([0xFE, 0x01, 0x03, 0x01, 0x03, 0xEF])
            self.ser.write(hosted_enable_frame)
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
        if msg_type == 0xD2: # GPIO Response
            try:
                # cmd(1), pin(1), status(1), response_len(1), response(8)
                cmd, pin, status, rlen = struct.unpack("<BBBB", payload[:4])
                val = payload[4] if rlen > 0 else 0
                self.state_cache["gpio"][pin] = {
                    "level": val,
                    "status": status,
                    "cmd": cmd
                }
                self.broadcast({
                    "type": "gpio_update",
                    "pin": pin,
                    "level": val,
                    "pwm_duty": val if cmd == 4 else None
                })
            except Exception as e:
                logger.error(f"Error parsing GPIO RSP: {e}")
        elif msg_type == 0xD3: # GPIO async Event
            try:
                # event_type(1), pin(1), value(1), reserved(1), timestamp(4)
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

    def send_text(self, text: str):
        if self.ser and self.ser.is_open:
            self.ser.write(text.encode() + b'\n')

    def subscribe(self, websocket: WebSocket):
        self.subscriptions.add(websocket)

    def unsubscribe(self, websocket: WebSocket):
        self.subscriptions.discard(websocket)

    def broadcast(self, event: dict):
        # We run it safely as asyncio call
        asyncio.create_task(self._async_broadcast(event))

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


active_workers: Dict[str, AsyncSerialWorker] = {}

@app.get("/api/ports")
def get_available_ports():
    ports = serial.tools.list_ports.comports()
    return [{"port": p.device, "desc": p.description} for p in ports]

@app.post("/api/port/connect")
def connect_port(port: str):
    if port not in active_workers:
        worker = AsyncSerialWorker(port)
        worker.start()
        active_workers[port] = worker
    return {"status": "success", "port": port}

@app.post("/api/gpio/command")
def send_gpio_command(port: str, pin: int, cmd: int, value: int = 0):
    if port not in active_workers:
        raise HTTPException(status_code=404, detail="Serial port not connected")
        
    worker = active_workers[port]
    # Structure of mxsp_gpio_cmd_payload_t:
    # cmd(1), logical_pin(1), reserved(1), payload_len(1), payload(8)
    payload = bytearray([cmd, pin, 0, 1])
    payload.append(value)
    payload.extend([0]*7) # Padding to 8 payload bytes
    
    worker.send_cmd(0xD1, bytes(payload)) # MXSP_MSG_TYPE_GPIO_CMD = 0xD1
    return {"status": "success"}

@app.post("/api/cli/send")
def send_cli_command(port: str, command: str):
    if port not in active_workers:
        raise HTTPException(status_code=404, detail="Serial port not connected")
        
    worker = active_workers[port]
    worker.send_text(command)
    return {"status": "success"}

@app.websocket("/ws/events")
async def websocket_endpoint(websocket: WebSocket, port: str = Query(...)):
    await websocket.accept()
    
    if port not in active_workers:
        worker = AsyncSerialWorker(port)
        worker.start()
        active_workers[port] = worker
    else:
        worker = active_workers[port]
        
    worker.subscribe(websocket)
    
    # Send current state hydration instantly
    await websocket.send_json({
        "type": "hydration",
        "state": worker.get_cached_state()
    })
    
    try:
        while True:
            # Keep socket alive and listen for browser client commands
            data = await websocket.receive_text()
            try:
                js = json.loads(data)
                if js.get("type") == "cli":
                    worker.send_text(js.get("command", ""))
                elif js.get("type") == "gpio":
                    # Parse command
                    pin = js.get("pin")
                    cmd = js.get("cmd")
                    val = js.get("value", 0)
                    payload = bytearray([cmd, pin, 0, 1])
                    payload.append(val)
                    payload.extend([0]*7)
                    worker.send_cmd(0xD1, bytes(payload))
            except json.JSONDecodeError:
                pass
    except WebSocketDisconnect:
        pass
    finally:
        worker.unsubscribe(websocket)
        # If no more tabs are viewing this port, we can keep the worker or clean it up.
        # Keeping it ensures that serial reading and logging continues backgrounded.

# Serve frontend dashboard static files directly from root URL
from fastapi.staticfiles import StaticFiles
from fastapi.responses import FileResponse

frontend_dir = os.path.abspath(os.path.join(os.path.dirname(__file__), "../frontend"))
if os.path.exists(frontend_dir):
    @app.get("/")
    def get_index():
        return FileResponse(os.path.join(frontend_dir, "index.html"))

    @app.get("/index.css")
    def get_css():
        return FileResponse(os.path.join(frontend_dir, "index.css"))

    @app.get("/app.js")
    def get_js():
        return FileResponse(os.path.join(frontend_dir, "app.js"))
