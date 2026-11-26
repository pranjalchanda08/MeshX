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
from copy import copy
from concurrent.futures import ThreadPoolExecutor, as_completed
from abc import ABC, abstractmethod

VERSION_STRING = "v0.2.0"

BUILD_TYPE = ["Debug", "Release"]
BSP_LIST = []
BSP_PATH = "port/bsp"
PLAT_LIST = []
PLAT_PATH = "port/platform"

MESHX_CFG_BIN = "meshx_cfg.mxc"
ESPTOOL_PY = "esptool.py"

def check_tool_installed(tool_name):
    """Checks if a tool is installed and present in the PATH."""
    result = shutil.which(tool_name)
    if result is None:
        raise EnvironmentError(f"{tool_name} is not installed or not found in PATH.")

def _refresh_site_packages():
    import site, importlib
    user_site = site.getusersitepackages()
    if user_site and user_site not in sys.path:
        sys.path.insert(0, user_site)
    for sp in site.getsitepackages():
        if sp not in sys.path:
            sys.path.append(sp)
    importlib.invalidate_caches()

def _try_install_python_pkg(pkg, mod):
    print(f"Installing missing python dependency: {pkg}...")
    pip_cmd = [sys.executable, "-m", "pip", "install", pkg]
    variants = [[], ["--break-system-packages"], ["--user"], ["--user", "--break-system-packages"]]
    for extra_args in variants:
        try:
            subprocess.check_call(pip_cmd + extra_args)
            _refresh_site_packages()
            __import__(mod)
            return True
        except (subprocess.CalledProcessError, ImportError, AttributeError):
            continue
    return False

def check_python_tool_installed(tools, import_module=None, auto_install=False):
    """
    Checks if Python module(s)/package(s) are installed.
    Accepts either:
      - A list of dicts: [{'package': '...', 'import_module': '...', 'auto_install': True/False}, ...]
      - A single package name string with optional import_module and auto_install.
    """
    tool_list = [{"package": tools, "import_module": import_module, "auto_install": auto_install}] if isinstance(tools, str) else tools
    for item in tool_list:
        pkg = item.get("package")
        mod = item.get("import_module") or pkg
        try:
            __import__(mod)
        except ImportError:
            if item.get("auto_install", False) and _try_install_python_pkg(pkg, mod):
                continue
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
        generated_cfg = os.path.abspath(os.path.join(build_dir, "esp-idf", "main", MESHX_CFG_BIN))
        static_cfg = os.path.abspath(os.path.join("port", "bsp", args.bsp, MESHX_CFG_BIN))

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
                raise TargetFlashException("Empty offset returned")
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

    @staticmethod
    def _find_zephyr_base(project_root):
        zephyr_base = os.environ.get("ZEPHYR_BASE")
        if zephyr_base and os.path.exists(os.path.join(zephyr_base, "VERSION")):
            return zephyr_base
        candidates = [
            os.path.join("D:", os.sep, "Zephyr_Project", "zephyr"),
            os.path.join(project_root, "third_party", "zephyr"),
            os.path.abspath(os.path.join(project_root, "..", "zephyr")),
        ]
        for c in candidates:
            if os.path.exists(os.path.join(c, "VERSION")):
                return c
        return None

    @staticmethod
    def _find_zephyr_sdk(project_root):
        sdk_dir = os.environ.get("ZEPHYR_SDK_INSTALL_DIR") or os.environ.get("ZEPHYR_SDK_INSTALL")
        if sdk_dir and os.path.exists(os.path.join(sdk_dir, "sdk_version")):
            return sdk_dir
        candidates = [
            os.path.join("D:", os.sep, "Zephyr_Project", "zephyr-sdk"),
            os.path.join(project_root, "third_party", "zephyr-sdk"),
            os.path.join(project_root, "tools", "zephyr-sdk"),
            os.path.abspath(os.path.join(project_root, "..", "zephyr-sdk")),
        ]
        for c in candidates:
            if os.path.exists(os.path.join(c, "sdk_version")):
                return c
        return None

    @staticmethod
    def _collect_zephyr_tool_paths(project_root):
        paths_to_add = [
            os.path.join(project_root, "tools", "bin"),
            os.path.join(project_root, "tools", "scripts"),
        ]
        try:
            import site, glob
            user_base = site.getuserbase()
            v_tag = f"Python{sys.version_info.major}{sys.version_info.minor}"
            candidate_scripts = [
                os.path.join(user_base, v_tag, "Scripts"),
                os.path.join(user_base, "Scripts"),
            ]
            local_app_data = os.environ.get("LOCALAPPDATA", "")
            if local_app_data:
                candidate_scripts.extend(glob.glob(os.path.join(local_app_data, "Packages", "PythonSoftwareFoundation.Python*", "LocalCache", "local-packages", v_tag, "Scripts")))
                candidate_scripts.extend(glob.glob(os.path.join(local_app_data, "Programs", "Python", "Python*", "Scripts")))
            paths_to_add.extend([s for s in candidate_scripts if os.path.exists(s)])
        except Exception:
            pass

        for prog_dir in [os.environ.get("ProgramFiles", ""), os.environ.get("ProgramFiles(x86)", "")]:
            if prog_dir:
                paths_to_add.extend([os.path.join(prog_dir, "CMake", "bin"), os.path.join(prog_dir, "Ninja")])

        local_app_data = os.environ.get("LOCALAPPDATA", "")
        if local_app_data:
            paths_to_add.append(os.path.join(local_app_data, "Microsoft", "WinGet", "Links"))

        return [p for p in paths_to_add if os.path.exists(p)]

    @staticmethod
    def _ensure_west_config(project_root, cur_zephyr):
        if not (cur_zephyr and os.path.exists(cur_zephyr)):
            return
        west_dir = os.path.join(project_root, ".west")
        config_file = os.path.join(west_dir, "config")
        if not os.path.exists(config_file):
            try:
                os.makedirs(west_dir, exist_ok=True)
                rel = os.path.relpath(cur_zephyr, project_root).replace("\\", "/")
                with open(config_file, "w", encoding="utf-8") as f:
                    f.write(f"[manifest]\npath = {rel}\nfile = west.yml\n\n[zephyr]\nbase = {rel}\n")
            except Exception:
                pass

    @classmethod
    def _ensure_zephyr_env(cls):
        """Auto-discovers and sets up Zephyr environment variables and tool paths if missing."""
        project_root = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))

        base = cls._find_zephyr_base(project_root)
        if base and "ZEPHYR_BASE" not in os.environ:
            os.environ["ZEPHYR_BASE"] = base
            print(f"Auto-discovered ZEPHYR_BASE: {base}")

        sdk = cls._find_zephyr_sdk(project_root)
        if sdk and "ZEPHYR_SDK_INSTALL_DIR" not in os.environ:
            os.environ["ZEPHYR_SDK_INSTALL_DIR"] = sdk
            print(f"Auto-discovered ZEPHYR_SDK_INSTALL_DIR: {sdk}")

        tool_paths = cls._collect_zephyr_tool_paths(project_root)
        current_path = os.environ.get("PATH", "")
        new_parts = [p for p in tool_paths if p.lower() not in current_path.lower()]
        if new_parts:
            os.environ["PATH"] = os.pathsep.join(new_parts) + os.pathsep + current_path

        cls._ensure_west_config(project_root, os.environ.get("ZEPHYR_BASE"))

    def toolcheck(self):
        print("Checking Zephyr build tools...")
        self._ensure_zephyr_env()
        check_tool_installed("cmake")
        check_tool_installed("ninja")
        check_tool_installed("west")

    def configure(self, build_root, args=None):
        if len(args.prod_name) != 1:
            raise InvalidProductNameException()
        build_dir = os.path.abspath(os.path.join(build_root, args.prod_name[0]))
        west_bin = shutil.which("west") or "west"
        cmd = [west_bin, "build", "-t", "menuconfig", "-d", build_dir]
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
        zephyr_major = 3
        zephyr_base = os.environ.get("ZEPHYR_BASE", "")
        version_file = os.path.join(zephyr_base, "VERSION") if zephyr_base else ""
        if version_file and os.path.exists(version_file):
            with open(version_file, "r", encoding="utf-8") as vf:
                for line in vf:
                    if line.startswith("VERSION_MAJOR"):
                        try:
                            zephyr_major = int(line.split("=", 1)[1].strip())
                        except ValueError:
                            pass
                        break

        board_map = {
            "weact_c3": "esp32c3_devkitm",
            "xiao_c3": "esp32c3_devkitm",
            "esp32_devkitC": "esp32_devkitc/esp32/procpu" if zephyr_major >= 4 else "esp32_devkitc_wroom",
            "rpi_pico_w": "rpi_pico/rp2040/w",
        }
        board = board_map.get(args.bsp, args.bsp)
        project_root = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
        pristine_mode = "always" if args.clean else "auto"

        extra_conf_files = []
        bsp_conf = os.path.join(project_root, "port", "bsp", args.bsp, f"{args.bsp}.conf")
        if os.path.exists(bsp_conf):
            extra_conf_files.append(os.path.abspath(bsp_conf))

        # Check MCU-specific conf (e.g. port/platform/zephyr/mcu/esp32/esp32.conf)
        mcu_conf_map = {
            "esp32_devkitC": os.path.join(project_root, "port", "platform", "zephyr", "mcu", "esp32", "esp32.conf"),
            "weact_c3": os.path.join(project_root, "port", "platform", "zephyr", "mcu", "esp32c3", "esp32c3.conf"),
            "xiao_c3": os.path.join(project_root, "port", "platform", "zephyr", "mcu", "esp32c3", "esp32c3.conf"),
            "rpi_pico_w": os.path.join(project_root, "port", "platform", "zephyr", "mcu", "rp2040", "rp2040.conf"),
        }
        mcu_conf = mcu_conf_map.get(args.bsp)
        if mcu_conf and os.path.exists(mcu_conf) and mcu_conf not in extra_conf_files:
            extra_conf_files.append(os.path.abspath(mcu_conf))

        west_bin = shutil.which("west") or "west"
        west_cwd = project_root
        if not os.path.isdir(os.path.join(project_root, ".west")):
            zephyr_base = os.environ.get("ZEPHYR_BASE", "")
            if zephyr_base and os.path.isdir(os.path.join(os.path.dirname(os.path.abspath(zephyr_base)), ".west")):
                west_cwd = os.path.dirname(os.path.abspath(zephyr_base))

        cmd = [
            west_bin, "build",
            "-b", board,
            "-d", os.path.abspath(build_dir),
            "-p", pristine_mode,
            project_root,
            "--",
            "-DMESHX_HOST=zephyr",
            f"-DBSP={args.bsp}",
            f"-DEXTRA_ZEPHYR_MODULES={project_root}",
            f"-DPROD_NAME={prod}",
            f"-DMESHX_BUILD_TYPE={str(args.build_type)}",
            f"-DPROD_PROFILE={os.path.abspath(args.prod_profile)}",
            "-DUSE_CCACHE=0"
        ]
        if extra_conf_files:
            cmd.append(f"-DEXTRA_CONF_FILE={';'.join(extra_conf_files)}")
        if args.define:
            cmd.append(f"-DMESHX_EXTRA_DEFS={';'.join(args.define)}")
        print("Running Zephyr build command:", " ".join(cmd))
        subprocess.run(cmd, cwd=west_cwd, check=True)

    def flash(self, args, build_root="build"):
        if len(args.prod_name) != 1:
            raise InvalidProductNameException()
        build_dir = os.path.abspath(os.path.join(build_root, args.prod_name[0]))
        pathext = os.environ.get("PATHEXT", "")
        if ".PY" not in pathext.upper().split(";"):
            os.environ["PATHEXT"] = pathext + ";.PY;.py"
        west_bin = shutil.which("west") or "west"
        cmd = [west_bin, "flash", "-d", build_dir]
        runner = self._get_flash_runner(build_dir, args.bsp)
        if runner == "esp32":
            project_root = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
            esptool_path = os.path.abspath(os.path.join(project_root, "modules", "hal", "espressif", "tools", "esptool_py", ESPTOOL_PY))
            if os.path.exists(esptool_path):
                cmd.extend(["--esp-tool", esptool_path])
            if args.port:
                os.environ["ESPTOOL_PORT"] = args.port
                cmd.extend(["--esp-device", args.port])
        elif args.port:
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
        west_bin = shutil.which("west") or "west"
        cmd = [west_bin, "flash", "--erase", "-d", build_dir]
        runner = self._get_flash_runner(build_dir, args.bsp)
        if runner == "esp32":
            project_root = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
            esptool_path = os.path.abspath(os.path.join(project_root, "modules", "hal", "espressif", "tools", "esptool_py", ESPTOOL_PY))
            if os.path.exists(esptool_path):
                cmd.extend(["--esp-tool", esptool_path])
            if args.port:
                os.environ["ESPTOOL_PORT"] = args.port
                cmd.extend(["--esp-device", args.port])
        elif args.port:
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

    @staticmethod
    def _parse_dts_reg(block_text):
        m = re.search(r'reg\s*=\s*<\s*(0x[0-9a-fA-F]+|\d+)\s+(0x[0-9a-fA-F]+|\d+)', block_text)
        if m:
            return int(m.group(1), 0), int(m.group(2), 0)
        return None

    def _get_partition_info(self, build_dir):
        dts_path = os.path.join(build_dir, "zephyr", "zephyr.dts")
        if not os.path.exists(dts_path):
            return None, None
        with open(dts_path, "r", encoding="utf-8", errors="replace") as f:
            content = f.read()

        blocks = re.findall(r'partition@[0-9a-fA-F]+\s*\{[^}]+\}', content)
        for block in blocks:
            if "meshx_cfg_partition" in block or '"meshx-cfg"' in block:
                parsed = self._parse_dts_reg(block)
                if parsed:
                    return parsed
        return None, None

    def flash_cfg(self, args, build_root="build"):
        if len(args.prod_name) != 1:
            raise InvalidProductNameException()
        self.validate_port(args.port)

        build_dir = os.path.abspath(os.path.join(build_root, args.prod_name[0]))
        generated_cfg = os.path.abspath(os.path.join(build_dir, "main", MESHX_CFG_BIN))
        static_cfg = os.path.abspath(os.path.join("port", "bsp", args.bsp, MESHX_CFG_BIN))
        cfg_to_flash = generated_cfg if os.path.exists(generated_cfg) else static_cfg

        if not os.path.exists(cfg_to_flash):
            print(f"Warning: Configuration binary not found at {generated_cfg} or {static_cfg}. Skipping meshx_cfg flash.")
            return

        offset, _ = self._get_partition_info(build_dir)
        if offset is None:
            print("Warning: Could not determine meshx-cfg partition offset from zephyr.dts. Skipping meshx_cfg flash.")
            return

        runner = self._get_flash_runner(build_dir, args.bsp)
        print(f"Flashing configuration binary ({cfg_to_flash}) to 0x{offset:X}...")

        if runner == "esp32":
            project_root = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
            esptool_path = os.path.abspath(os.path.join(project_root, "modules", "hal", "espressif", "tools", "esptool_py", ESPTOOL_PY))
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
            esptool_path = os.path.abspath(os.path.join(project_root, "modules", "hal", "espressif", "tools", "esptool_py", ESPTOOL_PY))
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

def _build_zephyr_target(target, args, build_root):
    for prod in args.prod_name:
        build_dir = os.path.join(build_root, prod)
        os.makedirs(build_dir, exist_ok=True)
        target.build_zephyr(args, build_dir, prod)


def _build_esp_target(args, build_root):
    for prod in args.prod_name:
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
        subprocess.run(cmake_command, check=True)
        build_command = ["ninja", "-C", build_dir]
        subprocess.run(build_command, check=True)


def build(args, build_root = "build"):
    """Build the expected target and product."""
    if args.host == "zephyr":
        target = ZephyrTarget(args.port, args.baud)
        _build_zephyr_target(target, args, build_root)
        return
    _build_esp_target(args, build_root)


def _find_valid_bsps(bsp_path):
    if not os.path.exists(bsp_path):
        return []
    bsps = []
    for item in os.listdir(bsp_path):
        if item.startswith('.'):
            continue
        item_path = os.path.join(bsp_path, item)
        if os.path.isdir(item_path) and os.path.exists(os.path.join(item_path, "bsp.cmake")):
            bsps.append(item)
    return bsps


def _find_valid_platforms(plat_path):
    if not os.path.exists(plat_path):
        return []
    platforms = []
    for item in os.listdir(plat_path):
        if item.startswith('.'):
            continue
        if os.path.isdir(os.path.join(plat_path, item)):
            platforms.append(item)
    return platforms


def _init_bsp_and_plat_lists():
    BSP_LIST.extend(_find_valid_bsps(BSP_PATH))
    PLAT_LIST.extend(_find_valid_platforms(PLAT_PATH))


def _create_arg_parser():
    parser = argparse.ArgumentParser(description="MeshX build and target management tool.")
    parser.add_argument('-v', '--version', action="store_true", help="Get the version details of meshx.py")
    parser.add_argument('-m', '--meshx-args', type=str, help="Directory path to meshx.args (Optional)")

    build_group = parser.add_argument_group("Build Options")
    build_group.add_argument('-b', '--build', action="store_true", help="Build respective BSP.")
    build_group.add_argument('-c', '--clean', action="store_true", help="Clean the build directory before building.")
    build_group.add_argument('--list-bsp', action="store_true", help="List available BSPs and exit.")

    bsp_default = BSP_LIST[0] if BSP_LIST else None
    plat_default = PLAT_LIST[0] if PLAT_LIST else None

    build_config_group = parser.add_argument_group("Build Configuration")
    build_config_group.add_argument('-B', '--bsp', choices=BSP_LIST, default=bsp_default, help=f"Specify the BSP to use. Default is {bsp_default}")
    build_config_group.add_argument('-N', '--prod-name', nargs='+', default=[], help="Specify the product name. Defaults to all Product specified in PROD_PROFILE")
    build_config_group.add_argument('-pp', '--prod-profile', default=None, help="Specify the product profile. Defaults to bsp_path/prod_profile.yml")
    build_config_group.add_argument('-D', '--define', action="append", default=[], help="Add compile-time definitions (e.g., -DDEBUG or -DLEVEL=5)")
    build_config_group.add_argument("--build-type", choices=BUILD_TYPE, default=BUILD_TYPE[0], help="Specify the build type. Default is Debug.")

    target_group = parser.add_argument_group("Target Options")
    target_group.add_argument('-H', '--host', choices=PLAT_LIST, default=plat_default, help=f"Target host. Default is {plat_default}")
    target_group.add_argument('-P', '--port', nargs='+', default=None, help="Target port(s), space or comma separated for multi-parallel flashing (e.g., -P COM3 COM4 or -P=COM3,COM4)")
    target_group.add_argument('-C', '--config', action="store_true", help="target configs")
    target_group.add_argument('-F', '--flash', choices=["firmware", "cfg", "all"], nargs='?', const="all", default=None, help="flash ESP target (default: all)")
    target_group.add_argument('-E', '--erase', choices=["firmware", "chip", "cfg", "all"], default=None, help="Erase target")
    target_group.add_argument('-R', '--run', action="store_true", help="Run target")
    target_group.add_argument("--decode", action="store_true", help="Decode TLV logs from target")
    target_group.add_argument("-HR", "--reset", action="store_true", help="Hard reset target")
    target_group.add_argument("--baud", type=int, default=115200, help="Serial baudrate. Default is 115200")
    return parser


def _check_build_prerequisites():
    for tool in ['cmake', 'ninja']:
        check_tool_installed(tool)

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


def _load_meshx_args(parser, args):
    parser.set_defaults(**vars(args))
    file_args = os.path.join(args.meshx_args, "meshx.args")
    if not os.path.exists(file_args):
        raise FileNotFoundError(f"meshx.args file not found at {file_args}")
    with open(file_args, 'r', encoding="utf-8") as f:
        return parser.parse_args(f.read().split())


def _resolve_product_names(args):
    if not os.path.exists(args.prod_profile):
        raise FileNotFoundError(f"Product profile '{args.prod_profile}' not found.")
    with open(args.prod_profile, 'r', encoding="utf-8") as f:
        profile_data = yaml.safe_load(f)

    try:
        available_products = [prod['name'] for prod in profile_data['prod'].get('products', [])]
    except (KeyError, TypeError):
        raise ValueError(f"Product profile '{args.prod_profile}' is not valid.")

    if args.prod_name:
        for prod in args.prod_name:
            if prod not in available_products:
                raise ValueError(f"Product name '{prod}' not found in profile '{args.prod_profile}'. Available products: {', '.join(available_products)}")
    else:
        args.prod_name = available_products
        print(f"No product name specified. Using all products from profile: {args.prod_name}")

    print(f"Final Product Names: {args.prod_name}")


def _parse_target_ports(port_args):
    if not port_args:
        return []
    ports = []
    for p in port_args:
        ports.extend([sub_p.strip() for sub_p in p.split(',') if sub_p.strip()])
    return ports


def _instantiate_target(host, port, baud):
    if host == "esp":
        return ESPTarget(port, baud)
    if host in ["zephyr", "nrf"]:
        return ZephyrTarget(port, baud)
    raise TargetFlashException(f"Unsupported target host: {host}")


def _run_parallel_op(op_name, op_func, target_factory, args, ports):
    if not ports:
        op_func(target_factory(None), args)
        return

    if len(ports) == 1:
        port_args = copy(args)
        port_args.port = ports[0]
        op_func(target_factory(ports[0]), port_args)
        return

    print(f"\n[Parallel {op_name}] Running across {len(ports)} ports: {', '.join(ports)}...")
    errors = {}
    with ThreadPoolExecutor(max_workers=len(ports)) as executor:
        future_to_port = {}
        for port in ports:
            target = target_factory(port)
            port_args = copy(args)
            port_args.port = port
            future_to_port[executor.submit(op_func, target, port_args)] = port

        for future in as_completed(future_to_port):
            p = future_to_port[future]
            try:
                future.result()
                print(f"[Parallel {op_name}] Port {p}: SUCCESS")
            except Exception as e:
                print(f"[Parallel {op_name}] Port {p}: FAILED ({e})")
                errors[p] = e

    if errors:
        raise TargetFlashException(f"{op_name} failed on ports: {list(errors.keys())}")


def _dispatch_erase_flash_actions(args, build_root, ports, target_factory):
    if args.erase in ["firmware", "chip", "all"]:
        _run_parallel_op("Erase", lambda t, a: t.erase(a, build_root), target_factory, args, ports)
    if args.erase in ["cfg", "all"]:
        _run_parallel_op("Erase Config", lambda t, a: t.erase_cfg(a, build_root), target_factory, args, ports)

    if args.flash in ["firmware", "all"]:
        _run_parallel_op("Flash Firmware", lambda t, a: t.flash(a, build_root), target_factory, args, ports)
    if args.flash in ["cfg", "all"]:
        _run_parallel_op("Flash Config", lambda t, a: t.flash_cfg(a, build_root), target_factory, args, ports)


def _dispatch_target_actions(args, build_root, ports, target_factory):
    primary_port = ports[0] if ports else None
    if args.config:
        target_factory(primary_port).configure(build_root, args)

    if args.clean:
        clean(args, build_root)
    if args.build:
        build(args, build_root)

    _dispatch_erase_flash_actions(args, build_root, ports, target_factory)

    if args.reset:
        _run_parallel_op("Reset", lambda t, a: t.reset(a, build_root), target_factory, args, ports)

    if args.run:
        run_args = copy(args)
        run_args.port = primary_port
        target_factory(primary_port).run(run_args, build_root)


def main():
    _init_bsp_and_plat_lists()
    parser = _create_arg_parser()
    args = parser.parse_args()

    if args.version:
        print(f"meshx.py Version: {VERSION_STRING}")
        return

    if args.list_bsp:
        print("Available BSPs:")
        for bsp in BSP_LIST:
            print(f" - {bsp}")
        return

    if args.build or args.config:
        _check_build_prerequisites()

    if args.meshx_args:
        args = _load_meshx_args(parser, args)

    if args.prod_profile is None:
        args.prod_profile = os.path.join("port", "bsp", args.bsp, "prod_profile.yml")

    print(f"meshx.py Version: {VERSION_STRING}")
    print(f"Build Type: {args.build_type}")
    print(f"BSP: {args.bsp}")
    print(f"Product Name: {args.prod_name}")
    print(f"Product Profile: {args.prod_profile}")

    _resolve_product_names(args)
    build_root = os.path.join("build", args.bsp, args.host, args.build_type)
    ports = _parse_target_ports(args.port)
    target_factory = lambda p: _instantiate_target(args.host, p, args.baud)

    _dispatch_target_actions(args, build_root, ports, target_factory)

if __name__ == "__main__":
    try:
        main()
    except KeyboardInterrupt:
        pass
