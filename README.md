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
*                                                  Version: 0.6.1                                                   *
*********************************************************************************************************************
```

[![CI Pipeline:main](https://github.com/pranjalchanda08/MeshX/actions/workflows/build_ci.yml/badge.svg)](https://github.com/pranjalchanda08/MeshX/actions/workflows/ci.yml) ![Release](https://img.shields.io/badge/Release-v0.6.1-blue)


MeshX is a portable C/C++ implementation of a Bluetooth Low Energy (BLE) Mesh node stack and example components. It is designed to be portable across board support packages (BSPs), microcontroller units (MCUs) and SDKs via a CMake-driven build system and small platform abstraction layers.

## Highlights
- **C++ Migration**: Modern element registry and model hierarchy for better modularity and type safety.
- **Dynamic Composition**: Auto-baking of device composition at runtime based on product profiles.
- **Portability**: CMake-based build integration for multiple BSPs (WeAct, Xiao, DevKitC) and MCUs.
- **Storage**: Platform-agnostic NVS with wear-leveling and KV engine support.
- **Build System**: Unified `meshx.py` wrapper for build, flash, and test automation.

## Repository Layout
- `main/` - Core application logic, BLE Mesh model hierarchy, and element composition.
- `port/` - Board Support Packages (BSP), RTOS ports (FreeRTOS, Zephyr), and Platform Abstraction Layers (PAL).
- `zephyr/` - Zephyr external module definition (`module.yml`, `CMakeLists.txt`, `Kconfig`).
- `tools/bin/` - Portable environment setup scripts (`env.bat`, `env_zephyr.bat`, `env.sh`, `env_zephyr.sh`) and CLI wrappers (`meshx.bat`).
- `tools/scripts/` - `meshx.py`, host log decoder, code generation utilities, and test suites.

## Build System (meshx.py)

The `meshx.py` script is the primary interface for managing builds, flashing, and monitoring.

### 1. Environment Setup
**For ESP-IDF (Linux):**
```bash
$ source tools/bin/env.sh source /path/to/esp-idf/export.sh
```

**For ESP-IDF (Windows Batch):**
```bat
.\tools\bin\env.bat <path_to_esp_idf>\export.bat
```

**For Zephyr RTOS:**
If Zephyr and Zephyr SDK are not installed, download and configure them:
1. Clone Zephyr RTOS:
   ```bash
   git clone --depth 1 --branch v3.7-branch https://github.com/zephyrproject-rtos/zephyr.git <path_to_zephyr>
   ```
2. Download the Zephyr SDK from GitHub releases:
   https://github.com/zephyrproject-rtos/sdk-ng/releases (e.g. `zephyr-sdk-0.16.8`)
3. Set your environment variables:
   ```bash
   # Linux:
   export ZEPHYR_BASE="<path_to_zephyr>"
   export ZEPHYR_SDK_INSTALL_DIR="<path_to_zephyr_sdk>"

   # Windows:
   setx ZEPHYR_BASE "<path_to_zephyr>"
   setx ZEPHYR_SDK_INSTALL_DIR "<path_to_zephyr_sdk>"
   ```
4. Run the setup script:
   ```bash
   # Linux:
   source ./tools/bin/env_zephyr.sh

   # Windows:
   call .\tools\bin\env_zephyr.bat
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
usage: meshx.py [-h] [-v] [-m MESHX_ARGS] [-b] [-c] [--list-bsp]
                [-B {esp32_devkitC,rpi_pico_w,weact_c3,xiao_c3}]
                [-N PROD_NAME [PROD_NAME ...]] [-pp PROD_PROFILE] [-D DEFINE]
                [--build-type {Debug,Release}] [-H {esp,zephyr}] [-P PORT]
                [-C] [-F [{firmware,cfg,all}]] [-E {firmware,chip,cfg,all}]
                [-R] [--decode] [-HR] [--baud BAUD]
```

### Examples
**Clean build for Xiao C3 (ESP-IDF):**
```bash
$ meshx.py -B xiao_c3 -N all_in_one -bc
```

**Build for WeAct C3 on Zephyr RTOS:**
```bash
$ meshx.py -b -H zephyr -B weact_c3 -N 4_relay_panel
```

**Flash, Monitor, and Live Decode Tokenized Logs:**
```bash
$ meshx.py -B xiao_c3 -N all_in_one -FR -P /dev/ttyACM0 --decode
```

## Supported Platforms & SDKs

MeshX features a layered, decoupled design supporting multiple RTOS kernels and vendor SDKs:

### 1. Board Support Packages (BSPs)

| BSP Name | Target MCU | Architecture | ESP-IDF | Zephyr |
|:---|:---|:---|:---|:---|
| **`weact_c3`** | ESP32-C3 | RISC-V 32-bit | ✅ | ✅ |
| **`xiao_c3`** | Seeed Studio XIAO ESP32-C3 | RISC-V 32-bit | ✅ | ✅ |
| **`esp32_devkitC`** | ESP32-WROOM-32 | Xtensa Dual-Core | ✅ | ✅ |
| **`rpi_pico_w`** | Raspberry Pi RP2040 | ARM Cortex-M0+ | ❌ | ⚠️ WIP |

### 2. Supported SDKs & Operating Systems

* **ESP-IDF (`-H esp`)**:
  * Platform integration in `port/platform/esp/esp_idf/`.
  * Runs on FreeRTOS (`port/os/FreeRTOS/`).
  * Requires ESP-IDF **v5.4+**.
* **Zephyr RTOS (`-H zephyr`)**:
  * Out-of-tree module integration via `zephyr/module.yml`, `zephyr/CMakeLists.txt`, and `zephyr/Kconfig`.
  * Platform & OSAL implementation in `port/platform/zephyr/` and `port/os/zephyr/`.
  * Uses Zephyr's native Bluetooth Mesh stack with MeshX element composition and dynamic models.
  * Compatible with Zephyr **v3.7+** and Zephyr SDK **v0.16.8+**.

---

## Configuration & Customization

### Product Profiles
Product definitions are located in `port/bsp/<bsp>/prod_profile.yml`. They specify:
- Elements (Relay, Light CWWW, Light HSL, Sensor, etc.)
- Light weight Vendor model compositions.
- Bluetooth Mesh Company ID (CID), Product ID (PID), and Version ID (VID)

### Dynamic Code Generation
At configure time, `tools/scripts/code_gen.py` parses the product profile and emits:
- `meshx_config.h` (Hardware mapping, elements count, model definitions)
- `meshx_cfg.bin` (Compiled binary config partition for non-volatile deployment)

### Binary Log Decoder (`--decode`)
MeshX supports a tokenized, zero-RAM binary logging system. Instead of shipping printf format strings to target RAM, strings reside in a no-load ELF dictionary. The host dynamically reconstructs formatted color logs:
```bash
# Monitor and decode live telemetry & logs
python tools/scripts/host_decoder.py -p /dev/ttyACM0 -b 115200 -e build/weact_c3/meshx_build_weact_c3.elf
```

---

## Feature Catalogue

### 1. Bluetooth Low Energy (BLE) Mesh Subsystem
* **Minimalistic SIG Foundation Models**: Bare-minimum SIG models required for standard BLE Mesh network compliance and onboarding:
  * **Configuration Server (`CONFIG_SRV`)**: Core network parameters, app/net keys, and publication/subscription management.
  * **Provisioning Server (`PROV_SRV`)**: Standard PB-ADV and PB-GATT bearer onboarding.
* **Universal Vendor Model (UVP)**:
  * Proprietary MeshX vendor model running as the single high-efficiency transport layer for all application workloads.
  * Carries typed logical payloads: Relay Power, CWWW Tunable White, HSL Color Lighting, and Sensor Data.
  * Bypasses heavy multi-model SIG overhead while providing compact, fast, vendor-optimized message handling.
* **Multi-Element Node Support**: Dynamic multi-element node composition (up to 16 elements per node) with per-element publication/subscription contexts.
* **Mesh Network Roles**: PB-ADV, PB-GATT, GATT Proxy, Relay, and Friend node roles supported out of the box.
* **Transmission Control Manager (TXCM)**: Built-in reliable transaction layer providing autonomous packet retries, deduplication caching, and acknowledgment handling.

---

### 2. Autonomous Pairing & Dynamic Linking Engine
* **1-Hop Proximity Pairing**: Autonomous RSSI-filtered pairing between producer and consumer nodes without requiring a smartphone application or gateway.
* **Multi-Hop Relayed Pairing**: Extends proximity pairing over existing mesh nodes to provision and bind devices across complex topologies.
* **Dynamic Group Allocator**: On-the-fly allocation of multicast group addresses (`0xC000`–`0xC3FF`) for binding endpoints.
* **Semantic Compatibility Matrix**: Built-in 1:1 Look-Up Tables (LUTs) validating matching producer/consumer roles, element variants, and functional capabilities before establishing links.
* **Autonomous Unpairing & Link Reconciliation**: Complete unbinding, state synchronization, and epoch tracking across paired groups.

---

### 3. Dynamic Composition & NVS Storage
* **Dynamic Composition Builder**: Fluent C++ builder API (`meshXCompositionBuilder`) to compose, register, and bake models and elements dynamically at boot based on product profiles.
* **Dual-Tier NVS Persistence**:
  * **BLE Mesh Network State**: Non-volatile storage of sequence numbers, IV index, network/app keys, and replay protection lists.
  * **MeshX Portable KV Engine**: Flash Abstraction Layer (FAL) with wear-leveling, garbage collection, and Protobuf schema decoding for custom device personalities.

---

### 4. Hardware Abstraction & Serial Command Protocol (MXCP)
* **Unified Hardware Control**:
  * GPIO runtime with pin-state tracking, edge/level interrupt wrappers, and logical-to-physical pin mapping.
  * Multi-channel PWM support with dynamic duty cycle and frequency configuration.
* **MeshX Command Protocol (MXCP / MXSP)**:
  * Single-layer, table-driven UART command/event bridge for communicating with an external host MCU.
  * Hosted GPIO mode allowing external microcontrollers to inspect pins, toggle outputs, and receive async interrupt events over serial.
* **Interactive Diagnostic Shell**: Embedded serial console providing interactive node inspection, test routines, and runtime diagnostics.

---

### 5. Developer Observability & Zero-RAM Logging
* **Tokenized Binary Logging (`--decode`)**:
  * Printf format strings and metadata are stored exclusively in the host ELF dictionary (zero target RAM overhead).
  * Host-side Python decoder (`host_decoder.py`) reconstructs full color, microsecond-timestamped logs and telemetry over serial.
* **Dual-SDK Support**: Clean separation between OSAL, PAL, and core application code—build seamlessly for **ESP-IDF** (FreeRTOS) or **Zephyr RTOS** from the same codebase.
* **Automated CI & Testing**: Modular unit test engine and serial-based automated integration test suite (`autotest`).

## Documentation

For deeper technical details, refer to the project documentation:

### 🏗️ Architecture & Design
- **[Call Flow Diagrams](docs/uml_diag/meshx/meshx_call_flow.md)**: Detailed sequence diagrams of system initialization and messaging.
- **[Class Diagram](docs/uml_diag/meshx/meshx_class_diagram.md)**: Object-oriented structure of the MeshX stack.

### 📖 Guides
- **[Developer Guide: Custom Models & Elements](docs/DEVELOPER_GUIDE_CUSTOM_MODELS_AND_ELEMENTS.md)**: Extending the stack with custom BLE Mesh models.
- **[Unit Testing](main/component/unit_test/README.md)**: Running local and platform-level unit tests.
- **[Auto-test Framework](tools/scripts/autotest/README.md)**: Automated hardware-in-the-loop and serial-based testing.
