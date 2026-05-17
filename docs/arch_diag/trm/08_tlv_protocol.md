# Page 8 — Protocol Design: TLV Engine

> **[← HAL](./07_hal.md)** | **[← Index](../README.md)** | **[Next: Composition →](./09_composition.md)**

---

## 9. Protocol Design — TLV Engine

### 9.1 Frame Format

The UVP PDU is structured as a **fixed 4-byte header** followed by a variable-length TLV payload. Routing context (source address, destination address, and local target element index) is propagated alongside the frame, rather than wasting over-the-air bandwidth.

```
┌─────────────────────────────────────────────────────────────┐
│  BLE Mesh Vendor Model PDU                                  │
│  Opcode: 0xC0|OP, CID_L, CID_H  (3 bytes)                   │
├─────────────────────────────────────────────────────────────┤
│  UVP Header (4 bytes fixed)                                 │
│  ┌───────┬─────────┬───────┬────────────────────────┐       │
│  │ TID   │ ACK_REQ │ RFU   │ TYPE_ID                │       │
│  │ 1 B   │ 1 bit   │ 7 bits│ 2 B (little-endian)    │       │
│  └───────┴─────────┴───────┴────────────────────────┘       │
├─────────────────────────────────────────────────────────────┤
│  TLV Payload (variable, max 377 bytes)                      │
│  TLV Block 1: [Tag:1B][Len:1B][Value:N bytes]               │
│  TLV Block 2: [Tag:1B][Len:1B][Value:N bytes]               │
│  ...                                                        │
│  TLV Block N                                                │
└─────────────────────────────────────────────────────────────┘
```

### 9.2 UVP Header Fields

| Field | Size | Description |
|-------|------|-------------|
| **TID** | 1 B | Transaction ID — monotonically incrementing per source; used for duplicate suppression |
| **ACK_REQ** | 1 bit | Acknowledgment Request Flag — set to `1` by clients to request a unicast ACK response |
| **RFU** | 7 bits | Reserved for Future Use |
| **TYPE_ID** | 2 B | Element type/capability identifier (little-endian); carried here for type-safe dispatch |

> **Local Element Resolution**: To maximize the access payload budget, the over-the-air UVP header completely omits the target element index (`EL_IDX`). Instead, when a vendor message is received, the target element index (`rx_el_id`) is resolved locally from the low-level Bluetooth Mesh model instance index (`param->model_operation.model->element_idx`).

#### Header C Struct

```c
typedef struct {
    uint8_t tid;           /**< Transaction ID (0-255) */
    uint8_t ack_req     : 1; /**< ACK Requested Flag (1 = true) */
    uint8_t rfu         : 7; /**< Reserved for Future Use */
    uint16_t type_id;      /**< Function/Type ID (0-65535) */
} __attribute__((packed)) meshx_uvp_header_t;
```

### 9.3 Payload Size Budget

| Layer | Bytes |
|-------|-------|
| BLE Mesh SAR max (32 segments × 12 B) | 384 |
| Minus vendor opcode (3 B) | 381 |
| Minus UVP header (4 B) | **377 B available for TLV payload** |

> **Unsegmented fast path:** For single-segment messages (≤12 B access payload), the TLV payload is limited to **12 − 3 (opcode) − 4 (header) = 5 B**. Small state-only frames (e.g., OnOff = 3 B TLV) fit without SAR.

### 9.4 Global Tag Namespace

Tags carry **only functional state values** — element identity is in the header.

| Tag ID | Name | Data Type | Width | Description |
|--------|------|-----------|-------|-------------|
| `0x01` | `ONOFF` | `uint8_t` | 1 B | `0` = Off, `1` = On |
| `0x02` | `LEVEL` | `uint16_t` | 2 B | Linear `0–65535` |
| `0x03` | `LIGHTNESS` | `uint16_t` | 2 B | Light Lightness `0–65535` |
| `0x04` | `TEMPERATURE` | `uint16_t` | 2 B | Color Temperature (Kelvin encoded) |
| `0x05` | `DELTA_UV` | `int16_t` | 2 B | Delta UV offset |
| `0x06` | `HUE` | `uint16_t` | 2 B | HSL Hue `0–65535` |
| `0x07` | `SATURATION` | `uint16_t` | 2 B | HSL Saturation `0–65535` |
| `0x08` | `TRANSITION_TIME` | `uint8_t` | 1 B | Generic Mesh transition time encoding |
| `0x09` | `DELAY` | `uint8_t` | 1 B | 5ms increments |
| `0xFF` | `STATUS` | variable | — | Status/ACK response TLV container |

> **Removed:** Tag `0x10` (`EL_TYPE_ID`) is no longer a TLV tag. It is carried in the fixed 2-byte `TYPE_ID` header field, saving 6 B per PDU (2 B tag+len overhead + 4 B value → 2 B fixed).

### 9.5 Element Type IDs (TYPE_ID)

Carried in the UVP header `type_id` field. Allows the dispatcher and provisioner to identify the element role without any TLV parsing.

| TYPE_ID | Name | Element Role |
|------------|------|-------------|
| `0x0001` | `RELAY_SERVER` | Generic OnOff relay server |
| `0x0002` | `RELAY_CLIENT` | Generic OnOff relay client |
| `0x0003` | `LIGHT_CWWW_SERVER` | CW/WW (Cool White / Warm White) light server |
| `0x0004` | `LIGHT_CWWW_CLIENT` | CW/WW light client |
| `0x0005` | `SENSOR_SERVER` | Environmental sensor server |
| `0x0007` | `LIGHT_HSL_SERVER` | HSL (Hue-Saturation-Lightness) light server |

### 9.6 Transaction ID (TID) Mechanism

The `TID` in the UVP header is a per-source monotonic counter. The dispatcher tracks the last-seen TID per source address for duplicate suppression:

```
if (header.tid == last_tid[src_addr]) → DROP (duplicate)
else                                  → dispatch + update last_tid[src_addr]
```

This provides **idempotent message delivery** over the lossy BLE mesh layer (REQ-F17).

### 9.7 Segmentation

Payloads exceeding the 11-byte unsegmented MTU are automatically segmented by the ESP-BLE-MESH SAR layer. The UVP Dispatcher reassembles segments before header parsing and TLV dispatch. Maximum TLV payload: **377 bytes**.

### 9.8 Example: Atomic CWWW Update

Setting a CWWW Server element to 50% brightness at 4000K:

```
Opcode : 0xC0 0x37 0x13                          (3 bytes, vendor opcode)
Header : TID=0x42 ACK_REQ=0 RFU=0 TYPE_ID=0x0003 (4 bytes)
TLV[0] : Tag=0x01 Len=0x01 Val=0x01              (3 bytes) OnOff = ON
TLV[1] : Tag=0x03 Len=0x02 Val=0x7FFF            (4 bytes) Lightness = 50%
TLV[2] : Tag=0x04 Len=0x02 Val=0x0FA0            (4 bytes) Temperature = 4000K
TLV[3] : Tag=0x08 Len=0x01 Val=0x00              (3 bytes) Transition = immediate
─────────────────────────────────────────────────────────────
Total PDU: 3 (opcode) + 4 (header) + 14 (TLV) = 21 bytes
→ Fits in 2 BLE Mesh segments. No SAR needed for single-state frames ≤12B access.
```

*Note: The target element index is not transmitted over-the-air. The receiving node's BSP/Port layer resolves the receiving element index (e.g., `rx_el_id = 1`) based on the model instance that received the PDU.*

### 9.9 Client/Server Role Differentiation

In the standard SIG model architecture, roles are distinguished by using completely different Model IDs (e.g., `0x1000` for Generic OnOff Server vs `0x1001` for Generic OnOff Client). In the MeshX UVP architecture, **all** communication routes through the single UVP Vendor Model (`MESHX_MODEL_ID_UVP`).

The system determines whether a message is intended for a server or a client through three mechanisms:
1. **Element Topology (Local Knowledge):** When a message arrives, the UVP Dispatcher uses the locally-resolved receiving element index (`rx_el_id`) to route the message to a specific C++ Element instance on the node. The target element index is resolved locally from the low-level Bluetooth Mesh model instance index (`param->model_operation.model->element_idx`). The local element instance possesses the inherent knowledge of its own role (e.g., `meshx_relay_server` vs `meshx_relay_client`) and processes the incoming TLVs accordingly (as a command to change state, or as a status update to cache). This local dispatch logic is fully implemented in `meshx_uvp_dispatcher.cpp`.
2. **The `ack_req` Flag:** Only a client sending a request will set `ack_req = 1`. A server sending an ACK or Status back will always set `ack_req = 0`. An element receiving `ack_req == 1` knows it is processing a client request.
3. **The `STATUS` TLV Tag (`0xFF`):** If a message encapsulates its payload within a `STATUS` tag, it is definitively a response destined for a client.

---

> **[← HAL](./07_hal.md)** | **[← Index](../README.md)** | **[Next: Composition →](./09_composition.md)**
