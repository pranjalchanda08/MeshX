# Page 12 — Hardware Platform Reference

> **[← RO Config](./11_ro_config.md)** | **[← Index](../design.md)** | **[Next: Decommissioning →](./13_decommissioning.md)**

---

## 13. Hardware Platform Reference

### 13.1 Target Hardware — ESP32-C3

| Parameter | Value |
|-----------|-------|
| SoC | ESP32-C3 (RISC-V single core, 160 MHz) |
| Flash | 4 MB (QIO) |
| RAM | 400 KB |
| BLE | Bluetooth 5.0 (BLE Mesh via ESP-BLE-MESH) |
| Wi-Fi | 802.11 b/g/n (Router Node only) |
| SDK | ESP-IDF v5.4 |
| RTOS | FreeRTOS (ESP-IDF port) |

### 13.2 Supported BSPs

| BSP | Directory | Notes |
|-----|-----------|-------|
| `weact_c3` | `port/bsp/weact_c3/` | WeAct Studio ESP32-C3 module — primary dev board |
| `xiao_c3` | `port/bsp/xiao_c3/` | Seeed XIAO ESP32-C3 |
| `esp32_devkitC` | `port/bsp/esp32_devkitC/` | Espressif DevKit-C (for initial porting) |

### 13.3 Memory Budget (Post-UVP Decommissioning)

| Region | Before UVP | After UVP | Savings |
|--------|-----------|-----------|---------|
| Flash (code+rodata) | ~760 KB | ~590 KB | **≥170 KB** |
| Internal SRAM (data+bss) | ~123 KB | ~100 KB | **≥23 KB** |
| NVS Partition | 24 KB | 24 KB | — |
| meshx_cfg Partition | — | 4 KB | new |

### 13.4 Flash Partition Map (Typical)

| Partition | Type | Size | Purpose |
|-----------|------|------|---------|
| `factory` | app | 1.5 MB | Main firmware |
| `ota_0` | app | 1.5 MB | OTA slot |
| `nvs` | data/nvs | 24 KB | ESP-IDF NVS (BLE Mesh keys, provisioning) |
| `meshx_kv` | data | 64 KB | MeshX KV Engine (element state) |
| `meshx_cfg` | data | 4 KB | Read-only product configuration (Nanopb) |

### 13.5 FreeRTOS Task Map

| Task | Priority | Stack | Purpose |
|------|----------|-------|---------|
| `meshx_control_task` | `TIMER_PRIO + 2` | 8192 B | Central event dispatcher |
| `meshx_log_task` | `TIMER_PRIO + 1` | 4096 B | Async threaded logger |
| `esp_timer` | `TIMER_PRIO` | — | ESP-IDF software timers |
| `btController` | high | — | BLE HCI controller (ESP-IDF) |
| `nimble_host` | — | — | NimBLE host stack |

### 13.6 Flash Memory Map (Visual)

```
0x000000  ┌────────────────────────────┐
          │  Bootloader (32 KB)        │
0x008000  ├────────────────────────────┤
          │  Partition Table (4 KB)    │
0x009000  ├────────────────────────────┤
          │  NVS (24 KB)               │  ← BLE Mesh keys, prov state
0x00F000  ├────────────────────────────┤
          │  meshx_cfg (4 KB)          │  ← Nanopb product config
0x010000  ├────────────────────────────┤
          │  Factory App (1.5 MB)      │  ← Main firmware
0x190000  ├────────────────────────────┤
          │  OTA_0 (1.5 MB)            │  ← OTA update slot
0x310000  ├────────────────────────────┤
          │  meshx_kv (64 KB)          │  ← KV Engine (element state)
0x3F0000  └────────────────────────────┘
```

---

> **[← RO Config](./11_ro_config.md)** | **[← Index](../design.md)** | **[Next: Decommissioning →](./13_decommissioning.md)**
