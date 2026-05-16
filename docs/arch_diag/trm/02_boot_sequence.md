# Page 2 — System Boot & Initialization Sequence

> **[← System Overview](./01_system_overview.md)** | **[← Index](../README.md)** | **[Next: C++ Architecture →](./03_cpp_architecture.md)**

---

## 3. System Boot & Initialization Sequence

The `meshx_init()` function (in `main/component/meshx/src/meshx.c:304`) orchestrates the full boot sequence in strict dependency order:

```mermaid
sequenceDiagram
    participant APP as "app_main()"
    participant MX  as "meshx_init()"
    participant PLT as "Platform Layer"
    participant LOG as "Logging Subsystem"
    participant RO  as "RO Config (Flash Partition)"
    participant CTL as "Control Task"
    participant TMR as "OS Timer"
    participant NVS as "NVS Subsystem"
    participant BLE as "BLE Mesh Stack"
    participant SER as "MXSP Serial"

    APP->>MX: meshx_init(&meshx_config)
    MX->>PLT: meshx_platform_init()
    MX->>LOG: meshx_logging_init()
    MX->>RO: meshx_load_persistent_config()
    Note over RO: Reads meshx_cfg partition<br/>Loads CID/PID/UUID/Name<br/>Falls back to defaults on error
    MX->>CTL: control_task_init()
    Note over CTL: Initialises message queue<br/>(required before NVS/Timers)
    MX->>TMR: meshx_os_timer_init()
    MX->>NVS: meshx_nvs_init() + meshx_dev_restore()
    Note over NVS: Opens NVS namespace<br/>Restores net_key_id, node_addr
    MX->>MX: meshx_tasks_init() → create_control_task() + meshx_txcm_init()
    MX->>MX: meshx_app_reg_element_callback()
    MX->>MX: meshx_app_reg_system_events_callback()
    MX->>BLE: meshx_ble_mesh_init()
    Note over BLE: meshx_element_init() → builder_bake()<br/>meshx_init_prov()<br/>meshx_plat_ble_mesh_init()<br/>Publishes STACK_READY event
    MX->>SER: meshx_serial_init()
    MX-->>APP: MESHX_SUCCESS
```

### 3.1 Boot Phase Summary

| Phase | Function | Notes |
|-------|----------|-------|
| **P1** | `meshx_platform_init()` | Initializes ESP-IDF hardware peripherals |
| **P2** | `meshx_logging_init()` | Sets up threaded async log task |
| **P3** | `meshx_load_persistent_config()` | Loads CID/PID/UUID from `meshx_cfg` flash partition; falls back to compile-time defaults |
| **P4** | `control_task_init()` | Allocates FreeRTOS message queue (must precede NVS & timers) |
| **P5** | `meshx_os_timer_init()` | Initializes software timer subsystem |
| **P6** | `meshx_nvs_init()` + `meshx_dev_restore()` | Opens NVS namespace, restores `net_key_id` and `node_addr` |
| **P7** | `meshx_tasks_init()` | Creates Control Task + initializes TXCM |
| **P8** | `meshx_ble_mesh_init()` | Bakes element composition, initializes provisioning, starts BLE stack |
| **P9** | `meshx_serial_init()` | Starts MXSP serial host bridge (if enabled) |

### 3.2 Fresh Boot vs. Provisioned Boot

```mermaid
flowchart TD
    A[Power On] --> B{meshx_cfg partition valid?}
    B -- Yes --> C[Load CID / PID / UUID from flash]
    B -- No --> D[Use compile-time defaults from meshx_config.h]
    C --> E{NVS net_key_id != 0?}
    D --> E
    E -- Yes --> F[Provisioned Boot: Restore mesh state]
    E -- No --> G[Fresh Boot: Start 1500ms identity beacon timer]
    F --> H[Publish STACK_READY → Control Task]
    G --> H
    H --> I[Start accepting BLE Mesh PDUs]
```

---

> **[← System Overview](./01_system_overview.md)** | **[← Index](../README.md)** | **[Next: C++ Architecture →](./03_cpp_architecture.md)**
