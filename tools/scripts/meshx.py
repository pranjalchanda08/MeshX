#!/usr/bin/env python3

"""
    Copyright © 2024 - 2025 MeshX
"""

import os
import sys
import yaml
import subprocess
import argparse
from abc import ABC, abstractmethod

VERSION_STRING = "v0.0.2"

BUILD_TYPE = ["Debug", "Release"]
BSP_LIST = []
BSP_PATH = "port/bsp"
PLAT_LIST = []
PLAT_PATH = "port/platform"

def check_tool_installed(tool_name):
    """
    Checks if a tool is installed and present in the PATH.

    Args:
        tool_name (str): Name of the tool to check.

    Raises:
        EnvironmentError: If the tool is not installed or not found in PATH.
    """
    result = subprocess.run(['which', tool_name], stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    if result.returncode != 0:
        raise EnvironmentError(f"{tool_name} is not installed or not found in PATH.")

def check_python_tool_installed(tool_name):
    """
    Checks if a Python tool is installed and present in the PATH.

    Args:
        tool_name (str): Name of the Python tool to check.

    Raises:
        EnvironmentError: If the Python tool is not installed or not found in PATH.
    """
    try:
        __import__(tool_name)
    except ImportError:
        raise EnvironmentError(f"{tool_name} is not installed or not found in PATH.")

class InvalidProductNameException(Exception):
    """
    Exception raised when the product name is invalid.
    """

    def __init__(self, message="Please provide exactly one product name"):
        self.message = message
        super().__init__(self.message)

class InvalidBuildTypeException(Exception):
    """
    Exception raised when an invalid build type is provided.
    """

    def __init__(self, message="Invalid build type. Please choose from Debug or Release"):
        self.message = message
        super().__init__(self.message)


class InvalidBspException(Exception):
    """
    Exception raised when an invalid BSP is provided.
    """

    def __init__(self, message="Invalid BSP. Please choose from the available BSP list"):
        self.message = message
        super().__init__(self.message)


class NoBspPathException(Exception):
    """
    Exception raised when no BSP path is provided.
    """

    def __init__(self, message="Please provide a BSP path"):
        self.message = message
        super().__init__(self.message)

class TargetFlashException(Exception):
    """
    Exception raised when a target is not found.
    """

    def __init__(self, message="Target not found"):
        self.message = message
        super().__init__(self.message)

class Target(ABC):
    def __init__(self, sdk, port):
        """
        Initializes a Target object.

        Args:
            sdk (str): The software development kit for the target.
            port (str): The serial port to use for communication with the target.

        """
        self.sdk = sdk
        self.port = port
        self.toolcheck()

    def __str__(self):
        """
        Returns a string representation of the Target object.

        The string contains the SDK name, chip name, and serial port used for
        communication with the target.

        Returns:
            str: A string representation of the Target object.
        """
        return f"SDK: {self.sdk}, Port: {self.port}"

    def proc_run(self, cmd, cwd=None):
        result = subprocess.run(cmd, check=True, cwd=cwd)
        if result.returncode != 0:
            raise TargetFlashException("Target Error")

    @abstractmethod
    def toolcheck(self):
        """
        Abstract method to perform tool checks on the target.

        This method should be overridden by subclasses to perform any necessary
        tool checks before proceeding with the target's configuration or flashing.

        Note: This method should not throw any exceptions. Instead, it should return
        normally if the tool checks pass, or not at all if the tool checks fail.

        Returns:
            None
        """
        pass

    @abstractmethod
    def configure(self, build_dir):
        """
        Abstract method to configure the target.

        This method should be overridden by subclasses to perform any necessary
        configuration steps before proceeding with the target's flashing.

        Returns:
            None
        """
        pass

    @abstractmethod
    def flash(self, args, build_root):
        """
        Abstract method to flash the target.

        This method should be overridden by subclasses to perform any necessary
        flashing steps using the provided arguments and build root.

        Args:
            args (list): A list of arguments to pass to the flashing tool.
            build_root (str): The path to the build root directory.

        Returns:
            None
        """
        pass

    @abstractmethod
    def erase(self, args, build_root):
        """
        Abstract method to erase the target.

        This method should be overridden by subclasses to perform any necessary
        erasing steps using the provided arguments and build root.

        Args:
            args (list): A list of arguments to pass to the erasing tool.
            build_root (str): The path to the build root directory.

        Returns:
            None
        """
        pass

    @abstractmethod
    def run(self, args, build_root):
        """
        Abstract method to run the target.

        This method should be overridden by subclasses to perform any necessary
        steps to run the target using the provided arguments and build root.

        Args:
            args (list): A list of arguments to pass to the running tool.
            build_root (str): The path to the build root directory.

        Returns:
            None
        """
        pass

class ESPTarget(Target):
    def __init__(self, port, baud):
        """
        Initialize the ESPTarget class.

        Args:
            port (str): The serial port associated with the ESP target.
            baud (int): The baud rate for the serial port.

        Returns:
            None
        """
        super().__init__("esp", port)
        self.baud = baud

    def toolcheck(self):
        print("Checking ESP tools...")
        tool = ['esptool.py']
        for t in tool:
            check_tool_installed(t)
        python_tool = ['esp_idf_monitor']
        for p in python_tool:
            check_python_tool_installed(p)
    def configure(self, build_root):
        if len(args.prod_name) != 1:
            raise InvalidProductNameException()

        build_dir = f"{build_root}/{args.prod_name[0]}"
        subprocess.run(["ninja", "-C", build_dir, "menuconfig"], check=True)

    def flash(self, args, build_root = "build"):
        if len(args.prod_name) != 1:
            raise InvalidProductNameException()

        if args.port is None:
            raise TargetFlashException("Please provide a port")
        else:
            print(f"Using port: {args.port}")
            # Check if port is valid
            if not os.path.exists(args.port):
                raise TargetFlashException(f"Port {args.port} does not exist")

        build_dir = f"{build_root}/{args.prod_name[0]}"
        esp_tool_cmd = [
            "esptool.py",
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

        build_dir = f"{build_root}/{args.prod_name[0]}"
        esp_tool_cmd = [
            "esptool.py",
            "--chip", "auto",
            "--port", args.port,
            "--before", "default_reset",
            "--after", "hard_reset",
            "erase_flash",
        ]
        self.proc_run(esp_tool_cmd, cwd=build_dir)

    def run(self, args, build_root = "build"):
        if len(args.prod_name) != 1:
            raise InvalidProductNameException()

        build_dir = f"{build_root}/{args.prod_name[0]}"
        esp_tool_cmd = [
            "python", "-m", "esp_idf_monitor",
            "--port", args.port,
            "--baud", str(self.baud),
        ]
        self.proc_run(esp_tool_cmd, cwd=build_dir)
class NRFTarget(Target):
    def __init__(self, sdk, port):
        super().__init__(sdk, port)
        raise TargetFlashException("Not implemented yet")

def clean(args, build_root = "build"):
    """
    Clean the build directory by removing all build artifacts.

    Args:
        args (list): A list of arguments to pass to the build helper.
        build_root (str): The path to the build root directory.

    Returns:
        None
    """
    for prod in args.prod_name:
        build_dir = f"{build_root}/{prod}"
        if os.path.exists(build_dir):
            print(f"Cleaning build directory: {build_dir}")
            subprocess.run(["rm", "-rf", build_dir], check=True)

def build(args, build_root = "build"):
    """
    Build the expected target and product.

    Args:
        args (list): A list of arguments to pass to the build helper.
        build_root (str): The path to the build root directory.

    Returns:
        None
    """
    for prod in args.prod_name:
        # Delete any existing sdkconfig file
        sdkconfig_path = "sdkconfig"
        if os.path.exists(sdkconfig_path):
            os.remove(sdkconfig_path)

        build_dir = f"{build_root}/{prod}"

        os.makedirs(build_dir, exist_ok=True)
        cmake_command =    ["cmake",
                            "-S", ".", "-B", build_dir, "-G", "Ninja",
                            f"-DBSP={args.bsp}",
                            f"-DPROD_NAME={prod}",
                            f"-DMESHX_BUILD_TYPE={args.build_type}",
                            f"-DPROD_PROFILE={args.prod_profile}",
                            f"-DELF='meshx_build_{args.bsp}'"
                        ]
        print("Running CMake command:", " ".join(cmake_command))
        # run cmake command
        subprocess.run(cmake_command, check=True)
            # sys.exit(0)
        # run ninja build from build directory
        build_command = ["ninja", "-C", build_dir]
        subprocess.run(build_command, check=True)

if __name__ == "__main__":
    for bsp in os.listdir(BSP_PATH):
        if os.path.isdir(os.path.join(BSP_PATH, bsp)):
            if bsp.startswith('.') or os.path.exists(os.path.join(BSP_PATH, bsp, "bsp.cmake")) == False:
                continue
            BSP_LIST.append(bsp)

    for plat in os.listdir(PLAT_PATH):
        if os.path.isdir(os.path.join(PLAT_PATH, plat)):
            if plat.startswith('.'):
                continue
            PLAT_LIST.append(plat)

    # Ensure the OS is Linux
    if os.name != 'posix':
        raise EnvironmentError("This script can only be run on Linux systems.")

    # Ensure python version is 3.6 or higher
    import sys
    if sys.version_info < (3, 6):
        raise EnvironmentError("This script requires Python 3.6 or higher.")

    # Ensure required tools are installed
    def expand_path(path):
        return os.path.expanduser(path) if path else None

    required_tools = ['cmake', 'ninja', 'git']
    for tool in required_tools:
        check_tool_installed(tool)

    # Parse command-line arguments
    parser = argparse.ArgumentParser(description="Build MeshX project.")

    parser.add_argument("-v","--version",        action="store_true",    help="Get the version details of meshx.py")
    # Load an arg file. Set as positional argument to ensure it is parsed before other arguments. Required=False

    parser.add_argument('-m', '--meshx-args', type=str, help="Directory path to meshx.args (Optional)")
    build_group = parser.add_argument_group("Build Options")

    build_group.add_argument("-b", "--build",    action="store_true",    help="Build respecective BSP.")
    build_group.add_argument("-c", "--clean",    action="store_true",    help="Clean the build directory before building.")
    build_group.add_argument("--list-bsp",       action="store_true",    help="List available BSPs and exit.")

    # Add a group for build configuration
    build_config_group = parser.add_argument_group("Build Configuration")
    build_config_group.add_argument("-B", "--bsp",          choices=BSP_LIST,       default=BSP_LIST[0], help=f"Specify the BSP to use. Default is {BSP_LIST[0]}")
    build_config_group.add_argument("-N", "--prod-name",    nargs='+',              default=[],   help="Specify the product name. Defaults to all Product specified in PROD_PROFILE")
    build_config_group.add_argument("-pp", "--prod-profile",default=None,           help="Specify the product profile. Defaults to bsp_path/prod_profile.yml")
    build_config_group.add_argument("--build-type",         choices=BUILD_TYPE,     default=BUILD_TYPE[0], help= "Specify the build type. Default is Debug.")

    target_group = parser.add_argument_group("Target Options")
    target_group.add_argument("-H", "--host",   choices=PLAT_LIST,      default=PLAT_LIST[0], help=f"Target host. Default is {PLAT_LIST[0]}")
    target_group.add_argument("-P", "--port",   type=str, default=None, help="Target port")
    target_group.add_argument("-C", "--config", action="store_true", help="target configs")
    target_group.add_argument("-F", "--flash",  action="store_true", help="flash ESP target")
    target_group.add_argument("-E", "--erase",  choices=["firmware", "chip"], default=None, help="Erase target")
    target_group.add_argument("-R", "--run",    action="store_true", help="Run target")
    target_group.add_argument("--baud",         type=int, default=115200, help="Serial baudrate. Default is 115200")

    args = parser.parse_args()

    if args.meshx_args:
        # Load arguments from a file. Do not override existing arguments
        parser.set_defaults(**vars(args))
        file_args = os.path.join(args.meshx_args, "meshx.args")
        if not os.path.exists(file_args):
            raise FileNotFoundError(f"meshx.args file not found at {file_args}")

        with open(file_args, 'r') as f:
            args = parser.parse_args(f.read().split())
    if args.list_bsp:
        print("Available BSPs:")
        # port/bsp directory listing
        for bsp in os.listdir(BSP_PATH):
            if os.path.isdir(os.path.join(BSP_PATH, bsp)):
                if bsp.startswith('.') or os.path.exists(os.path.join(BSP_PATH, bsp, "bsp.cmake")) == False:
                    continue
                print(f" - {bsp}")
        sys.exit(0)

    if args.prod_profile is None:
        args.prod_profile = f"port/bsp/{args.bsp}/prod_profile.yml"

    # Print the parsed arguments
    print(f"meshx.py Version: {VERSION_STRING}")
    if args.version:
        sys.exit(0)
    print(f"Build Type: {args.build_type}")
    print(f"BSP: {args.bsp}")
    print(f"Product Name: {args.prod_name}")
    print(f"Product Profile: {args.prod_profile}")

    if args.prod_name != []:
        # Load product names from args.prod_profile eg: tools/scripts/prod_profile.ci.yml
        with open(args.prod_profile, 'r') as f:
            profile_data = yaml.safe_load(f)
        available_products = [prod['name'] for prod in profile_data['prod'].get('products', [])]
        for prod in args.prod_name:
            if prod not in available_products:
                raise ValueError(f"Product name '{prod}' not found in profile '{args.prod_profile}'. Available products: {', '.join(available_products)}")
    else:
        # If no product name is specified, use all products from the profile
        with open(args.prod_profile, 'r') as f:
            profile_data = yaml.safe_load(f)
        args.prod_name = [prod['name'] for prod in profile_data['prod'].get('products', [])]
        print(f"No product name specified. Using all products from profile: {args.prod_name}")

    print(f"Final Product Names: {args.prod_name}")

    build_root = f"build/{args.bsp}/{args.build_type}"

    target_sel = None
    if args.host == "esp":
        target_sel = ESPTarget(args.port, args.baud)
    else:
        raise TargetFlashException(f"Unsupported target host: {args.host}")

    # All Target process Stages
    if args.config:
        target_sel.configure(build_root)

    # All Build process Stages
    if args.clean:
        clean(args, build_root)

    if args.build:
        build(args, build_root)

    # All Tartget process Stages
    if args.erase:
        target_sel.erase(args, build_root)
    if args.flash:
        target_sel.flash(args, build_root)
    if args.run:
        target_sel.run(args, build_root)
