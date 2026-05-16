# Page 10 — Hosted Mode & MXSP Serial Protocol

> **[← Composition](./09_composition.md)** | **[← Index](../design.md)** | **[Next: RO Config →](./11_ro_config.md)**

---

## 11. Hosted Mode & MXSP Serial Protocol

### 11.1 Hosted Mode Overview

When `CONFIG_MESHX_HOSTED_MODE` is enabled, the MeshX node acts as a **BLE Mesh co-processor**: all GPIO I/O and UVP events are forwarded to a host MCU over UART using the **MeshX Serial Protocol (MXSP)**.

```
Host MCU (Linux / STM32 / RPi)
      │  MXSP frames over UART
      ▼
ESP32-C3 (BLE Mesh Co-Processor)
      │  UVP over BLE Mesh
      ▼
Standard Nodes
```

### 11.2 MXSP Frame Format

```
┌──────────┬──────────┬───────────────┬──────────┬──────────┐
│ START    │ MSG_TYPE │ LENGTH (2B LE)│ PAYLOAD  │ CRC16    │
│ 0xAA 0x55│ 1 byte   │               │ N bytes  │ 2 bytes  │
└──────────┴──────────┴───────────────┴──────────┴──────────┘
```

| MSG_TYPE | Purpose |
|----------|---------|
| `0x01` | Data event (element state update from BLE) |
| `0x02` | Control event (system event: prov, reset) |
| `0x10` | UVP/TLV pass-through (REQ-F21) |
| `0x11` | GPIO hosted mode event |

### 11.3 Application Callback Integration

```c
// Application main.c — always forward to MXSP if in hosted mode
static meshx_err_t meshx_app_data_cb(...) {
    mxsp_send_data_event(msg_hdr, data_payload_u);  // MXSP bridge
    // ... local handling ...
}
static meshx_err_t meshx_app_ctrl_cb(...) {
    mxsp_send_ctrl_event(msg_hdr, msg);             // MXSP bridge
}
```

### 11.4 Hosted vs. Standalone Mode

| Feature | Standalone Mode | Hosted Mode |
|---------|----------------|-------------|
| GPIO control | Direct (silicon) | Serial (MXSP) |
| UVP event delivery | Application callback | MXSP `0x01` frame to host |
| NVS persistence | On-chip FAL | On-chip FAL (unchanged) |
| BLE Mesh stack | On ESP32-C3 | On ESP32-C3 (co-processor) |
| Host required | No | Yes (UART connected) |

### 11.5 MXSP Data Event Payload Structure

```c
typedef struct {
    meshx_app_element_msg_header_t hdr;  // element_idx, el_type_id, func_id
    union {
        meshx_relay_data_t       relay;
        meshx_cwww_data_t        cwww;
        meshx_sensor_data_t      sensor;
        meshx_hsl_data_t         hsl;
    } payload;
} mxsp_data_event_t;
```

---

> **[← Composition](./09_composition.md)** | **[← Index](../design.md)** | **[Next: RO Config →](./11_ro_config.md)**
