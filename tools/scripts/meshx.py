#!/usr/bin/env python3
"""
    Copyright © 2024 - 2025 MeshX
"""

import os
import sys
import yaml
import shutil
import argparse
import subprocess
import re
from abc import ABC, abstractmethod

VERSION_STRING = "v0.2.0"

BUILD_TYPE = ["Debug", "Release"]
BSP_LIST = []
BSP_PATH = "port/bsp"
PLAT_LIST = []
PLAT_PATH = "port/platform"

def check_tool_installed(tool_name):
    """Checks if a tool is installed and present in the PATH."""
    result = shutil.which(tool_name)
    if result is None:
        raise EnvironmentError(f"{tool_name} is not installed or not found in PATH.")

def check_python_tool_installed(tools, import_module=None, auto_install=False):
    """
    Checks if Python module(s)/package(s) are installed.
    Accepts either:
      - A list of dicts: [{'package': '...', 'import_module': '...', 'auto_install': True/False}, ...]
      - A single package name string with optional import_module and auto_install.
    """
    if isinstance(tools, str):
        tool_list = [{"package": tools, "import_module": import_module, "auto_install": auto_install}]
    else:
        tool_list = tools

    for item in tool_list:
        pkg = item.get("package")
        mod = item.get("import_module") or pkg
        should_install = item.get("auto_install", False)

        try:
            __import__(mod)
        except ImportError:
            if should_install:
                print(f"Installing missing python dependency: {pkg}...")
                pip_cmd = [sys.executable, "-m", "pip", "install", pkg]
                try:
                    subprocess.check_call(pip_cmd)
                    __import__(mod)
                    continue
                except subprocess.CalledProcessError:
                    try:
                        subprocess.check_call(pip_cmd + ["--break-system-packages"])
                        __import__(mod)
                        continue
                    except subprocess.CalledProcessError as e:
                        print(f"Warning: Failed to auto-install {pkg}: {e}")
            raise EnvironmentError(f"Python package '{pkg}' (module: '{mod}') is not installed.")

class InvalidProductNameException(Exception):
    """Exception raised when the product name is invalid."""

    def __init__(self, message="Please provide exactly one product name"):
        self.message = message
        super().__init__(self.message)

class InvalidBuildTypeException(Exception):
    """Exception raised when an invalid build type is provided."""

    def __init__(self, message="Invalid build type. Please choose from Debug or Release"):
        self.message = message
        super().__init__(self.message)

class InvalidBspException(Exception):
    """Exception raised when an invalid BSP is provided."""

    def __init__(self, message="Invalid BSP. Please choose from the available BSP list"):
        self.message = message
        super().__init__(self.message)

class NoBspPathException(Exception):
    """Exception raised when no BSP path is provided."""

    def __init__(self, message="Please provide a BSP path"):
        self.message = message
        super().__init__(self.message)

class TargetFlashException(Exception):
    """Exception raised when a target is not found."""

    def __init__(self, message="Target not found"):
        self.message = message
        super().__init__(self.message)

class Target(ABC):
    def __init__(self, sdk, port, baud=115200):
        """
        Initializes a Target object.

        Args:
            sdk (str): The software development kit for the target.
            port (str): The serial port to use for communication with the target.
            baud (int): The baud rate for the serial port.
        """
        self.sdk = sdk
        self.port = port
        self.baud = baud
        self.toolcheck()

    def __str__(self):
        return f"SDK: {self.sdk}, Port: {self.port}"

    def proc_run(self, cmd, cwd=None):
        result = subprocess.run(cmd, check=True, cwd=cwd)
        if result.returncode != 0:
            raise TargetFlashException("Target Error")

    def validate_port(self, port):
        if port is None:
            raise TargetFlashException("Please provide a port")
        # On Windows, COM ports (e.g. COM1, COM3, \\.\COM10) are character devices, not regular files
        if not port.upper().startswith("COM") and not port.startswith(r"\\.\COM") and not os.path.exists(port):
            raise TargetFlashException(f"Port {port} does not exist")
        print(f"Using port: {port}")

    @abstractmethod
    def toolcheck(self):
        """Abstract method to perform tool checks on the target."""

    @abstractmethod
    def configure(self, build_root, args=None):
        """Abstract method to configure the target."""

    @abstractmethod
    def flash(self, args, build_root):
        """Abstract method to flash the target."""

    @abstractmethod
    def erase(self, args, build_root):
        """Abstract method to erase the target."""

    def run(self, args, build_root="build"):
        if len(args.prod_name) != 1:
            raise InvalidProductNameException()

        build_dir = os.path.join(build_root, args.prod_name[0])

        if args.decode:
            # Resolve ELF path
            elf_candidates = [
                os.path.abspath(os.path.join(build_dir, f"meshx_build_{args.bsp}.elf")),
                os.path.abspath(os.path.join(build_dir, "zephyr", "zephyr.elf")),
                os.path.abspath(os.path.join(build_dir, "zephyr.elf")),
            ]
            elf_path = None
            for cand in elf_candidates:
                if os.path.exists(cand):
                    elf_path = cand
                    break
            if not elf_path:
                elf_path = elf_candidates[0]

            # Call host_decoder
            decoder_script = os.path.join(os.path.dirname(__file__), "host_decoder.py")
            cmd = [
                sys.executable, decoder_script,
                "--elf", elf_path,
                "--port", args.port,
                "--baud", str(self.baud)
            ]
            # Use subprocess execution on Windows because os.exec* has known bugs with spaces in sys.executable path
            if os.name != "nt" and hasattr(os, "execvp"):
                os.execvp(sys.executable, cmd)
            else:
                self.proc_run(cmd, cwd=build_dir)
        else:
            self.run_fallback(args, build_dir)

    @abstractmethod
    def run_fallback(self, args, build_dir):
        """
        Abstract method to run the target-specific monitor or fallback tool.
        """

    @abstractmethod
    def reset(self, args, build_root):
        """
        Abstract method to hard-reset the target.
        """

    @abstractmethod
    def flash_cfg(self, args, build_root):
        """
        Abstract method to flash configuration partition.
        """

    @abstractmethod
    def erase_cfg(self, args, build_root):
        """
        Abstract method to erase configuration partition.
        """

class ESPTarget(Target):
    def __init__(self, port, baud=115200):
        """
        Initialize the ESPTarget class.

        Args:
            port (str): The serial port associated with the ESP target.
            baud (int): The baud rate for the serial port.
        """
        super().__init__("esp", port, baud)

    def toolcheck(self):
        print("Checking ESP tools...")
        tools = [
            {"package": "protobuf", "import_module": "google.protobuf", "auto_install": True},
            {"package": "grpcio-tools", "import_module": "grpc_tools.protoc", "auto_install": True},
            {"package": "pyyaml", "import_module": "yaml", "auto_install": True},
            {"package": "esptool"},
            {"package": "esp_idf_monitor"},
        ]
        check_python_tool_installed(tools)

    def configure(self, build_root, args=None):
        if len(args.prod_name) != 1:
            raise InvalidProductNameException()

        build_dir = os.path.join(build_root, args.prod_name[0])
        subprocess.run(["ninja", "-C", build_dir, "menuconfig"], check=True)

    def reset(self, args, build_root):
        self.validate_port(args.port)
        print(f"Hard-resetting ESP target on port: {args.port}")
        # We use esptool's ability to trigger a reset sequence
        cmd = [sys.executable, "-m", "esptool", "--port", args.port, "--before", "default_reset", "--after", "hard_reset", "read_mac"]
        subprocess.run(cmd, check=True)

    def flash(self, args, build_root = "build"):
        if len(args.prod_name) != 1:
            raise InvalidProductNameException()
        self.validate_port(args.port)

        build_dir = os.path.join(build_root, args.prod_name[0])
        esp_tool_cmd = [
            sys.executable, "-m", "esptool",
            "--chip", "auto",
            "--port", args.port,
            "--before", "default_reset",
            "--after", "hard_reset",
            "write_flash", "@flash_args",
        ]
        self.proc_run(esp_tool_cmd, cwd=build_dir)

    def erase(self, args, build_root = "build"):
        if len(args.prod_name) != 1:
            raise InvalidProductNameException()
        self.validate_port(args.port)

        build_dir = os.path.join(build_root, args.prod_name[0])
        esp_tool_cmd = [
            sys.executable, "-m", "esptool",
            "--chip", "auto",
            "--port", args.port,
            "--before", "default_reset",
            "--after", "hard_reset",
            "erase_flash",
        ]
        self.proc_run(esp_tool_cmd, cwd=build_dir)


    def _get_parttool_cmd(self, partition_table, *args):
        """
        Build and execute a parttool command robustly across OS environments.
        Tries:
        1. IDF_PATH/components/partition_table/parttool.py using sys.executable
        2. parttool.py if on PATH via shutil.which
        3. sys.executable -m parttool
        """
        cmd_args = ["--quiet", "--partition-table-file", partition_table] + list(args)

        # 1. Check IDF_PATH environment variable or standard location
        idf_path = os.environ.get("IDF_PATH")
        if idf_path:
            parttool_script = os.path.join(idf_path, "components", "partition_table", "parttool.py")
            if os.path.exists(parttool_script):
                return subprocess.check_output([sys.executable, parttool_script] + cmd_args, text=True).strip()

        # 2. Check if parttool.py is on PATH
        parttool_path = shutil.which("parttool.py")
        if parttool_path:
            return subprocess.check_output([sys.executable, parttool_path] + cmd_args, text=True).strip()

        # 3. Fallback to python -m parttool
        return subprocess.check_output([sys.executable, "-m", "parttool"] + cmd_args, text=True).strip()

    def flash_cfg(self, args, build_root = "build"):
        if len(args.prod_name) != 1:
            raise InvalidProductNameException()
        self.validate_port(args.port)

        build_dir = os.path.join(build_root, args.prod_name[0])

        # Priority: 1. Build directory (generated), 2. Static BSP directory (fallback)
        generated_cfg = os.path.abspath(os.path.join(build_dir, "esp-idf", "main", "meshx_cfg.mxc"))
        static_cfg = os.path.abspath(os.path.join("port", "bsp", args.bsp, "meshx_cfg.mxc"))

        cfg_to_flash = generated_cfg if os.path.exists(generated_cfg) else static_cfg

        if not os.path.exists(cfg_to_flash):
            print(f"Warning: Configuration binary not found at {generated_cfg} or {static_cfg}. Skipping meshx_cfg flash.")
            return

        print(f"Flashing configuration binary: {cfg_to_flash}")

        # Get offset dynamically using parttool.py
        partition_table = os.path.join(build_dir, "partition_table", "partition-table.bin")
        try:
            offset = self._get_parttool_cmd(partition_table, "get_partition_info", "--partition-name", "meshx_cfg", "--info", "offset")
            if not offset:
                raise Exception("Empty offset returned")
        except Exception as e:
            raise TargetFlashException(f"Failed to get meshx_cfg partition offset: {e}. Is ESP-IDF activated or IDF_PATH set?")

        esp_tool_cmd = [
            sys.executable, "-m", "esptool",
            "--chip", "auto",
            "--port", args.port,
            "--before", "default_reset",
            "--after", "hard_reset",
            "write_flash", offset, cfg_to_flash
        ]
        self.proc_run(esp_tool_cmd, cwd=build_dir)

    def erase_cfg(self, args, build_root = "build"):
        self.validate_port(args.port)

        build_dir = os.path.join(build_root, args.prod_name[0])
        partition_table = os.path.join(build_dir, "partition_table", "partition-table.bin")
        try:
            offset = self._get_parttool_cmd(partition_table, "get_partition_info", "--partition-name", "meshx_cfg", "--info", "offset")
            size = self._get_parttool_cmd(partition_table, "get_partition_info", "--partition-name", "meshx_cfg", "--info", "size")
        except Exception as e:
            raise TargetFlashException(f"Failed to get meshx_cfg partition info: {e}. Is ESP-IDF activated or IDF_PATH set?")

        esp_tool_cmd = [
            sys.executable, "-m", "esptool",
            "--chip", "auto",
            "--port", args.port,
            "--before", "default_reset",
            "--after", "hard_reset",
            "erase_region", offset, size
        ]
        self.proc_run(esp_tool_cmd, cwd=build_dir)

    def run_fallback(self, args, build_dir):
        self.validate_port(args.port)
        esp_tool_cmd = [
            sys.executable, "-m", "esp_idf_monitor",
            "--port", args.port,
            "--baud", str(self.baud),
        ]
        self.proc_run(esp_tool_cmd, cwd=build_dir)

class ZephyrTarget(Target):
    def __init__(self, port, baud=115200):
        super().__init__("zephyr", port, baud)

    def toolcheck(self):
        print("Checking Zephyr build tools...")
        check_tool_installed("cmake")
        check_tool_installed("ninja")
        try:
            check_tool_installed("west")
        except EnvironmentError:
            print("Note: 'west' not found in PATH. Ensure Zephyr environment is initialized.")

    def configure(self, build_root, args=None):
        if len(args.prod_name) != 1:
            raise InvalidProductNameException()
        build_dir = os.path.abspath(os.path.join(build_root, args.prod_name[0]))
        cmd = ["west", "build", "-t", "menuconfig", "-d", build_dir]
        subprocess.run(cmd, check=True)

    def _get_flash_runner(self, build_dir, bsp=None):
        runners_yaml_path = os.path.join(build_dir, "zephyr", "runners.yaml")
        if os.path.exists(runners_yaml_path):
            try:
                import yaml
                with open(runners_yaml_path, "r") as f:
                    data = yaml.safe_load(f)
                    if isinstance(data, dict):
                        return data.get("flash-runner")
            except Exception:
                pass
        if bsp in ["weact_c3", "xiao_c3", "esp32_devkitC"]:
            return "esp32"
        return None

    def build_zephyr(self, args, build_dir, prod):
        board_map = {
            "weact_c3": "esp32c3_devkitm",
            "xiao_c3": "esp32c3_devkitm",
            "esp32_devkitC": "esp32_devkitc_wroom",
            "rpi_pico_w": "rpi_pico/rp2040/w",
        }
        board = board_map.get(args.bsp, args.bsp)
        project_root = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
        pristine_mode = "always" if args.clean else "auto"
        cmd = [
            "west", "build",
            "-b", board,
            "-d", build_dir,
            "-p", pristine_mode,
            ".",
            "--",
            "-DMESHX_HOST=zephyr",
            f"-DBSP={args.bsp}",
            f"-DEXTRA_ZEPHYR_MODULES={project_root}",
            f"-DPROD_NAME={prod}",
            f"-DMESHX_BUILD_TYPE={str(args.build_type)}",
            f"-DPROD_PROFILE={args.prod_profile}",
            "-DUSE_CCACHE=0"
        ]
        if args.define:
            cmd.append(f"-DMESHX_EXTRA_DEFS={';'.join(args.define)}")
        print("Running Zephyr build command:", " ".join(cmd))
        subprocess.run(cmd, check=True)

    def flash(self, args, build_root="build"):
        if len(args.prod_name) != 1:
            raise InvalidProductNameException()
        build_dir = os.path.abspath(os.path.join(build_root, args.prod_name[0]))
        pathext = os.environ.get("PATHEXT", "")
        if ".PY" not in pathext.upper().split(";"):
            os.environ["PATHEXT"] = pathext + ";.PY;.py"
        cmd = ["west", "flash", "-d", build_dir]
        runner = self._get_flash_runner(build_dir, args.bsp)
        if runner == "esp32":
            project_root = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
            esptool_path = os.path.abspath(os.path.join(project_root, "modules", "hal", "espressif", "tools", "esptool_py", "esptool.py"))
            if os.path.exists(esptool_path):
                cmd.extend(["--esp-tool", esptool_path])
            if args.port:
                os.environ["ESPTOOL_PORT"] = args.port
                cmd.extend(["--esp-device", args.port])
        elif args.port:
            if runner in ["nrfjprog", "pyocd", "jlink"]:
                cmd.extend(["--dev-id", args.port])
            else:
                cmd.extend(["--dev-id", args.port])
        print("Flashing Zephyr target:", " ".join(cmd))
        self.proc_run(cmd)

    def erase(self, args, build_root="build"):
        if len(args.prod_name) != 1:
            raise InvalidProductNameException()
        build_dir = os.path.abspath(os.path.join(build_root, args.prod_name[0]))
        pathext = os.environ.get("PATHEXT", "")
        if ".PY" not in pathext.upper().split(";"):
            os.environ["PATHEXT"] = pathext + ";.PY;.py"
        cmd = ["west", "flash", "--erase", "-d", build_dir]
        runner = self._get_flash_runner(build_dir, args.bsp)
        if runner == "esp32":
            project_root = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
            esptool_path = os.path.abspath(os.path.join(project_root, "modules", "hal", "espressif", "tools", "esptool_py", "esptool.py"))
            if os.path.exists(esptool_path):
                cmd.extend(["--esp-tool", esptool_path])
            if args.port:
                os.environ["ESPTOOL_PORT"] = args.port
                cmd.extend(["--esp-device", args.port])
        elif args.port:
            if runner in ["nrfjprog", "pyocd", "jlink"]:
                cmd.extend(["--dev-id", args.port])
            else:
                cmd.extend(["--dev-id", args.port])
        print("Erasing Zephyr target flash:", " ".join(cmd))
        self.proc_run(cmd)

    def reset(self, args, build_root="build"):
        if len(args.prod_name) != 1:
            raise InvalidProductNameException()
        build_dir = os.path.abspath(os.path.join(build_root, args.prod_name[0]))
        runner = self._get_flash_runner(build_dir, args.bsp)
        if runner == "esp32" and args.port:
            print(f"Hard-resetting ESP target on port: {args.port}")
            cmd = [sys.executable, "-m", "esptool", "--port", args.port, "--before", "default_reset", "--after", "hard_reset", "read_mac"]
            subprocess.run(cmd, check=True)
            return
        elif runner == "nrfjprog":
            cmd = ["west", "flash", "--runner", "nrfjprog", "--reset", "-d", build_dir]
        else:
            cmd = ["west", "flash", "--reset", "-d", build_dir]
        print("Resetting Zephyr target:", " ".join(cmd))
        self.proc_run(cmd)

    def _get_partition_info(self, build_dir, label="meshx_cfg_partition"):
        dts_path = os.path.join(build_dir, "zephyr", "zephyr.dts")
        if os.path.exists(dts_path):
            with open(dts_path, "r") as f:
                content = f.read()
            m = re.search(r'meshx_cfg_partition:\s*partition@([0-9a-fA-F]+)\s*\{[^}]*reg\s*=\s*<\s*(0x[0-9a-fA-F]+|\d+)\s*(0x[0-9a-fA-F]+|\d+)', content, re.DOTALL)
            if m:
                return int(m.group(2), 0), int(m.group(3), 0)
            m2 = re.search(r'partition@([0-9a-fA-F]+)\s*\{[^}]*label\s*=\s*"meshx-cfg"[^}]*reg\s*=\s*<\s*(0x[0-9a-fA-F]+|\d+)\s*(0x[0-9a-fA-F]+|\d+)', content, re.DOTALL)
            if m2:
                return int(m2.group(2), 0), int(m2.group(3), 0)
        return None, None

    def flash_cfg(self, args, build_root="build"):
        if len(args.prod_name) != 1:
            raise InvalidProductNameException()
        self.validate_port(args.port)

        build_dir = os.path.abspath(os.path.join(build_root, args.prod_name[0]))
        generated_cfg = os.path.abspath(os.path.join(build_dir, "main", "meshx_cfg.mxc"))
        static_cfg = os.path.abspath(os.path.join("port", "bsp", args.bsp, "meshx_cfg.mxc"))
        cfg_to_flash = generated_cfg if os.path.exists(generated_cfg) else static_cfg

        if not os.path.exists(cfg_to_flash):
            print(f"Warning: Configuration binary not found at {generated_cfg} or {static_cfg}. Skipping meshx_cfg flash.")
            return

        offset, size = self._get_partition_info(build_dir)
        if offset is None:
            print("Warning: Could not determine meshx-cfg partition offset from zephyr.dts. Skipping meshx_cfg flash.")
            return

        runner = self._get_flash_runner(build_dir, args.bsp)
        print(f"Flashing configuration binary ({cfg_to_flash}) to 0x{offset:X}...")

        if runner == "esp32":
            project_root = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
            esptool_path = os.path.abspath(os.path.join(project_root, "modules", "hal", "espressif", "tools", "esptool_py", "esptool.py"))
            esptool_cmd = [sys.executable, esptool_path] if os.path.exists(esptool_path) else [sys.executable, "-m", "esptool"]
            cmd = esptool_cmd + [
                "--chip", "auto",
                "--port", args.port,
                "--baud", str(self.baud),
                "--before", "default_reset",
                "--after", "hard_reset",
                "write_flash", hex(offset), cfg_to_flash
            ]
            self.proc_run(cmd, cwd=build_dir)
        elif runner == "pyocd":
            cmd = ["pyocd", "load", cfg_to_flash, "-a", hex(offset)]
            if args.port:
                cmd.extend(["-u", args.port])
            self.proc_run(cmd, cwd=build_dir)
        else:
            print(f"Custom configuration flashing via runner '{runner}' not directly supported.")

    def erase_cfg(self, args, build_root="build"):
        self.validate_port(args.port)
        build_dir = os.path.abspath(os.path.join(build_root, args.prod_name[0]))
        offset, size = self._get_partition_info(build_dir)
        if offset is None or size is None:
            print("Warning: Could not determine meshx-cfg partition offset/size from zephyr.dts.")
            return

        runner = self._get_flash_runner(build_dir, args.bsp)
        print(f"Erasing configuration partition at 0x{offset:X} (size 0x{size:X})...")
        if runner == "esp32":
            project_root = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
            esptool_path = os.path.abspath(os.path.join(project_root, "modules", "hal", "espressif", "tools", "esptool_py", "esptool.py"))
            esptool_cmd = [sys.executable, esptool_path] if os.path.exists(esptool_path) else [sys.executable, "-m", "esptool"]
            cmd = esptool_cmd + [
                "--chip", "auto",
                "--port", args.port,
                "--baud", str(self.baud),
                "erase_region", hex(offset), hex(size)
            ]
            self.proc_run(cmd, cwd=build_dir)
        else:
            print(f"Configuration partition erase via runner '{runner}' is handled by whole-chip erase (--erase).")

    def run_fallback(self, args, build_dir):
        if not args.port:
            raise TargetFlashException("Please provide a port for serial monitoring")
        if args.bsp in ["weact_c3", "xiao_c3", "esp32_devkitC"]:
            try:
                import esp_idf_monitor
                cmd = [sys.executable, "-m", "esp_idf_monitor", "--port", args.port, "--baud", str(self.baud)]
                self.proc_run(cmd, cwd=build_dir)
                return
            except ImportError:
                pass
        cmd = [sys.executable, "-m", "serial.tools.miniterm", args.port, str(self.baud)]
        self.proc_run(cmd, cwd=build_dir)

NRFTarget = ZephyrTarget

def clean(args, build_root = "build"):
    """Clean the build directory by removing all build artifacts."""
    for prod in args.prod_name:
        build_dir = os.path.join(build_root, prod)
        if os.path.exists(build_dir):
            print(f"Cleaning build directory: {build_dir}")
            shutil.rmtree(build_dir, ignore_errors=True)

def build(args, build_root = "build"):
    """Build the expected target and product."""
    if args.host == "zephyr":
        target = ZephyrTarget(args.port, args.baud)
        for prod in args.prod_name:
            build_dir = os.path.join(build_root, prod)
            os.makedirs(build_dir, exist_ok=True)
            target.build_zephyr(args, build_dir, prod)
        return

    for prod in args.prod_name:
        # Delete any existing sdkconfig file
        sdkconfig_path = "sdkconfig"
        if os.path.exists(sdkconfig_path):
            os.remove(sdkconfig_path)

        build_dir = os.path.join(build_root, prod)

        os.makedirs(build_dir, exist_ok=True)
        cmake_command = [
            "cmake",
            "-S", ".", "-B", build_dir, "-G", "Ninja",
            "-DCCACHE_ENABLE=1",
            f"-DBSP={args.bsp}",
            f"-DPROD_NAME={prod}",
            f"-DMESHX_BUILD_TYPE={args.build_type}",
            f"-DPROD_PROFILE={args.prod_profile}",
            f"-DELF='meshx_build_{args.bsp}'"
        ]

        uvp_defs = []
        if args.define:
            uvp_defs.extend(args.define)

        cmake_command.append(f"-DMESHX_EXTRA_DEFS={';'.join(uvp_defs)}")

        print("Running CMake command:", " ".join(cmake_command))
        # run cmake command
        subprocess.run(cmake_command, check=True)
        # run ninja build from build directory
        build_command = ["ninja", "-C", build_dir]
        subprocess.run(build_command, check=True)

def main():
    for bsp in os.listdir(BSP_PATH):
        if os.path.isdir(os.path.join(BSP_PATH, bsp)):
            if bsp.startswith('.') or not os.path.exists(os.path.join(BSP_PATH, bsp, "bsp.cmake")):
                continue
            BSP_LIST.append(bsp)

    for plat in os.listdir(PLAT_PATH):
        if os.path.isdir(os.path.join(PLAT_PATH, plat)):
            if plat.startswith('.'):
                continue
            PLAT_LIST.append(plat)

    # Parse command-line arguments
    parser = argparse.ArgumentParser(description="MeshX build and target management tool.")

    parser.add_argument('-v', '--version', action="store_true", help="Get the version details of meshx.py")
    parser.add_argument('-m', '--meshx-args', type=str, help="Directory path to meshx.args (Optional)")

    build_group = parser.add_argument_group("Build Options")
    build_group.add_argument('-b', '--build', action="store_true", help="Build respective BSP.")
    build_group.add_argument('-c', '--clean', action="store_true", help="Clean the build directory before building.")
    build_group.add_argument('--list-bsp', action="store_true", help="List available BSPs and exit.")

    build_config_group = parser.add_argument_group("Build Configuration")
    build_config_group.add_argument('-B', '--bsp', choices=BSP_LIST, default=BSP_LIST[0] if BSP_LIST else None, help=f"Specify the BSP to use. Default is {BSP_LIST[0] if BSP_LIST else 'None'}")
    build_config_group.add_argument('-N', '--prod-name', nargs='+', default=[], help="Specify the product name. Defaults to all Product specified in PROD_PROFILE")
    build_config_group.add_argument('-pp', '--prod-profile', default=None, help="Specify the product profile. Defaults to bsp_path/prod_profile.yml")
    build_config_group.add_argument('-D', '--define', action="append", default=[], help="Add compile-time definitions (e.g., -DDEBUG or -DLEVEL=5)")
    build_config_group.add_argument("--build-type", choices=BUILD_TYPE, default=BUILD_TYPE[0], help="Specify the build type. Default is Debug.")

    target_group = parser.add_argument_group("Target Options")
    target_group.add_argument('-H', '--host', choices=PLAT_LIST, default=PLAT_LIST[0] if PLAT_LIST else None, help=f"Target host. Default is {PLAT_LIST[0] if PLAT_LIST else 'None'}")
    target_group.add_argument('-P', '--port', type=str, default=None, help="Target port")
    target_group.add_argument('-C', '--config', action="store_true", help="target configs")
    target_group.add_argument('-F', '--flash', choices=["firmware", "cfg", "all"], nargs='?', const="all", default=None, help="flash ESP target (default: all)")
    target_group.add_argument('-E', '--erase', choices=["firmware", "chip", "cfg", "all"], default=None, help="Erase target")
    target_group.add_argument('-R', '--run', action="store_true", help="Run target")
    target_group.add_argument("--decode", action="store_true", help="Decode TLV logs from target")
    target_group.add_argument("-HR", "--reset", action="store_true", help="Hard reset target")
    target_group.add_argument("--baud", type=int, default=115200, help="Serial baudrate. Default is 115200")

    args = parser.parse_args()

    if args.version:
        print(f"meshx.py Version: {VERSION_STRING}")
        sys.exit(0)

    if args.build or args.config:
        # Ensure required build tools are installed
        required_tools = ['cmake', 'ninja']
        for tool in required_tools:
            check_tool_installed(tool)

        # Check if submodules are initialized (specifically nanopb)
        nanopb_check_file = os.path.join("third_party", "nanopb", "pb_common.c")
        if not os.path.exists(nanopb_check_file):
            print("Warning: third_party/nanopb is not initialized. Attempting to initialize submodules...")
            try:
                subprocess.run(["git", "submodule", "update", "--init", "--recursive", "third_party/nanopb"], check=True)
                if not os.path.exists(nanopb_check_file):
                    print("Error: Failed to initialize nanopb submodule. Please check your internet connection or run 'git submodule update --init --recursive third_party/nanopb' manually.")
                    sys.exit(1)
                print("Successfully initialized submodules.")
            except (subprocess.CalledProcessError, FileNotFoundError):
                print("Error: 'git submodule' command failed. Please ensure submodules are initialized.")
                sys.exit(1)

    if args.meshx_args:
        parser.set_defaults(**vars(args))
        file_args = os.path.join(args.meshx_args, "meshx.args")
        if not os.path.exists(file_args):
            raise FileNotFoundError(f"meshx.args file not found at {file_args}")
        with open(file_args, 'r') as f:
            args = parser.parse_args(f.read().split())

    if args.list_bsp:
        print("Available BSPs:")
        for bsp in BSP_LIST:
            print(f" - {bsp}")
        sys.exit(0)

    if args.prod_profile is None:
        args.prod_profile = os.path.join("port", "bsp", args.bsp, "prod_profile.yml")

    print(f"meshx.py Version: {VERSION_STRING}")
    print(f"Build Type: {args.build_type}")
    print(f"BSP: {args.bsp}")
    print(f"Product Name: {args.prod_name}")
    print(f"Product Profile: {args.prod_profile}")

    if args.prod_name != []:
        if not os.path.exists(args.prod_profile):
            raise FileNotFoundError(f"Product profile '{args.prod_profile}' not found.")
        with open(args.prod_profile, 'r') as f:
            profile_data = yaml.safe_load(f)
        try:
            available_products = [prod['name'] for prod in profile_data['prod'].get('products', [])]
            for prod in args.prod_name:
                if prod not in available_products:
                    raise ValueError(f"Product name '{prod}' not found in profile '{args.prod_profile}'. Available products: {', '.join(available_products)}")
        except (KeyError, TypeError):
            raise ValueError(f"Product profile '{args.prod_profile}' is not valid.")
    else:
        with open(args.prod_profile, 'r') as f:
            profile_data = yaml.safe_load(f)
        args.prod_name = [prod['name'] for prod in profile_data['prod'].get('products', [])]
        print(f"No product name specified. Using all products from profile: {args.prod_name}")

    print(f"Final Product Names: {args.prod_name}")

    build_root = os.path.join("build", args.bsp, args.build_type)

    target_sel = None
    if args.host == "esp":
        target_sel = ESPTarget(args.port, args.baud)
    elif args.host == "zephyr" or args.host == "nrf":
        target_sel = ZephyrTarget(args.port, args.baud)
    else:
        raise TargetFlashException(f"Unsupported target host: {args.host}")

    if args.config:
        target_sel.configure(build_root, args)

    if args.clean:
        clean(args, build_root)
    if args.build:
        build(args, build_root)
    if args.erase:
        if args.erase in ["firmware", "chip", "all"]:
            target_sel.erase(args, build_root)
        if args.erase in ["cfg", "all"]:
            target_sel.erase_cfg(args, build_root)
    if args.flash:
        if args.flash in ["firmware", "all"]:
            target_sel.flash(args, build_root)
        if args.flash in ["cfg", "all"]:
            target_sel.flash_cfg(args, build_root)
    if args.reset:
        target_sel.reset(args, build_root)
    if args.run:
        target_sel.run(args, build_root)

if __name__ == "__main__":
    try:
        main()
    except KeyboardInterrupt:
        pass
