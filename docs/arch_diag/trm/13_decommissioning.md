# Page 13 — Decommissioning & Flash Reclamation

> **[← Hardware Platform](./12_hardware_platform.md)** | **[← Index](../README.md)** | **[Next: Error Codes →](./14_error_codes.md)**

---

## 14. Decommissioning & Flash Reclamation

### 14.1 SIG Application Models Removed

| Model Family | Models Decommissioned |
|---|---|
| **Generic** | OnOff Client/Server, Level Client/Server, Power OnOff Client/Server/Setup, Power Level Client/Server/Setup, Default Transition Time Client/Server, Battery Client/Server, Location Client/Server/Setup, Property Client, Admin/Manu/User/Client Property Servers |
| **Lighting** | Lightness Client/Server/Setup, CTL Client/Server/Setup/Temperature, HSL Client/Server/Setup/Hue/Saturation, XYL Client/Server, LC Client/Server/Setup |
| **Sensor** | Sensor Client/Server/Setup |
| **Time & Scene** | Time Client/Server/Setup, Scene Client/Server/Register |
| **Scheduler** | Scheduler Client/Server/Action |

### 14.2 Code Infrastructure Removed

| Component | Replacement |
|-----------|------------|
| `meshXBaseServerModel<T>` template | `meshXUVPElement` flat class |
| `meshXBaseClientModel<T>` template | `meshXUVPElement` flat class |
| Per-model message dispatchers | Single `meshXUVPDispatcher` |
| SIG model opcode tables | Single vendor opcode `0x1337` |

### 14.3 Retained SIG Models

| Model | Reason |
|-------|--------|
| **Config Server** | Required by BLE Mesh spec for key/subscription management |
| **Health Server** | Required by BLE Mesh spec for fault reporting |
| **Provisioning** (PB-ADV) | Standard provisioning bearer (kept) |

> **Note:** PB-GATT (Proxy) can be disabled via `sdkconfig` to reclaim additional flash when GATT proxy functionality is not needed.

### 14.4 Template Elimination Analysis

The core flash saving comes from removing C++ templates. Each instantiated template produced:

| Artifact | Overhead Per Template |
|----------|----------------------|
| vtable + RTTI | ~256–512 B |
| Opcode dispatch table | ~128–256 B |
| String literals (model names) | ~32–64 B |
| Per-state context struct | ~64–128 B |
| **Subtotal per model** | **~480–960 B** |

With ~40+ SIG model types eliminated across client/server variants: **~19–38 KB from template overhead alone**.

The remaining savings come from eliminating the SIG model implementation code (~130+ KB of `.rodata` and `.text` from the ESP-BLE-MESH SIG model library).

### 14.5 Files Targeted for Removal

```
main/component/meshx/ble_mesh/base_model/
    ├── src/meshx_base_model_light.cpp      ← REMOVE
    ├── src/meshx_base_model_generic.cpp    ← REMOVE
    └── inc/meshx_base_model_class.hpp      ← REMOVE (replace with UVP flat class)

main/component/meshx/ble_mesh/elements_c/
    ├── server/models/                      ← REMOVE all SIG server models
    └── client/models/base/                 ← REMOVE all SIG client models

sdkconfig (model feature flags)
    CONFIG_BLE_MESH_GENERIC_*=n             ← DISABLE
    CONFIG_BLE_MESH_LIGHTING_*=n            ← DISABLE
    CONFIG_BLE_MESH_SENSOR_*=n              ← DISABLE
    CONFIG_BLE_MESH_SCENE_*=n              ← DISABLE
```

---

> **[← Hardware Platform](./12_hardware_platform.md)** | **[← Index](../README.md)** | **[Next: Error Codes →](./14_error_codes.md)**
