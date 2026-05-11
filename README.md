# MeshX

```txt
*********************************************************************************************************************
* MMMMMMMM               MMMMMMMM                                     hhhhhhh                 XXXXXXX       XXXXXXX *
* M:::::::M             M:::::::M                                     h:::::h                 X:::::X       X:::::X *
* M::::::::M           M::::::::M                                     h:::::h                 X:::::X       X:::::X *
* M:::::::::M         M:::::::::M                                     h:::::h                 X::::::X      X:::::X *
* M::::::::::M       M::::::::::M    eeeeeeeeeeee        ssssssssss   h:::: hhhhhh            XX:::::X     X:::::XX *
* M:::::::::::M     M:::::::::::M  ee::::::::::::ee    ss::::::::::s  h::::::::::hhh            X:::::X   X:::::X   *
* M:::::::M::::M   M::::M:::::::M e::::::eeeee:::::eess:::::::::::::s h::::::::::::::hh           X:::::X:::::X     *
* M::::::M M::::M M::::M M::::::Me::::::e     e:::::es::::::ssss:::::sh:::::::hhh::::::h           X:::::::::X      *
* M::::::M  M::::M::::M  M::::::Me:::::::eeeee::::::e s:::::s  ssssss h::::::h   h::::::h          X:::::::::X      *
* M::::::M   M:::::::M   M::::::Me:::::::::::::::::e    s::::::s      h:::::h     h:::::h         X:::::X:::::X     *
* M::::::M    M:::::M    M::::::Me::::::eeeeeeeeeee        s::::::s   h:::::h     h:::::h        X:::::X X:::::X    *
* M::::::M     MMMMM     M::::::Me:::::::e           ssssss   s:::::s h:::::h     h:::::h     XXX:::::X   X:::::XXX *
* M::::::M               M::::::Me::::::::e          s:::::ssss::::::sh:::::h     h:::::h     X::::::X     X::::::X *
* M::::::M               M::::::M e::::::::eeeeeeee  s::::::::::::::s h:::::h     h:::::h     X:::::X       X:::::X *
* M::::::M               M::::::M  ee:::::::::::::e   s:::::::::::ss  h:::::h     h:::::h     X:::::X       X:::::X *
* MMMMMMMM               MMMMMMMM    eeeeeeeeeeeeee    sssssssssss    hhhhhhh     hhhhhhh     XXXXXXX       XXXXXXX *
*                                                                                                                   *
*                                                  Version: 0.5.0                                                   *
*********************************************************************************************************************
```

[![CI Pipeline:main](https://github.com/pranjalchanda08/MeshX/actions/workflows/build_ci.yml/badge.svg)](https://github.com/pranjalchanda08/MeshX/actions/workflows/ci.yml) ![Release](https://img.shields.io/badge/Release-v0.5--alpha-blue)

MeshX is a portable C/C++ implementation of a Bluetooth Low Energy (BLE) Mesh node stack and example components. It is designed to be portable across board support packages (BSPs), microcontroller units (MCUs) and SDKs via a CMake-driven build system and small platform abstraction layers.

## Highlights
- **C++ Migration**: Modern element registry and model hierarchy for better modularity and type safety.
- **Dynamic Composition**: Auto-baking of device composition at runtime based on product profiles.
- **Portability**: CMake-based build integration for multiple BSPs (WeAct, Xiao, DevKitC) and MCUs.
- **Storage**: Platform-agnostic NVS with wear-leveling and KV engine support.
- **Build System**: Unified `meshx.py` wrapper for build, flash, and test automation.

## Repository Layout
- `main/` - Core logic, component initialization, and BLE Mesh model hierarchy.
- `port/` - BSPs and Platform Abstraction Layers (PAL).
- `tools/scripts/` - Build helpers, `meshx.py`, and code generation utilities.

## Build System (meshx.py)

The `meshx.py` script is the primary interface for managing builds, flashing, and monitoring.

### 1. Environment Setup
To set up the environment (including ESP-IDF):
```bash
$ source tools/scripts/env.sh source /path/to/esp-idf/export.sh
```

### 2. Build Commands
The tool supports both clean and incremental builds:

| Command | Description |
|---------|-------------|
| `meshx.py -bc [options]` | **Clean Build**: Wipes the build directory and re-configures. |
| `meshx.py -b [options]` | **Incremental Build**: Recommended for faster development cycles. |
| `meshx.py -FR [options]` | **Flash and Run**: Flashes the binary and starts the serial monitor. |

### 3. CLI Usage Summary
```txt
usage: meshx.py [-h] [-v] [-b] [-c] [-B BSP] [-N PROD_NAME] [-P PORT] [-F] [-R]

options:
  -v, --version         Get the version details of meshx.py
  -b, --build           Build respective BSP.
  -c, --clean           Clean the build directory before building.
  -B, --bsp BSP         Specify the BSP to use (e.g., xiao_c3).
  -N, --prod-name NAME  Specify the product name (e.g., all_in_one).
  -P, --port PORT       Serial port (e.g., /dev/ttyACM0).
  -F, --flash           Flash target.
  -R, --run             Run target (starts monitor).
```

### Examples
**Clean build for Xiao C3:**
```bash
$ ./tools/scripts/meshx.py -B xiao_c3 -N all_in_one -bc
```

**Flash and Run:**
```bash
$ ./tools/scripts/meshx.py -B xiao_c3 -N all_in_one -FR -P /dev/ttyACM0
```
> [!NOTE]
> Terminate the run/monitor session using `CTRL + ]`.

## Supported Platforms

### BSPs
| BSP name      | Board / Notes                    |
|---------------|----------------------------------|
| weact_c3      | WeAct ESP32-C3 development board |
| xiao_c3       | Seeed Studio XIAO ESP32-C3       |
| esp32_devkitC | ESP32 WROOM Development board    |

### SDKs
- **ESP-IDF**: Fully integrated via `port/platform/esp/esp_idf/`. Requires ESP-IDF v5.4+.

## Configuration & Customization

### Product Profiles
Product profiles live in `port/bsp/<bsp>/prod_profile.yml`. They define the elements and models included in a specific build.

### Code Generation
The build helper invokes `tools/scripts/code_gen.py` at configure time to generate `meshx_config.h` based on the selected product profile.

## Documentation

For deeper technical details, refer to the internal documentation:

### 🏗️ Architecture & Design
- **[Call Flow Diagrams](docs/uml_diag/meshx/meshx_call_flow.md)**: Detailed sequence diagrams of system initialization and messaging.
- **[Class Diagram](docs/uml_diag/meshx/meshx_class_diagram.md)**: Object-oriented structure of the MeshX stack.

### 📖 Guides
- **[Developer Guide: Custom Models & Elements](docs/DEVELOPER_GUIDE_CUSTOM_MODELS_AND_ELEMENTS.md)**: Step-by-step guide on extending the stack with custom BLE Mesh models.
- **[Unit Testing](main/component/unit_test/README.md)**: Information on running local and platform-level unit tests.
- **[Auto-test Framework](tools/scripts/autotest/README.md)**: Documentation for the automated serial-based testing tool.

---
For detailed information on contributing or adding new platforms, please refer to the documentation in the `docs/` directory.
