# TRD-07 — Design Decisions and Dependencies

## 1. Key Design Decisions and Trade-offs

### 1.1 Linear Search vs O(1) Index Lookup

**Decision:** Use linear scan of the command table.

**Rationale:** The table has ~20 entries. Linear scan is cache-friendly and fast for this size. O(1) indexed lookup would require dense IDs and adds complexity. If the table grows beyond ~50 entries, consider a sorted array with binary search.

### 1.2 8-bit TYPE with DIR in Bit 7

**Decision:** Keep the 8-bit TYPE field from MXSP. Encode direction in bit 7 (CMD=0, EVT=1), ID in bits 6-0.

**Rationale:** 7-bit ID space (128 values per direction) is more than sufficient for the current ~20 telemetry entries and decades of growth. This avoids any changes to the RX state machine — no new states, no new parsing logic. The frame format is wire-compatible in structure with MXSP, reducing migration risk.

### 1.3 Keep Existing App Callback API

**Decision:** `meshx_app_data_cb_t`, `meshx_app_ctrl_cb_t`, `meshx_send_msg_to_app()`, and `meshx_send_ctrl_msg_to_app()` remain unchanged.

**Rationale:** These are the application-facing APIs used by the control task and element callbacks. Changing them would cascade into the C++ element layer and application code. Only the internal serial layer changes; the external API stays stable.

### 1.4 GPIO Pin in Every Command Payload

**Decision:** Each GPIO command payload includes `logical_pin` as the first field rather than relying on a generic wrapper.

**Rationale:** This makes each command self-describing and handler-independent. The handler extracts the pin directly from its typed payload struct.

## 2. Dependencies and Integration Points

### 2.1 Internal Dependencies

| Component | Impact |
|-----------|--------|
| `meshx_serial.c` | Major rewrite: state machine, TX builder |
| `meshx_mxsp_cmd.c` | Complete replacement by `meshx_mxcp.c` |
| `meshx_api.h` | Internal wiring change: `meshx_send_msg_to_app()` → `mxcp_send_event()` |
| `meshx_composition_builder.cpp` | Minor: update struct references |
| Control Task | No change — uses `meshx_send_msg_to_app()` / `meshx_send_ctrl_msg_to_app()` which are preserved |
| UVP Dispatcher | No change — uses element callbacks |
| Element classes | No change — use `meshx_send_msg_to_app()` |

### 2.2 External Dependencies

- Platform serial write/read (`meshx_platform_serial_write`, `meshx_platform_serial_read`) — unchanged
- GPIO subsystem interface (`meshx_gpio_*`) — unchanged
- RTOS task/queue — unchanged

*(REQ-006)*
