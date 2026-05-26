import asyncio
import os
import sys
import yaml
import struct
import json
import logging
import serial
import serial.tools.list_ports
from typing import Dict
from fastapi.responses import FileResponse
from fastapi.middleware.cors import CORSMiddleware
from fastapi import FastAPI, WebSocket, WebSocketDisconnect, Query, HTTPException

from serial_worker import AsyncSerialWorker

# Add server and scripts directories to path to import local modules
sys.path.append(os.path.dirname(os.path.abspath(__file__)))

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

# Store references to background tasks to prevent garbage collection
background_tasks = set()
active_workers: Dict[str, AsyncSerialWorker] = {}

@app.get("/api/ports")
def get_available_ports():
    ports = serial.tools.list_ports.comports()
    return [{"port": p.device, "desc": p.description} for p in ports]

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
    serial_worker.current_bsp = bsp
    serial_worker.current_product = product
    if port in active_workers:
        worker = active_workers[port]
        worker.set_active_bsp_and_product(bsp, product)
    return {"status": "success", "message": f"Active BSP set to {bsp} and Profile to {product}"}

@app.post("/api/flash")
async def flash_device(port: str, bsp: str, product: str, erase: bool = False):
    if port not in active_workers:
        raise HTTPException(status_code=404, detail="Serial port not connected")

    worker = active_workers[port]

    serial_worker.current_bsp = bsp
    serial_worker.current_product = product
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

    serial_worker.current_bsp = bsp
    serial_worker.current_product = product
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
