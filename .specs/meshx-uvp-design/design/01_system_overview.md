# Page 1 — System Overview & Layered Architecture

> **[← Index](../design.md)** | **[Next: Boot Sequence →](./02_boot_sequence.md)**

---

## 1. System Overview

The **MeshX Unified Vendor Protocol (UVP)** is a high-density, walled-garden communication architecture that consolidates all application-level state control into a single BLE Mesh Vendor Model per element. It replaces the entire SIG model stack (Generic, Lighting, Sensor, Scene) to reclaim ≥150 KB Flash and ≥23 KB RAM on the ESP32-C3 target.

The design distinguishes between two node roles:

| Role | Transport | Responsibilities |
|------|-----------|-----------------|
| **Standard Node** | BLE Mesh only | Receives UVP frames, drives GPIO/PWM, persists state to NVS |
| **Router Node** | BLE Mesh + Wi-Fi | Acts as TCP-to-UVP gateway; bridges Cloud/MQTT to mesh |

### 1.1 Architectural Stack

```mermaid
graph TD
    subgraph "MeshX Router (Gateway)"
        Cloud["Cloud / MQTT / TCP"] <--> TCP_Relay["TCP-to-UVP Relay"]
        TCP_Relay <--> R_UVP["UVP Dispatcher"]
    end

    subgraph "Standard Node (BLE Only)"
        S_UVP["UVP Dispatcher"] --> HAL["GPIO / PWM Drivers"]
        S_UVP <--> KV["MeshX KV Engine (NVS)"]
        S_UVP --> MXSP["MXSP Serial Host (optional)"]
    end

    R_UVP <== "BLE Mesh (UVP Opcode 0x1337)" ==> S_UVP
```

### 1.2 Key Design Principles

| Principle | Implementation |
|-----------|---------------|
| **Single Vendor Model per Element** | One model handles all functional states (REQ-F01) |
| **Atomic Multi-State Updates** | Single radio PDU carries multiple TLV tags (REQ-F03) |
| **Global Tag Namespace** | Each Tag ID has a fixed definition across the ecosystem (REQ-F24) |
| **Extensibility** | 1-byte Tag space → 256 unique tags (REQ-F05) |
| **Reliability** | All transmissions routed through TXCM (REQ-F18) |
| **Walled Garden** | Protocol is intentionally private to MeshX (REQ-NF05) |

---

## 2. Detailed Layered Architecture

### 2.1 Layer Definition

| Layer | Component | Responsibility |
|-------|-----------|----------------|
| **L7: Application** | `main.c` | Consumes `meshx_app_data_cb` / `meshx_app_ctrl_cb`; forwards to MXSP |
| **L6: MeshX API** | `meshx_api.c` | `meshx_send_msg_to_app()`, `meshx_send_ctrl_msg_to_app()` — bridge to Control Task |
| **L5: Dispatcher** | `meshx_uvp_dispatcher.c` | TLV parsing and element-specific TagHandler routing |
| **L4: Port** | `meshx_port_ble_mesh.c` | Translates `esp_ble_mesh_msg_t` ↔ MeshX UVP frames |
| **L3: Stack** | `esp_ble_mesh_core` | BLE Mesh Network / Transport layer (ESP-IDF managed) |
| **L2: HAL** | `meshx_gpio.c` / `meshx_pwm.c` | Platform-abstracted GPIO bit-bang and PWM duty control |
| **L1: Persistence** | `meshx_kv_engine.c` / `meshx_nvs.c` | Non-volatile element state storage via FAL interface |

### 2.2 Inbound Control & Persistence Sequence

```mermaid
sequenceDiagram
    participant STK as ESP-BLE-MESH
    participant PRT as meshx_port
    participant DSP as UVP Dispatcher
    participant KV as KV Engine
    participant HAL as GPIO/PWM HAL
    participant APP as Application

    Note over STK: Incoming BLE PDU Opcode 0x1337
    STK->>PRT: meshx_vnd_model_cb()
    PRT->>DSP: meshx_uvp_process_payload()

    loop Every TLV Tag in Payload
        DSP->>DSP: parse_tlv(tag, len, val)
        DSP->>HAL: meshx_hw_update(tag, val)
        DSP->>KV: meshx_kv_engine_set(key, val)
        Note right of KV: Buffered - committed on NVS timer
    end

    DSP->>PRT: prepare_status_tlv()
    PRT->>STK: esp_ble_mesh_send_msg via TXCM
    STK-->>APP: meshx_app_data_cb(el_type, func_id, payload)
```

### 2.3 Outbound Command Sequence (Client → Server)

```mermaid
sequenceDiagram
    participant APP as Application
    participant API as meshx_api
    participant CTL as Control Task
    participant TXCM as TXCM Module
    participant STK as BLE Stack

    APP->>API: meshx_send_msg_to_element(el_id, type, func_id, payload)
    API->>CTL: control_task_msg_publish(TO_BLE, SET_ON_OFF)
    CTL->>TXCM: meshx_txcm_request_send(msg, retries)
    TXCM->>STK: esp_ble_mesh_client_model_send_msg()
    STK-->>TXCM: ACK or Timeout
    TXCM-->>CTL: TXCM_MSG_TIMEOUT if no ACK
    CTL-->>APP: meshx_app_data_cb(CLIENT_EVT, err_code)
```

---

> **[← Index](../design.md)** | **[Next: Boot Sequence →](./02_boot_sequence.md)**
