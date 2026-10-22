#!/usr/bin/env python3
"""
    Copyright © 2024 - 2026 MeshX
    Zephyr Workspace Setup and Tool Dependency Automation

    Strictly anchors all dependencies inside the workspace (third_party/ and tools/):
      - Zephyr Source:   third_party/zephyr
      - Zephyr SDK:      third_party/zephyr-sdk
      - Portable Tools:  tools/bin
      - Modules:         modules/
"""

import os
import sys
import shutil
import subprocess
import argparse
import glob
import site

def check_command(cmd):
    return shutil.which(cmd) is not None

def get_project_root():
    return os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))

def auto_discover_paths(project_root):
    """Auto-discovers and adds Python user scripts, tools/bin, and system tools to PATH."""
    discovered = []
    
    # 0. Local tools/bin and tools/scripts
    tools_bin = os.path.join(project_root, "tools", "bin")
    scripts_dir = os.path.join(project_root, "tools", "scripts")
    for p in [tools_bin, scripts_dir]:
        if os.path.exists(p) and p not in os.environ.get("PATH", ""):
            os.environ["PATH"] = p + os.pathsep + os.environ.get("PATH", "")
            discovered.append(p)

    # 1. Python user Scripts (where pip installs west, kconfiglib)
    try:
        user_base = site.getuserbase()
        v_tag = f"Python{sys.version_info.major}{sys.version_info.minor}"
        candidate_scripts = [
            os.path.join(user_base, v_tag, "Scripts"),
            os.path.join(user_base, "Scripts")
        ]
        for p in candidate_scripts:
            if os.path.exists(p) and p not in os.environ.get("PATH", ""):
                os.environ["PATH"] = p + os.pathsep + os.environ.get("PATH", "")
                discovered.append(p)
    except Exception:
        pass

    # 2. Check standard Program Files and WinGet paths for CMake and Ninja
    for prog_dir in [os.environ.get("ProgramFiles", ""), os.environ.get("ProgramFiles(x86)", "")]:
        if prog_dir:
            cmake_candidate = os.path.join(prog_dir, "CMake", "bin")
            if os.path.exists(cmake_candidate) and cmake_candidate not in os.environ.get("PATH", ""):
                os.environ["PATH"] = cmake_candidate + os.pathsep + os.environ.get("PATH", "")
                discovered.append(cmake_candidate)
            ninja_candidate = os.path.join(prog_dir, "Ninja")
            if os.path.exists(ninja_candidate) and ninja_candidate not in os.environ.get("PATH", ""):
                os.environ["PATH"] = ninja_candidate + os.pathsep + os.environ.get("PATH", "")
                discovered.append(ninja_candidate)

    # WinGet packages / links
    local_app_data = os.environ.get("LOCALAPPDATA", "")
    if local_app_data:
        winget_links = os.path.join(local_app_data, "Microsoft", "WinGet", "Links")
        if os.path.exists(winget_links) and winget_links not in os.environ.get("PATH", ""):
            os.environ["PATH"] = winget_links + os.pathsep + os.environ.get("PATH", "")
            discovered.append(winget_links)
        for ninja_pkg in glob.glob(os.path.join(local_app_data, "Microsoft", "WinGet", "Packages", "Ninja-build.Ninja*")):
            if os.path.exists(os.path.join(ninja_pkg, "ninja.exe")) and ninja_pkg not in os.environ.get("PATH", ""):
                os.environ["PATH"] = ninja_pkg + os.pathsep + os.environ.get("PATH", "")
                discovered.append(ninja_pkg)

    return discovered

def run_cmd(cmd, cwd=None):
    cmd_str = ' '.join(cmd) if isinstance(cmd, list) else cmd
    print(f">> Executing: {cmd_str}")
    subprocess.check_call(cmd, cwd=cwd)

def setup_west_config(project_root, zephyr_path):
    """Ensures .west/config points to third_party/zephyr."""
    west_dir = os.path.join(project_root, ".west")
    os.makedirs(west_dir, exist_ok=True)
    config_path = os.path.join(west_dir, "config")
    rel_zephyr = os.path.relpath(zephyr_path, project_root).replace("\\", "/")
    content = f"[manifest]\npath = {rel_zephyr}\nfile = west.yml\n\n[zephyr]\nbase = {rel_zephyr}\n"
    with open(config_path, "w", encoding="utf-8") as f:
        f.write(content)
    print(f">> Configured west manifest: {config_path} -> {rel_zephyr}")

def register_sdk_cmake(sdk_path):
    """Registers Zephyr SDK CMake package."""
    export_cmake = os.path.join(sdk_path, "cmake", "zephyr_sdk_export.cmake")
    if os.path.exists(export_cmake) and check_command("cmake"):
        try:
            subprocess.run(["cmake", "-P", export_cmake], check=True, capture_output=True)
            print(">> Successfully registered Zephyr SDK with CMake package registry.")
        except Exception as e:
            print(f">> Note: CMake SDK registration returned: {e}")

def main():
    parser = argparse.ArgumentParser(description="MeshX Zephyr Tool and Workspace Dependency Automation")
    parser.add_argument("--branch", type=str, default="v3.7-branch", help="Zephyr branch or tag to clone (default: v3.7-branch)")
    parser.add_argument("--sdk-path", type=str, default=None, help="Path to existing Zephyr SDK to anchor into third_party/zephyr-sdk")
    parser.add_argument("--install-python-deps", action="store_true", help="Automatically install west, kconfiglib, pyelftools")
    parser.add_argument("--update-modules", action="store_true", help="Fetch required hardware HAL modules via west")
    args = parser.parse_args()

    project_root = get_project_root()
    third_party_dir = os.path.join(project_root, "third_party")
    tools_dir = os.path.join(project_root, "tools")
    os.makedirs(third_party_dir, exist_ok=True)
    os.makedirs(os.path.join(tools_dir, "bin"), exist_ok=True)

    # 1. Auto-discover local tools
    auto_discover_paths(project_root)

    print("=" * 60)
    print("MeshX Zephyr Workspace Setup & Dependency Automation")
    print(f"Project Root: {project_root}")
    print(f"Python:       {sys.executable} ({sys.version.split()[0]})")
    print("=" * 60)

    # 2. Check / Install Python dependencies
    if not check_command("west") or args.install_python_deps:
        print("[INFO] Checking/Installing 'west' via pip...")
        run_cmd([sys.executable, "-m", "pip", "install", "west"])
        auto_discover_paths(project_root)

    try:
        import kconfiglib
        import elftools
    except ImportError:
        print("[INFO] Installing kconfiglib and pyelftools...")
        run_cmd([sys.executable, "-m", "pip", "install", "kconfiglib", "pyelftools"])

    # 3. Check cmake and ninja
    for tool in ["cmake", "ninja", "wget"]:
        status = "FOUND" if check_command(tool) else "NOT FOUND"
        print(f"Tool check '{tool}': {status}")

    # 4. Anchor Zephyr source at third_party/zephyr
    zephyr_dest = os.path.join(third_party_dir, "zephyr")
    if os.path.exists(os.path.join(zephyr_dest, "VERSION")):
        print(f"[ANCHOR] Zephyr RTOS present at: {zephyr_dest}")
    else:
        print(f"[INFO] Cloning Zephyr ({args.branch}) into {zephyr_dest}...")
        clone_cmd = [
            "git", "clone",
            "--depth", "1",
            "--branch", args.branch,
            "https://github.com/zephyrproject-rtos/zephyr.git",
            zephyr_dest
        ]
        run_cmd(clone_cmd)
        print("[INFO] Zephyr clone complete.")

    # 5. Configure .west/config
    setup_west_config(project_root, zephyr_dest)

    # 6. Anchor Zephyr SDK at third_party/zephyr-sdk
    zephyr_sdk_dest = os.path.join(third_party_dir, "zephyr-sdk")
    if args.sdk_path and os.path.exists(args.sdk_path):
        if not os.path.exists(zephyr_sdk_dest):
            print(f"[INFO] Anchoring provided SDK from {args.sdk_path} into {zephyr_sdk_dest}...")
            shutil.copytree(args.sdk_path, zephyr_sdk_dest)

    if os.path.exists(os.path.join(zephyr_sdk_dest, "sdk_version")):
        print(f"[ANCHOR] Zephyr SDK toolchain present at: {zephyr_sdk_dest}")
        register_sdk_cmake(zephyr_sdk_dest)
    elif os.path.exists(os.path.join(tools_dir, "zephyr-sdk", "sdk_version")):
        tools_sdk = os.path.join(tools_dir, "zephyr-sdk")
        print(f"[ANCHOR] Zephyr SDK toolchain present at: {tools_sdk}")
        register_sdk_cmake(tools_sdk)
    else:
        print(f"[NOTE] Zephyr SDK not yet placed in {zephyr_sdk_dest}.")
        print("       To install: download Zephyr SDK and place in third_party/zephyr-sdk")

    # 7. Hardware modules
    hal_espressif = os.path.join(project_root, "modules", "hal", "espressif")
    if not os.path.exists(os.path.join(hal_espressif, ".git")):
        print("[INFO] Fetching hal_espressif module via west...")
        try:
            run_cmd(["west", "update", "hal_espressif"], cwd=project_root)
        except Exception as e:
            print(f"[WARNING] west update hal_espressif returned: {e}")
    else:
        print(f"[ANCHOR] hal_espressif module present at: {hal_espressif}")

    print("\n" + "=" * 60)
    print("MeshX Zephyr Workspace Setup Complete!")
    print("Anchors verified:")
    print(f"  - ZEPHYR_BASE:           {zephyr_dest}")
    print(f"  - ZEPHYR_SDK_INSTALL_DIR: {zephyr_sdk_dest}")
    print(f"  - Tools / Scripts:       {os.path.join(project_root, 'tools')}")
    print("=" * 60 + "\n")
    return 0

if __name__ == "__main__":
    sys.exit(main())
