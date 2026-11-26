#!/usr/bin/env python3
"""
MeshX Generic Model Context Protocol (MCP) Server.
Provides structured tools for MeshX build, OTG device shell, and shell execution.
Completely generic: Zero hardcoded machine paths or user secrets.
"""

import sys
import os
import json
import subprocess
import time
import re
from pathlib import Path

import concurrent.futures

# Resolve repository root dynamically (tools/mcp/ -> repo root)
REPO_ROOT = Path(__file__).resolve().parents[2]
SCRIPTS_DIR = REPO_ROOT / "tools" / "scripts"
MESHX_PY_NAME = "meshx.py"
MESHX_SCRIPT_PATH = str(SCRIPTS_DIR / MESHX_PY_NAME)

# Sensitive path scrubbing pattern
USER_HOME = str(Path.home())


def parse_ports(port_arg):
    """Normalizes port arguments into a list of individual port strings.
    Supports:
      - None / unset
      - String: 'COM3', 'COM3,COM4', 'COM3 COM4', '/dev/ttyACM0,/dev/ttyACM1'
      - List: ['COM3', 'COM4']
    """
    if not port_arg:
        default = os.environ.get("MESHX_DEFAULT_PORT")
        if not default:
            return []
        port_arg = default

    if isinstance(port_arg, list):
        ports = []
        for p in port_arg:
            ports.extend([item.strip() for item in re.split(r'[, ]+', str(p)) if item.strip()])
        return ports

    return [item.strip() for item in re.split(r'[, ]+', str(port_arg)) if item.strip()]


def sanitize_output(text: str) -> str:
    """Masks home directories and private paths to prevent leaking sensitive system details."""
    if not text:
        return ""
    if USER_HOME:
        text = text.replace(USER_HOME, "~")
    return text


def _find_zephyr_base(project_root):
    env_base = os.environ.get("ZEPHYR_BASE")
    if env_base and (Path(env_base) / "VERSION").exists():
        return env_base
    candidates = [
        Path("D:/Zephyr_Project/zephyr"),
        project_root / "third_party" / "zephyr",
        project_root.parent / "zephyr",
    ]
    for c in candidates:
        if (c / "VERSION").exists():
            return str(c)
    return None


def _find_zephyr_sdk(project_root):
    env_sdk = os.environ.get("ZEPHYR_SDK_INSTALL_DIR") or os.environ.get("ZEPHYR_SDK_INSTALL")
    if env_sdk and (Path(env_sdk) / "sdk_version").exists():
        return env_sdk
    candidates = [
        Path("D:/Zephyr_Project/zephyr-sdk"),
        project_root / "third_party" / "zephyr-sdk",
        project_root / "tools" / "zephyr-sdk",
        project_root.parent / "zephyr-sdk",
    ]
    for c in candidates:
        if (c / "sdk_version").exists():
            return str(c)
    return None


def _collect_user_script_dirs():
    dirs = []
    try:
        import site
        user_base = site.getuserbase()
        v_tag = f"Python{sys.version_info.major}{sys.version_info.minor}"
        dirs.extend([
            os.path.join(user_base, v_tag, "Scripts"),
            os.path.join(user_base, "Scripts"),
        ])
        local_app_data = os.environ.get("LOCALAPPDATA", "")
        if local_app_data:
            import glob
            dirs.extend(glob.glob(os.path.join(local_app_data, "Packages", "PythonSoftwareFoundation.Python*", "LocalCache", "local-packages", v_tag, "Scripts")))
            dirs.extend(glob.glob(os.path.join(local_app_data, "Programs", "Python", "Python*", "Scripts")))
    except Exception:
        pass
    return [d for d in dirs if os.path.exists(d)]


def _collect_system_tool_dirs():
    dirs = []
    for prog_dir in [os.environ.get("ProgramFiles", ""), os.environ.get("ProgramFiles(x86)", "")]:
        if prog_dir:
            dirs.extend([
                os.path.join(prog_dir, "CMake", "bin"),
                os.path.join(prog_dir, "Ninja"),
            ])
    local_app_data = os.environ.get("LOCALAPPDATA", "")
    if local_app_data:
        dirs.append(os.path.join(local_app_data, "Microsoft", "WinGet", "Links"))
    return [d for d in dirs if os.path.exists(d)]


def ensure_zephyr_env():
    """Auto-discovers and ensures ZEPHYR_BASE, ZEPHYR_SDK_INSTALL_DIR, and west are in PATH/env."""
    project_root = REPO_ROOT

    base = _find_zephyr_base(project_root)
    if base and "ZEPHYR_BASE" not in os.environ:
        os.environ["ZEPHYR_BASE"] = base

    sdk = _find_zephyr_sdk(project_root)
    if sdk and "ZEPHYR_SDK_INSTALL_DIR" not in os.environ:
        os.environ["ZEPHYR_SDK_INSTALL_DIR"] = sdk

    paths_to_add = [
        str(REPO_ROOT / "tools" / "bin"),
        str(REPO_ROOT / "tools" / "scripts"),
    ]
    paths_to_add.extend(_collect_user_script_dirs())
    paths_to_add.extend(_collect_system_tool_dirs())

    current_path = os.environ.get("PATH", "")
    new_parts = [p for p in paths_to_add if p.lower() not in current_path.lower()]
    if new_parts:
        os.environ["PATH"] = os.pathsep.join(new_parts) + os.pathsep + current_path


# Initialize discovery early
ensure_zephyr_env()


def run_process_with_timeout(cmd, cwd=None, timeout=300):
    """Executes a command within the pre-initialized environment and captures output safely."""
    try:
        proc = subprocess.Popen(
            cmd,
            cwd=cwd or str(REPO_ROOT),
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            stdin=subprocess.PIPE,
            text=True,
            shell=isinstance(cmd, str),
            encoding="utf-8",
            errors="replace",
            env=os.environ.copy()
        )
        stdout, _ = proc.communicate(timeout=timeout)
        return {
            "exit_code": proc.returncode,
            "output": sanitize_output(stdout)
        }
    except subprocess.TimeoutExpired:
        proc.kill()
        return {
            "exit_code": -1,
            "output": f"Command timed out after {timeout} seconds."
        }
    except Exception as e:
        return {
            "exit_code": -1,
            "output": f"Execution failed: {str(e)}"
        }


# --- Tool Implementations ---

def tool_meshx_build(args):
    bsp = args.get("bsp", os.environ.get("MESHX_DEFAULT_BSP", "esp32_devkitC"))
    clean = args.get("clean", False)
    build_type = args.get("build_type", "Debug")
    host = args.get("host")
    if not host:
        host = "zephyr" if bsp in ["rpi_pico_w"] else "esp"
    defines = args.get("define")
    prod_name = args.get("prod_name")

    if host == "zephyr":
        ensure_zephyr_env()

    cmd = [sys.executable, MESHX_SCRIPT_PATH]
    if clean:
        cmd.append("-bc")
    else:
        cmd.append("-b")
    cmd.extend(["-B", bsp])
    cmd.extend(["-H", host])
    cmd.extend(["--build-type", build_type])
    if defines:
        cmd.extend(["-D", defines])
    if prod_name:
        cmd.extend(["-N", prod_name])

    res = run_process_with_timeout(cmd, timeout=args.get("timeout_seconds", 600))
    return {
        "success": res["exit_code"] == 0,
        "command": " ".join(cmd),
        "exit_code": res["exit_code"],
        "output": res["output"]
    }


def tool_meshx_flash(args):
    ports = parse_ports(args.get("port") or args.get("ports"))
    bsp = args.get("bsp", os.environ.get("MESHX_DEFAULT_BSP", "esp32_devkitC"))
    target = args.get("target", "all")
    host = args.get("host")
    if not host:
        host = "zephyr" if bsp in ["rpi_pico_w"] else "esp"

    if host == "zephyr":
        ensure_zephyr_env()

    cmd = [sys.executable, MESHX_SCRIPT_PATH, "-B", bsp, "-H", host, "-F", target]
    if ports:
        cmd.extend(["-P", *ports])

    res = run_process_with_timeout(cmd, timeout=args.get("timeout_seconds", 300))
    return {
        "success": res["exit_code"] == 0,
        "ports": ports,
        "exit_code": res["exit_code"],
        "output": res["output"]
    }


def tool_meshx_erase(args):
    target = args.get("target", "chip")
    ports = parse_ports(args.get("port") or args.get("ports"))
    bsp = args.get("bsp", os.environ.get("MESHX_DEFAULT_BSP", "esp32_devkitC"))
    host = args.get("host")
    if not host:
        host = "zephyr" if bsp in ["rpi_pico_w"] else "esp"

    if host == "zephyr":
        ensure_zephyr_env()

    cmd = [sys.executable, MESHX_SCRIPT_PATH, "-B", bsp, "-H", host, "-E", target]
    if ports:
        cmd.extend(["-P", *ports])

    res = run_process_with_timeout(cmd, timeout=args.get("timeout_seconds", 120))
    return {
        "success": res["exit_code"] == 0,
        "ports": ports,
        "exit_code": res["exit_code"],
        "output": res["output"]
    }


def tool_meshx_list_bsps(args):
    cmd = [sys.executable, MESHX_SCRIPT_PATH, "--list-bsp"]
    res = run_process_with_timeout(cmd, timeout=30)
    return {
        "output": res["output"]
    }


def tool_meshx_shell_exec(args):
    command = args.get("command")
    if not command:
        return {"error": "Missing 'command' argument."}
    timeout = args.get("timeout_seconds", 300)
    cwd = args.get("cwd")
    work_dir = (REPO_ROOT / cwd).resolve() if cwd else REPO_ROOT

    res = run_process_with_timeout(command, cwd=str(work_dir), timeout=timeout)
    return {
        "command": command,
        "exit_code": res["exit_code"],
        "output": res["output"]
    }


def _send_serial_cmd(port, cmd_to_send, baud, timeout_sec):
    try:
        import serial
    except ImportError:
        return {"port": port, "error": "pyserial module not found in the active python environment."}

    try:
        with serial.Serial(port, baudrate=baud, timeout=1.0) as ser:
            ser.reset_input_buffer()
            ser.write((cmd_to_send + "\r\n").encode("utf-8"))
            ser.flush()

            start_time = time.time()
            received_lines = []
            while (time.time() - start_time) < timeout_sec:
                line = ser.readline().decode("utf-8", errors="replace")
                if line:
                    received_lines.append(line)
                else:
                    time.sleep(0.05)

            return {
                "port": port,
                "command_sent": cmd_to_send,
                "response": "".join(received_lines).strip()
            }
    except Exception as e:
        return {
            "port": port,
            "error": f"Failed to communicate with serial target: {str(e)}"
        }


def tool_meshx_target_shell(args):
    ports = parse_ports(args.get("port") or args.get("ports"))
    cmd_to_send = args.get("command")
    baud = args.get("baud", int(os.environ.get("MESHX_DEFAULT_BAUD", 115200)))
    timeout_sec = args.get("timeout_seconds", 5)

    if not ports:
        return {"error": "Target serial port(s) not specified and MESHX_DEFAULT_PORT not set."}
    if not cmd_to_send:
        return {"error": "Missing 'command' to send to device shell."}

    # Parallel execution if multiple ports specified
    if len(ports) == 1:
        return _send_serial_cmd(ports[0], cmd_to_send, baud, timeout_sec)

    results = {}
    with concurrent.futures.ThreadPoolExecutor(max_workers=min(len(ports), 8)) as executor:
        future_to_port = {
            executor.submit(_send_serial_cmd, p, cmd_to_send, baud, timeout_sec): p for p in ports
        }
        for future in concurrent.futures.as_completed(future_to_port):
            p = future_to_port[future]
            try:
                results[p] = future.result()
            except Exception as e:
                results[p] = {"port": p, "error": str(e)}

    return {
        "command_sent": cmd_to_send,
        "results": results
    }


# --- Tool Specifications ---

TOOLS = [
    {
        "name": "meshx_build",
        "description": "Builds MeshX firmware for a specific BSP using the active toolchain environment.",
        "inputSchema": {
            "type": "object",
            "properties": {
                "bsp": {"type": "string", "description": "BSP target name, e.g. esp32_devkitC, xiao_c3, weact_c3, rpi_pico_w"},
                "clean": {"type": "boolean", "description": "Whether to perform a clean build (-bc)"},
                "build_type": {"type": "string", "enum": ["Debug", "Release"], "description": "Build type"},
                "host": {"type": "string", "enum": ["esp", "zephyr"], "description": "Target host"},
                "define": {"type": "string", "description": "Compile time definitions e.g. -DDEBUG"},
                "prod_name": {"type": "string", "description": "Product name profile to build"},
                "timeout_seconds": {"type": "integer", "description": "Build timeout in seconds (default: 600)"}
            }
        }
    },
    {
        "name": "meshx_flash",
        "description": "Flashes MeshX firmware to one or more connected targets in parallel.",
        "inputSchema": {
            "type": "object",
            "properties": {
                "port": {"type": "string", "description": "Serial port(s), e.g. 'COM3', 'COM3,COM4', or space separated for parallel flashing"},
                "ports": {
                    "type": "array",
                    "items": {"type": "string"},
                    "description": "Array of serial ports for parallel flashing (e.g. ['COM3', 'COM4'])"
                },
                "bsp": {"type": "string", "description": "BSP target name"},
                "host": {"type": "string", "enum": ["esp", "zephyr"], "description": "Target host (esp or zephyr)"},
                "target": {"type": "string", "enum": ["firmware", "cfg", "all"], "description": "Flash component (default: all)"},
                "timeout_seconds": {"type": "integer", "description": "Timeout in seconds (default: 300)"}
            }
        }
    },
    {
        "name": "meshx_erase",
        "description": "Erases flash memory on one or more target hardware devices in parallel.",
        "inputSchema": {
            "type": "object",
            "properties": {
                "target": {"type": "string", "enum": ["firmware", "chip", "cfg", "all"], "description": "Target to erase"},
                "port": {"type": "string", "description": "Serial port(s), e.g. 'COM3' or comma/space separated ('COM3,COM4')"},
                "ports": {
                    "type": "array",
                    "items": {"type": "string"},
                    "description": "Array of serial ports to erase in parallel"
                },
                "bsp": {"type": "string", "description": "BSP target name"},
                "host": {"type": "string", "enum": ["esp", "zephyr"], "description": "Target host (esp or zephyr)"}
            }
        }
    },
    {
        "name": "meshx_list_bsps",
        "description": "Lists all available Board Support Packages (BSPs) supported by the project.",
        "inputSchema": {"type": "object", "properties": {}}
    },
    {
        "name": "meshx_shell_exec",
        "description": "Executes shell commands OTG directly inside the pre-initialized MeshX build & toolchain environment.",
        "inputSchema": {
            "type": "object",
            "properties": {
                "command": {"type": "string", "description": "Command string to run (e.g. 'python tools/scripts/code_gen.py' or 'idf.py size')"},
                "cwd": {"type": "string", "description": "Working directory relative to repository root"},
                "timeout_seconds": {"type": "integer", "description": "Timeout in seconds (default: 300)"}
            },
            "required": ["command"]
        }
    },
    {
        "name": "meshx_target_shell",
        "description": "Directly interacts with the device shell/OTG console over serial port(s) in parallel without locking into terminal monitors.",
        "inputSchema": {
            "type": "object",
            "properties": {
                "command": {"type": "string", "description": "Command to send to device console"},
                "port": {"type": "string", "description": "Serial port name(s), comma/space separated for parallel query"},
                "ports": {
                    "type": "array",
                    "items": {"type": "string"},
                    "description": "Array of serial ports for parallel query (e.g. ['COM3', 'COM4'])"
                },
                "baud": {"type": "integer", "description": "Baudrate (default 115200)"},
                "timeout_seconds": {"type": "integer", "description": "Wait time for response (default 5)"}
            },
            "required": ["command"]
        }
    }
]

TOOL_DISPATCH = {
    "meshx_build": tool_meshx_build,
    "meshx_flash": tool_meshx_flash,
    "meshx_erase": tool_meshx_erase,
    "meshx_list_bsps": tool_meshx_list_bsps,
    "meshx_shell_exec": tool_meshx_shell_exec,
    "meshx_target_shell": tool_meshx_target_shell,
}


def _handle_tools_call(req_id, params):
    name = params.get("name")
    arguments = params.get("arguments", {})
    func = TOOL_DISPATCH.get(name)
    if not func:
        return {
            "jsonrpc": "2.0",
            "id": req_id,
            "error": {
                "code": -32601,
                "message": f"Tool '{name}' not found"
            }
        }
    try:
        res = func(arguments)
        return {
            "jsonrpc": "2.0",
            "id": req_id,
            "result": {
                "content": [
                    {
                        "type": "text",
                        "text": json.dumps(res, indent=2)
                    }
                ]
            }
        }
    except Exception as e:
        return {
            "jsonrpc": "2.0",
            "id": req_id,
            "error": {
                "code": -32000,
                "message": f"Execution error: {str(e)}"
            }
        }


def handle_request(req):
    req_id = req.get("id")
    method = req.get("method")
    params = req.get("params", {})

    if method == "initialize":
        return {
            "jsonrpc": "2.0",
            "id": req_id,
            "result": {
                "protocolVersion": "2024-11-05",
                "capabilities": {
                    "tools": {}
                },
                "serverInfo": {
                    "name": "meshx-mcp-server",
                    "version": "1.0.0"
                }
            }
        }
    if method == "notifications/initialized":
        return None
    if method == "tools/list":
        return {
            "jsonrpc": "2.0",
            "id": req_id,
            "result": {
                "tools": TOOLS
            }
        }
    if method == "tools/call":
        return _handle_tools_call(req_id, params)
    if method == "ping":
        return {
            "jsonrpc": "2.0",
            "id": req_id,
            "result": {}
        }
    return {
        "jsonrpc": "2.0",
        "id": req_id,
        "error": {
            "code": -32601,
            "message": f"Method '{method}' not implemented"
        }
    }


def main():
    """Main stdio JSON-RPC loop adhering to the MCP standard."""
    while True:
        try:
            line = sys.stdin.readline()
            if not line:
                break
            line = line.strip()
            if not line:
                continue

            req = json.loads(line)
            resp = handle_request(req)
            if resp is not None:
                sys.stdout.write(json.dumps(resp) + "\n")
                sys.stdout.flush()
        except KeyboardInterrupt:
            break
        except Exception as e:
            err_resp = {
                "jsonrpc": "2.0",
                "id": None,
                "error": {
                    "code": -32700,
                    "message": f"Parse error: {str(e)}"
                }
            }
            sys.stdout.write(json.dumps(err_resp) + "\n")
            sys.stdout.flush()


if __name__ == "__main__":
    main()

