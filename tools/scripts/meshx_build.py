import os
import sys
import subprocess
import argparse

BUILD_TYPE = ["Debug", "Release"]
BSP_LIST = []
for bsp in os.listdir("port/bsp"):
    if os.path.isdir(os.path.join("port/bsp", bsp)):
        if bsp.startswith('.'):
            continue
        BSP_LIST.append(bsp)

# Ensure the OS is Linux
if os.name != 'posix':
    raise EnvironmentError("This script can only be run on Linux systems.")

# Ensure python version is 3.6 or higher
import sys
if sys.version_info < (3, 6):
    raise EnvironmentError("This script requires Python 3.6 or higher.")

# Ensure required tools are installed
def check_tool_installed(tool_name):
    result = subprocess.run(['which', tool_name], stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    if result.returncode != 0:
        raise EnvironmentError(f"{tool_name} is not installed or not found in PATH.")

def expand_path(path):
    return os.path.expanduser(path) if path else None

required_tools = ['cmake', 'ninja', 'git']
for tool in required_tools:
    check_tool_installed(tool)

# Parse command-line arguments
parser = argparse.ArgumentParser(description="Build MeshX project.")

parser.add_argument("--list-bsp",       action="store_true",    help="List available BSPs and exit.")
parser.add_argument("--clean",          action="store_true",    help="Clean the build directory before building.")
parser.add_argument("--bsp",            choices=BSP_LIST,       default=BSP_LIST[0], help="Specify the BSP to use.")
parser.add_argument("--prod-name",      nargs='+',              default=[],   help="Specify the product name.")
parser.add_argument("--prod-profile",   default=None,           help="Specify the product profile.")
parser.add_argument("--build-type",     choices=BUILD_TYPE,     default=BUILD_TYPE[0], help= "Specify the build type.")

# Add a group for ESP targets
esp_group = parser.add_argument_group("ESP targets")
esp_group.add_argument("--menuconfig", action="store_true", help="call ninja menuconfig after cmake")

args = parser.parse_args()

if args.list_bsp:
    print("Available BSPs:")
    # port/bsp directory listing
    for bsp in os.listdir("port/bsp"):
        if os.path.isdir(os.path.join("port/bsp", bsp)):
            if bsp.startswith('.'):
                continue
            print(f" - {bsp}")
    sys.exit(0)

if args.prod_profile is None:
    args.prod_profile = f"port/bsp/{args.bsp}/prod_profile.yml"

# Print the parsed arguments
print(f"Build Type: {args.build_type}")
print(f"BSP: {args.bsp}")
print(f"Product Name: {args.prod_name}")
print(f"Product Profile: {args.prod_profile}")

if not args.prod_name == []:
    # Load product names from args.prod_profile eg: tools/scripts/prod_profile.ci.yml
    import yaml
    with open(args.prod_profile, 'r') as f:
        profile_data = yaml.safe_load(f)
    available_products = [prod['name'] for prod in profile_data['prod'].get('products', [])]
    for prod in args.prod_name:
        if prod not in available_products:
            raise ValueError(f"Product name '{prod}' not found in profile '{args.prod_profile}'. Available products: {', '.join(available_products)}")
else:
    # If no product name is specified, use all products from the profile
    import yaml
    with open(args.prod_profile, 'r') as f:
        profile_data = yaml.safe_load(f)
    args.prod_name = list(prod['name'] for prod in profile_data['prod'].get('products', []))
    print(f"No product name specified. Using all products from profile: {args.prod_name}")

print(f"Final Product Names: {args.prod_name}")

for prod in args.prod_name:
    # Delete any existing sdkconfig file
    sdkconfig_path = "sdkconfig"
    if os.path.exists(sdkconfig_path):
        os.remove(sdkconfig_path)

    build_dir = f"build/{args.bsp}/{args.build_type}/{prod}"
    if args.clean:
        if os.path.exists(build_dir):
            print(f"Cleaning build directory: {build_dir}")
            subprocess.run(["rm", "-rf", build_dir], check=True)

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
    if args.menuconfig:
        cmake_command.append("-DPLATFORM_MENUCONFIG=ON")
        subprocess.run(["ninja", "-C", build_dir, "menuconfig"], check=True)
        # sys.exit(0)
    # run ninja build from build directory
    build_command = ["ninja", "-C", build_dir]
    subprocess.run(build_command, check=True)
