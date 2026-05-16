# Page 15 — Configuration Reference

> **[← Error Codes](./14_error_codes.md)** | **[← Index](../design.md)**

---

## 16. Configuration Reference

### 16.1 Configuration Ownership

With the RO config partition and Composition Builder in place, configuration responsibility is split across three sources. **There are no compile-time element flags.** All UVP element type handlers are compiled unconditionally into MeshX core; the Composition Builder selects which are instantiated at runtime.

| Configuration Item | Owner | Mechanism |
|--------------------|-------|-----------|
| Company ID (`cid`) | RO Config partition | Loaded at boot via `meshx_ro_cfg_init()` |
| Product ID (`pid`) | RO Config partition | Loaded at boot via `meshx_ro_cfg_init()` |
| Product Name | RO Config partition | Loaded at boot via `meshx_ro_cfg_init()` |
| Device UUID | RO Config partition | Loaded at boot; fallback derives from MAC |
| Element composition | Composition Builder | Registered via `meshx_builder_add_element()` |
| TXCM enable | Composition Builder | Auto-detected: enabled when any client element is registered |
| NVS commit period | `meshx_config_t` | Runtime field, default 1000 ms |
| App callbacks | `meshx_config_t` | Runtime field |
| Log level | `meshx_config_t` | Runtime field |

> **No per-element compile flags.** `CONFIG_RELAY_*_COUNT`, `CONFIG_LIGHT_*_COUNT`, `CONFIG_SENSOR_*_COUNT`, and `CONFIG_TXCM_ENABLE` are **removed**. The MeshX core always includes all element type implementations. The builder controls composition; the linker eliminates any unreachable code at link time.

### 16.2 Runtime Configuration (`meshx_config_t`)

The runtime config struct is intentionally minimal. Fields previously held here have been moved to their authoritative sources:

- **CID / PID / VID / product name** → RO config partition (`meshx_ro_cfg_init()`)
- **Element composition array** → Composition Builder (`meshx_builder_add_element()`)
- **UUID** → RO config partition (legacy `meshx_uuid_addr` field removed)

```c
typedef struct meshx_config {
    uint32_t meshx_nvs_save_period;     // NVS commit debounce (ms), default 1000
    meshx_app_data_cb_t app_element_cb; // Data event callback (element state)
    meshx_app_ctrl_cb_t app_ctrl_cb;    // Control event callback (prov, system)
    unsigned meshx_log_level;           // Log level (MESHX_LOG_INFO, etc.)
} meshx_config_t;
```

#### Typical Initialisation

```c
// 1. Register elements via Composition Builder
meshx_builder_add_element(MESHX_ELEMENT_TYPE_RELAY_SERVER, 1);
meshx_builder_add_element(MESHX_ELEMENT_TYPE_LIGHT_CWWW_SERVER, 1);

// 2. Populate the slim runtime config
meshx_config_t config = {
    .meshx_nvs_save_period = 1000,
    .app_element_cb        = my_data_callback,
    .app_ctrl_cb           = my_ctrl_callback,
    .meshx_log_level       = MESHX_LOG_INFO,
};

// 3. Init — RO config partition is read internally before BLE stack starts
meshx_init(&config);
```

### 16.3 sdkconfig Key Flags

Only ESP-BLE-MESH SIG model flags remain as sdkconfig concerns. MeshX-internal element selection is now fully runtime via the Composition Builder.

| Flag | Value | Purpose |
|------|-------|---------|
| `CONFIG_BLE_MESH_GENERIC_SERVER` | `n` | Disable SIG Generic server models (decommissioned) |
| `CONFIG_BLE_MESH_LIGHTING_SERVER` | `n` | Disable SIG Lighting server models (decommissioned) |
| `CONFIG_BLE_MESH_SENSOR_SERVER` | `n` | Disable SIG Sensor server models (decommissioned) |
| `CONFIG_BLE_MESH_SCENE_SERVER` | `n` | Disable SIG Scene/Scheduler models (decommissioned) |
| `CONFIG_BLE_MESH_GATT_PROXY_SERVER` | `n` | Disable PB-GATT proxy (optional, saves flash) |
| `CONFIG_BLE_MESH_HEALTH_SERVER` | `y` | Keep — required by BLE Mesh spec |
| `CONFIG_BLE_MESH_CFG_CLI` | `n` | Disable config client (node only) |
| `CONFIG_MESHX_HOSTED_MODE` | per BSP | Enable MXSP serial co-processor bridge |

### 16.4 Non-Functional Requirements Traceability

| REQ | Description | Design Mechanism |
|-----|-------------|-----------------|
| REQ-NF01 | ≥150 KB Flash reduction | SIG model decommissioning ([§13](./13_decommissioning.md)) |
| REQ-NF02 | <50ms processing latency | Control Task direct dispatch; no heap alloc in hot path |
| REQ-NF03 | Robust TLV parser | Bounds-checked length field; truncation detection |
| REQ-NF04 | Large payload segmentation | ESP-BLE-MESH SAR; max 377B TLV payload ([§8](./08_tlv_protocol.md)) |
| REQ-NF05 | Walled garden | Private vendor opcode; no SIG interoperability |

---

*End of Technical Reference Manual — MeshX UVP v2.0*

> **[← Error Codes](./14_error_codes.md)** | **[← Index](../design.md)**
