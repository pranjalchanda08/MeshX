# TRD-08 — Host-Side Tooling Migration

The Python web console (`tools/web_console/server/`) must be updated alongside the engine firmware to use the new MXCP TYPE encoding. The frame structure (`[SOF][LEN][TYPE][PAYLOAD][CHK][EOF]`) is unchanged, but the TYPE byte semantics and payload layouts change.

## 1. `demux.py` — No Changes Required

The `StreamDemultiplexer` parses frames by `0xFE` SOF anchor, extracts `payload_len` from byte 1, validates checksum and `0xEF` EOF. It delivers `(msg_type, payload)` to callbacks. Since the frame structure is identical, **no changes are needed** in `demux.py`. The `msg_type` it delivers will simply carry the new MXCP TYPE values instead of the old MXSP values.

## 2. `server.py` — Required Changes

### 2.1 `send_cmd()` Frame Builder

The frame builder is already correct — it builds `[0xFE][length][msg_type][payload][checksum][0xEF]`. The only change is what `msg_type` values the callers pass. No code change in `send_cmd()` itself.

### 2.2 Hosted Mode Activation (`start()`, `resume()`, `enable_mxcp()`)

Currently sends `0x03` (old `MXSP_MSG_TYPE_HOSTED_MODE`). Must send new `MXCP_CMD_HOSTED_MODE_ENABLE` = `0x01`:

```python
# Before:
self.send_cmd(0x03, bytes([0x01]))

# After:
self.send_cmd(0x01, bytes([0x01]))  # MXCP_CMD_HOSTED_MODE_ENABLE
```

### 2.3 GPIO Commands (`send_gpio_command()`, WebSocket handlers)

Currently sends `0xD1` (old `MXSP_MSG_TYPE_GPIO_CMD`) with old `mxsp_gpio_cmd_payload_t` layout. Must use new per-command IDs with new typed payloads:

```python
# Before:
worker.send_cmd(0xD1, bytes(payload))  # Old: cmd + pin + reserved + len + data

# After — each GPIO command is a separate MXCP CMD ID:
CMD_GPIO_SET_LEVEL    = 0x21
CMD_GPIO_GET_LEVEL    = 0x22
CMD_GPIO_TOGGLE       = 0x23
CMD_GPIO_SET_PWM_DUTY = 0x24
CMD_GPIO_SET_PWM_FREQ = 0x25
CMD_GPIO_INTR_ENABLE  = 0x26
CMD_GPIO_GET_CONFIG   = 0x28
CMD_GPIO_GET_STATE    = 0x29

# Example: SET_LEVEL
payload = struct.pack("<BB", pin, value)
worker.send_cmd(CMD_GPIO_SET_LEVEL, payload)
```

The WebSocket JSON handler for `"type": "gpio"` must map the `cmd` field to the correct MXCP command ID and pack the appropriate typed payload.

### 2.4 GPIO Response Parsing (`handle_mxsp()`)

Currently matches `msg_type == 0xD2` and `0xD3`. Must match new EVT TYPE values:

```python
# Before:
if msg_type == 0xD2:  # GPIO Response
elif msg_type == 0xD3:  # GPIO Event

# After:
EVT_GPIO_SET_LEVEL_RSP    = 0x80 | 0x21  # 0xA1
EVT_GPIO_GET_LEVEL_RSP    = 0x80 | 0x22  # 0xA2
EVT_GPIO_TOGGLE_RSP       = 0x80 | 0x23  # 0xA3
EVT_GPIO_GET_CONFIG_RSP   = 0x80 | 0x28  # 0xA8
EVT_GPIO_GET_STATE_RSP    = 0x80 | 0x29  # 0xA9
EVT_GPIO_ASYNC            = 0x80 | 0x3E  # 0xBE

GPIO_RSP_EVT_IDS = {0xA1, 0xA2, 0xA3, 0xA4, 0xA5, 0xA6, 0xA7, 0xA8, 0xA9}

if msg_type in GPIO_RSP_EVT_IDS:
    # Parse mxcp_evt_gpio_rsp_t: status(1), pin(1), response_len(1), response(N)
    status, pin, rlen = struct.unpack("<BBB", payload[:3])
    ...
elif msg_type == EVT_GPIO_ASYNC:
    # Parse mxcp_evt_gpio_async_t: event_type(1), pin(1), value(1), reserved(1), timestamp(4)
    ...
```

### 2.5 System Event Parsing (`handle_mxsp()`)

Currently matches `msg_type == 0xB1` (SYS_EVT_NOTIFY) with embedded `meshx_ctrl_msg_header_t`. Must match new EVT TYPE values — the evt_id is no longer embedded in the payload; it IS the TYPE byte:

```python
# Before:
elif msg_type == 0xB1:  # System Event Notify
    evt_id, reserved = struct.unpack("<HH", payload[:4])
    if evt_id == 0x07:  # GET_COMPOSITION
        ...

# After:
EVT_COMPOSITION_RSP    = 0x80 | 0x06  # 0x86
EVT_ELEMENT_STATE_RSP  = 0x80 | 0x07  # 0x87
EVT_EL_DATA_NOTIFY     = 0x80 | 0x10  # 0x90

if msg_type == EVT_COMPOSITION_RSP:
    # Payload is mxcp_evt_composition_rsp_t directly (no ctrl header)
    num_elements = payload[0]
    offset = 1
    ...

elif msg_type == EVT_ELEMENT_STATE_RSP:
    # Payload is mxcp_evt_element_state_rsp_t directly
    ...

elif msg_type == EVT_EL_DATA_NOTIFY:
    # Payload is mxcp_evt_el_data_notify_t directly
    el_id, el_type, func_id, msg_len = struct.unpack("<HHHH", payload[:8])
    data = payload[8:]
    ...
```

### 2.6 Composition / System Command Requests

Currently sends `0xC2` (SYS_CMD_SEND) with embedded `meshx_ctrl_msg_header_t`. Must use new flat CMD IDs with no embedded header:

```python
# Before:
payload = struct.pack("<HH", 0x07, 0x00)  # evt_id=GET_COMPOSITION + reserved
worker.send_cmd(0xC2, payload)

# After — no payload header, CMD ID carries the semantics:
worker.send_cmd(0x03, b'')  # MXCP_CMD_GET_COMPOSITION, no payload
```

### 2.7 Element Commands (WebSocket `"el_cmd"` handlers)

Currently sends `0xC1` with `meshx_app_element_msg_header_t` embedded. Must use new `MXCP_CMD_EL_SEND`:

```python
# Before:
worker.send_cmd(0xC1, payload)  # MXSP_MSG_TYPE_EL_CMD_SEND

# After:
worker.send_cmd(0x10, payload)  # MXCP_CMD_EL_SEND (same payload layout)
```

## 3. Summary of TYPE Value Mapping

| Old MXSP TYPE | Usage | New MXCP TYPE | Notes |
|---------------|-------|---------------|-------|
| `0x03` | Hosted mode enable | `0x01` | `MXCP_CMD_HOSTED_MODE_ENABLE` |
| `0xC2` + payload `{0x07, 0x00}` | Get composition | `0x03` | No embedded header, ID in TYPE byte |
| `0xC2` + payload `{0x09, 0x00}` | Get element state | `0x04` | No embedded header |
| `0xC1` | Element command | `0x10` | `MXCP_CMD_EL_SEND`, same payload |
| `0xD1` + cmd in payload | GPIO command | `0x21`-`0x29` | Per-command ID, new typed payload |
| `0xD2` | GPIO response | `0xA1`-`0xA9` | Per-response EVT ID, bit 7 set |
| `0xD3` | GPIO async event | `0xBE` | `EVT_GPIO_ASYNC` |
| `0xB1` + evt_id in payload | System event | `0x81`-`0x8A` | evt_id removed from payload, now in TYPE byte |
| `0xB2` + header in payload | Data event | `0x90` | `EVT_EL_DATA_NOTIFY`, header removed from payload |

## 4. Files Affected

| File | Change |
|------|--------|
| `tools/web_console/server/demux.py` | **None** — frame structure unchanged |
| `tools/web_console/server/server.py` | Update `send_cmd()` callers: hosted mode TYPE, GPIO command IDs and payloads, response EVT parsing, system event parsing, composition request format, element command TYPE |
