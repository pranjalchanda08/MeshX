# MeshX Technical Reference Manual

**Version:** 3.0  
**Platform:** ESP32-C3 · ESP-IDF v5.4 · ESP-BLE-MESH  
**Classification:** User-Facing Reference Documentation  
**Status:** Active

---

## What Is MeshX?

**MeshX** is an embedded BLE Mesh firmware platform for ESP32-C3 targets that consolidates all application-level device control into a single, composable, Unified Vendor Protocol (UVP). It replaces the SIG model stack (Generic, Lighting, Sensor, Scene) to reclaim **≥170 KB Flash** and **≥23 KB RAM**, while providing a clean binary protocol interface (`MXCP`) for host-side automation, testing, and web-based consoles.

---

## Document Map

| # | Page | What You Will Learn |
|---|------|---------------------|
| **1** | [System Overview & Layered Architecture](01_system_overview.md) | Node roles, inbound/outbound data flow, UVP stack diagram |
| **2** | [System Boot & Initialization Sequence](02_boot_sequence.md) | `meshx_init()` phases, RO-config loading, element composition |
| **3** | [C++ Class Architecture](03_cpp_architecture.md) | Flat dispatcher pattern, `meshXUVPElement`, model hierarchy |
| **4** | [Control Task — Central Message Bus](04_control_task.md) | Pub-sub queue, message codes, task configuration |
| **5** | [TXCM — Transmission Control Module](05_txcm.md) | Reliability, retry state machine, timeout handling |
| **6** | [State Persistence Strategy](06_state_persistence.md) | KV engine, NVS commit, key format, flash layout |
| **7** | [Hardware Abstraction Layer (HAL)](07_hal.md) | GPIO/PWM abstraction, hosted mode I/O, class hierarchy |
| **8** | [UVP Protocol & TLV Engine](08_tlv_protocol.md) | Frame format, 4-byte header, tag namespace, payload budget |
| **9** | [Composition & Element Discovery](09_composition.md) | Builder API, `prod_profile.yml`, EL_TYPE_ID assignment |
| **10** | [MXCP — Binary Host Protocol](10_mxcp_protocol.md) | Frame structure, command/event namespace, dispatch table |
| **11** | [Read-Only Persistent Configuration](11_ro_config.md) | `meshx_cfg` partition, Nanopb/Protobuf layout, fallback flow |
| **12** | [Hardware Platform Reference](12_hardware_platform.md) | ESP32-C3 specs, BSPs, memory budget, partition map |
| **13** | [Decommissioning & Flash Reclamation](13_decommissioning.md) | SIG models removed, template elimination analysis |
| **14** | [Error Codes Reference](14_error_codes.md) | All error codes by domain, error handling pattern |
| **15** | [Configuration Reference](15_configuration.md) | Compile-time macros, runtime config struct, NFR traceability |
| **16** | [Memory Impact Analysis](16_memory_impact.md) | Post-refactor Flash/RAM savings, template elimination |
| **17** | [Logical Models & MXCP Testing](17_logical_models_testing.md) | `device.py` interfaces, L0 test suite, binary frame injection |
| **18** | [Web Console & USB CDC Multiplexing](18_web_console.md) | Dynamic stream routing, host demux parser, test UI |
| **19** | [ELF-Based Binary Logging](19_elf_logging.md) | Non-loadable log section, TLV encoding, host-side decoder |
| **20** | [UVP Routing & ACK Architecture](20_uvp_routing.md) | `meshx_uvp_ctx_t`, dual-routing, ACK request flag |

---

## Quick Reference

### MXCP Frame Format

```
 Byte 0:    SOF = 0xFE
 Byte 1:    LEN (payload length, 0–255)
 Byte 2:    TYPE  →  Bit 7: 0=CMD / 1=EVT  |  Bits 6-0: ID
 Byte 3..N: PAYLOAD (typed struct)
 Byte N+1:  CHECKSUM = LEN ^ TYPE ^ (all payload bytes XOR)
 Byte N+2:  EOF = 0xEF
```

### Key Command IDs

| CMD TYPE | ID | Description |
|----------|----|-------------|
| `MXCP_CMD_HOSTED_MODE_ENABLE` | `0x01` | Activate binary MXCP transport |
| `MXCP_CMD_NODE_RESET` | `0x02` | Reset the mesh node |
| `MXCP_CMD_GET_COMPOSITION` | `0x03` | Query element composition |
| `MXCP_CMD_EL_SEND` | `0x10` | Inject element command (8-byte header + payload) |
| `MXCP_CMD_GPIO_SET_LEVEL` | `0x21` | Set GPIO pin level |

### Key Event IDs (bit 7 set on wire)

| EVT TYPE | ID | Description |
|----------|----|-------------|
| `MXCP_EVT_EL_DATA_NOTIFY` | `0x10` (→ `0x90` on wire) | Asynchronous element state telemetry |
| `MXCP_EVT_PROV_COMP` | `0x01` (→ `0x81`) | Provisioning complete |
| `MXCP_EVT_GPIO_ASYNC` | `0x3E` (→ `0xBE`) | Interrupt-driven GPIO event |

### UVP TLV Tag Namespace

| Tag | Name | Type | Width |
|-----|------|------|-------|
| `0x01` | ONOFF | `uint8_t` | 1 B |
| `0x02` | LEVEL | `uint16_t` | 2 B |
| `0x03` | LIGHTNESS | `uint16_t` | 2 B |
| `0x04` | TEMPERATURE | `uint16_t` | 2 B |
| `0x06` | HUE | `uint16_t` | 2 B |
| `0x07` | SATURATION | `uint16_t` | 2 B |
| `0x10` | EL_TYPE_ID | `uint32_t` | 4 B |
| `0xFF` | STATUS | variable | — |

### Memory Savings Summary

| Region | Before | After | Saved |
|--------|--------|-------|-------|
| Flash | ~760 KB | ~590 KB | **≥170 KB** |
| SRAM | ~123 KB | ~100 KB | **≥23 KB** |

### Element Composition (all_in_one)

| Element ID | Variant | Role |
|-----------|---------|------|
| 1 | `RELAY_SERVER` | Server — drives relay GPIO |
| 2 | `RELAY_CLIENT` | Client — forwards host commands over BLE |
| 3 | `LIGHT_CWWW_SERVER` | Server — drives PWM (warm/cool white) |
| 4 | `LIGHT_CWWW_CLIENT` | Client |
| 5 | `SENSOR_SERVER` | Server — reports sensor readings |
| 6 | `SENSOR_CLIENT` | Client |
| 7 | `LIGHT_HSL_SERVER` | Server — drives HSL colour |
| 8 | `LIGHT_HSL_CLIENT` | Client |

---

## Getting Started

### 1. Build Firmware

```bash
source tools/scripts/env.sh source /path/to/esp-idf/export.sh
python3 tools/scripts/meshx.py -B=xiao_c3 -N=all_in_one -b
```

### 2. Flash Device

```bash
python3 tools/scripts/meshx.py -B=xiao_c3 -N=all_in_one -f -P /dev/ttyACM0
```

### 3. Run L0 Tests via MXCP

```bash
python3 tools/scripts/autotest/runner.py -b xiao_c3:/dev/ttyACM0 --skip-flash
```

### 4. Enable Hosted Mode Manually (Serial Shell)

```
ut 8 1 1 1
```

After this command, the node accepts binary MXCP frames over the same USB CDC port.

---

*Start reading: [Page 1 — System Overview →](01_system_overview.md)*
