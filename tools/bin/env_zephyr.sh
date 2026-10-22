#!/bin/bash
# ============================================================================
# MeshX Zephyr Environment Setup Script (Linux Bash)
#
# Must be sourced, not executed directly.
# Usage:
#   source tools/bin/env_zephyr.sh [optional_sdk_source_path]
# ============================================================================

# Exit if not sourced
(return 0 2>/dev/null) || { echo "This script must be sourced, not executed. Run: source tools/bin/env_zephyr.sh"; exit 1; }

# Resolve project directories
export PROJECT_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
SCRIPTS_DIR="$PROJECT_ROOT/tools/scripts"
TOOLS_BIN_DIR="$PROJECT_ROOT/tools/bin"
THIRD_PARTY_DIR="$PROJECT_ROOT/third_party"
TOOLS_DIR="$PROJECT_ROOT/tools"

echo "============================================================"
echo "MeshX Zephyr Environment Setup"
echo "Project Root: $PROJECT_ROOT"
echo "============================================================"

# ----------------------------------------------------------------------------
# 1. Python Discovery & PATH Configuration
# ----------------------------------------------------------------------------
if ! command -v python3 &>/dev/null; then
    if ! command -v python &>/dev/null; then
        echo "[ERROR] python3/python could not be found. Please install Python 3.10+ and add it to PATH."
        return 1
    fi
fi

# Add tools/scripts and tools/bin to PATH
export PATH="$SCRIPTS_DIR:$TOOLS_BIN_DIR:$PATH"

# Ensure ~/.local/bin is on PATH (standard Linux location for pip --user)
if [ -d "$HOME/.local/bin" ]; then
    export PATH="$HOME/.local/bin:$PATH"
fi

# Add Python user local bin (where pip --user installs west, etc.)
if command -v python3 &>/dev/null; then
    USER_BASE=$(python3 -m site --user-base 2>/dev/null)
    if [ -n "$USER_BASE" ] && [ -d "$USER_BASE/bin" ]; then
        export PATH="$USER_BASE/bin:$PATH"
    fi
fi

## ----------------------------------------------------------------------------
# 2. Check/Install Required Python Packages & Build Tools
# ----------------------------------------------------------------------------
PIP_ARGS="--user"
# If PEP 668 externally managed environment is enforced, add --break-system-packages
if python3 -m pip install --help 2>&1 | grep -q -- '--break-system-packages'; then
    PIP_ARGS="--user --break-system-packages"
fi

if ! command -v west &>/dev/null; then
    echo "[INFO] Installing 'west' meta-tool via pip ($PIP_ARGS)..."
    python3 -m pip install $PIP_ARGS west || echo "[WARN] pip install west failed."
    if [ -d "$HOME/.local/bin" ]; then
        export PATH="$HOME/.local/bin:$PATH"
    fi
    if command -v python3 &>/dev/null; then
        USER_BASE=$(python3 -m site --user-base 2>/dev/null)
        if [ -n "$USER_BASE" ] && [ -d "$USER_BASE/bin" ]; then
            export PATH="$USER_BASE/bin:$PATH"
        fi
    fi
    hash -r 2>/dev/null || true
fi

if ! python3 -c "import kconfiglib" &>/dev/null; then
    echo "[INFO] Installing Kconfiglib & Zephyr dependencies via pip ($PIP_ARGS)..."
    python3 -m pip install $PIP_ARGS kconfiglib pyelftools || echo "[WARN] pip install kconfiglib failed."
fi

# Auto-install CMake if missing
if ! command -v cmake &>/dev/null; then
    echo "[INFO] CMake not found. Installing via pip ($PIP_ARGS)..."
    python3 -m pip install $PIP_ARGS cmake
    if ! command -v cmake &>/dev/null; then
        if command -v apt-get &>/dev/null; then
            echo "[INFO] Attempting system package install: sudo apt-get update && sudo apt-get install -y cmake"
            sudo apt-get update && sudo apt-get install -y cmake
        fi
    fi
    hash -r 2>/dev/null || true
fi

# Auto-install Ninja if missing
if ! command -v ninja &>/dev/null; then
    echo "[INFO] Ninja not found. Installing via pip ($PIP_ARGS)..."
    python3 -m pip install $PIP_ARGS ninja
    if ! command -v ninja &>/dev/null; then
        if command -v apt-get &>/dev/null; then
            echo "[INFO] Attempting system package install: sudo apt-get install -y ninja-build"
            sudo apt-get install -y ninja-build
        fi
    fi
    hash -r 2>/dev/null || true
fi

# ----------------------------------------------------------------------------
# 3. Locate Zephyr RTOS (External System Path or Workspace Fallback)
# ----------------------------------------------------------------------------
if [ -n "$ZEPHYR_BASE" ]; then
    if [ -f "$ZEPHYR_BASE/VERSION" ]; then
        echo "[INFO] Using external ZEPHYR_BASE: $ZEPHYR_BASE"
    else
        echo "[WARN] ZEPHYR_BASE is set to '$ZEPHYR_BASE', but VERSION was not found."
        unset ZEPHYR_BASE
    fi
fi

if [ -z "$ZEPHYR_BASE" ]; then
    if [ -f "/mnt/d/Zephyr_Project/zephyr/VERSION" ]; then
        export ZEPHYR_BASE="/mnt/d/Zephyr_Project/zephyr"
    elif [ -f "$HOME/zephyrproject/zephyr/VERSION" ]; then
        export ZEPHYR_BASE="$HOME/zephyrproject/zephyr"
    elif [ -f "$HOME/zephyr/VERSION" ]; then
        export ZEPHYR_BASE="$HOME/zephyr"
    elif [ -f "$THIRD_PARTY_DIR/zephyr/VERSION" ]; then
        export ZEPHYR_BASE="$THIRD_PARTY_DIR/zephyr"
    elif [ -f "$PROJECT_ROOT/../zephyr/VERSION" ]; then
        export ZEPHYR_BASE="$(cd "$PROJECT_ROOT/../zephyr" && pwd)"
    fi
fi

if [ -z "$ZEPHYR_BASE" ]; then
    echo ""
    echo "============================================================================"
    echo "[ERROR] ZEPHYR_BASE is not defined!"
    echo "============================================================================"
    echo "Zephyr RTOS was not detected in environment or workspace."
    echo ""
    echo "How to install Zephyr:"
    echo "  1. Clone Zephyr RTOS to your preferred location (e.g. ~/zephyrproject/zephyr):"
    echo "       git clone --depth 1 --branch v3.7-branch https://github.com/zephyrproject-rtos/zephyr.git ~/zephyrproject/zephyr"
    echo ""
    echo "  2. Set the environment variable in your ~/.bashrc or current session:"
    echo "       export ZEPHYR_BASE=~/zephyrproject/zephyr"
    echo ""
    echo "  3. Re-run this script:"
    echo "       source tools/bin/env_zephyr.sh"
    echo "============================================================================"
    echo ""
    return 1
fi

# Ensure root .west workspace configuration points to ZEPHYR_BASE
mkdir -p "$PROJECT_ROOT/.west"
python3 -c "import os; z=os.environ.get('ZEPHYR_BASE',''); r=os.environ.get('PROJECT_ROOT',''); rel=os.path.relpath(z, r) if z and r else z; open(os.path.join(r, '.west', 'config'), 'w').write(f'[manifest]\npath = {rel}\nfile = west.yml\n\n[zephyr]\nbase = {rel}\n')"


# ----------------------------------------------------------------------------
# 4. Locate Zephyr SDK Toolchain (External System Path or Workspace Fallback)
# ----------------------------------------------------------------------------
if [ -z "$ZEPHYR_SDK_INSTALL_DIR" ] && [ -n "$ZEPHYR_SDK_INSTALL" ]; then
    export ZEPHYR_SDK_INSTALL_DIR="$ZEPHYR_SDK_INSTALL"
fi

if [ -n "$ZEPHYR_SDK_INSTALL_DIR" ]; then
    if [ -f "$ZEPHYR_SDK_INSTALL_DIR/sdk_version" ]; then
        echo "[INFO] Using external ZEPHYR_SDK_INSTALL_DIR: $ZEPHYR_SDK_INSTALL_DIR"
    else
        echo "[WARN] ZEPHYR_SDK_INSTALL_DIR is set to '$ZEPHYR_SDK_INSTALL_DIR', but sdk_version was not found."
        unset ZEPHYR_SDK_INSTALL_DIR
    fi
fi

# Check if first argument points to an SDK
if [ -n "$1" ] && [ -f "$1/sdk_version" ]; then
    export ZEPHYR_SDK_INSTALL_DIR="$(cd "$1" && pwd)"
fi

if [ -z "$ZEPHYR_SDK_INSTALL_DIR" ]; then
    if [ -f "/mnt/d/Zephyr_Project/zephyr-sdk/sdk_version" ]; then
        export ZEPHYR_SDK_INSTALL_DIR="/mnt/d/Zephyr_Project/zephyr-sdk"
    elif [ -f "$HOME/zephyr-sdk/sdk_version" ]; then
        export ZEPHYR_SDK_INSTALL_DIR="$HOME/zephyr-sdk"
    elif [ -f "$THIRD_PARTY_DIR/zephyr-sdk/sdk_version" ]; then
        export ZEPHYR_SDK_INSTALL_DIR="$THIRD_PARTY_DIR/zephyr-sdk"
    elif [ -f "$TOOLS_DIR/zephyr-sdk/sdk_version" ]; then
        export ZEPHYR_SDK_INSTALL_DIR="$TOOLS_DIR/zephyr-sdk"
    elif [ -f "$PROJECT_ROOT/../zephyr-sdk/sdk_version" ]; then
        export ZEPHYR_SDK_INSTALL_DIR="$(cd "$PROJECT_ROOT/../zephyr-sdk" && pwd)"
    else
        # Check standard installation paths like ~/zephyr-sdk-* or /opt/zephyr-sdk*
        for candidate in "$HOME"/zephyr-sdk-* /opt/zephyr-sdk* /opt/toolchains/zephyr-sdk*; do
            if [ -f "$candidate/sdk_version" ]; then
                export ZEPHYR_SDK_INSTALL_DIR="$candidate"
                break
            fi
        done
    fi
fi

# Register Zephyr SDK with CMake if present
if [ -n "$ZEPHYR_SDK_INSTALL_DIR" ] && [ -f "$ZEPHYR_SDK_INSTALL_DIR/cmake/zephyr_sdk_export.cmake" ]; then
    if command -v cmake &>/dev/null; then
        cmake -P "$ZEPHYR_SDK_INSTALL_DIR/cmake/zephyr_sdk_export.cmake" &>/dev/null
    fi
fi

# ----------------------------------------------------------------------------
# 5. Ensure Required Hardware Modules if workspace manifest is active
# ----------------------------------------------------------------------------
if [ -d "$PROJECT_ROOT/.west" ] && command -v west &>/dev/null; then
    if [ ! -d "$PROJECT_ROOT/modules/hal/espressif/.git" ]; then
        echo "[INFO] Fetching hal_espressif module via west..."
        west update hal_espressif 2>/dev/null
    fi
    if [ ! -d "$PROJECT_ROOT/modules/crypto/tinycrypt/.git" ]; then
        echo "[INFO] Fetching tinycrypt module via west..."
        west update tinycrypt 2>/dev/null
    fi
    if [ ! -f "$PROJECT_ROOT/modules/hal/espressif/zephyr/blobs/lib/esp32c3/libbtdm_app.a" ]; then
        echo "[INFO] Fetching hal_espressif binary blobs via west..."
        west blobs fetch hal_espressif 2>/dev/null
    fi
    if [ ! -d "$PROJECT_ROOT/modules/hal/rpi_pico/.git" ]; then
        echo "[INFO] Fetching hal_rpi_pico module via west..."
        west update hal_rpi_pico 2>/dev/null
    fi
    if [ ! -d "$PROJECT_ROOT/modules/hal/cmsis/.git" ]; then
        echo "[INFO] Fetching cmsis module via west..."
        west update cmsis 2>/dev/null
    fi
fi

# ----------------------------------------------------------------------------
# 6. Status Report
# ----------------------------------------------------------------------------
echo ""
echo "[STATUS] Environment Configuration:"
echo "  - ZEPHYR_BASE:            $ZEPHYR_BASE"
if [ -n "$ZEPHYR_SDK_INSTALL_DIR" ]; then
    echo "  - ZEPHYR_SDK_INSTALL_DIR: $ZEPHYR_SDK_INSTALL_DIR"
else
    echo "  - ZEPHYR_SDK_INSTALL_DIR: NOT FOUND"
    echo "    ----------------------------------------------------------------------"
    echo "    How to download and set up the Zephyr SDK:"
    echo "      1. Download the Zephyr SDK bundle for Linux (v0.16.8 or newer):"
    echo "         https://github.com/zephyrproject-rtos/sdk-ng/releases"
    echo "         e.g., zephyr-sdk-0.16.8_linux-x86_64.tar.xz"
    echo ""
    echo "      2. Extract it to your preferred directory (e.g. ~/zephyr-sdk-0.16.8)"
    echo ""
    echo "      3. Register the toolchain in your environment:"
    echo "         export ZEPHYR_SDK_INSTALL_DIR=~/zephyr-sdk-0.16.8"
    echo ""
    echo "      4. Run setup script inside the SDK:"
    echo "         cd ~/zephyr-sdk-0.16.8 && ./setup.sh"
    echo "    ----------------------------------------------------------------------"
fi

if command -v west &>/dev/null; then
    echo "  - west:                   AVAILABLE"
else
    echo "  - west:                   NOT FOUND"
fi

if command -v cmake &>/dev/null; then
    echo "  - cmake:                  AVAILABLE"
else
    echo "  - cmake:                  NOT FOUND [Install via: sudo apt install cmake]"
fi

if command -v ninja &>/dev/null; then
    echo "  - ninja:                  AVAILABLE"
else
    echo "  - ninja:                  NOT FOUND [Install via: sudo apt install ninja-build]"
fi

echo "============================================================"
echo "MeshX Zephyr Environment Ready!"
echo "To build: meshx.py -b -H zephyr -B esp32c3_devkitm -N single_relay"
echo "============================================================"
echo ""

# ----------------------------------------------------------------------------
# 7. Call base MeshX environment setup script post-completion
# ----------------------------------------------------------------------------
if [ -f "$TOOLS_BIN_DIR/env.sh" ]; then
    source "$TOOLS_BIN_DIR/env.sh"
fi
