# TRD-01 — System Context and Architecture

## 1. Overview

The MXCP (MeshX Command Protocol) framework replaces the current MXSP (MeshX Serial Protocol) 2-layer dispatch architecture with a **unified, single-layer, table-driven command/event model**. All telemetry between Host MCU and MeshX Engine flows through typed commands (Host → Engine) and typed events (Engine → Host), dispatched from flat lookup tables.

## 2. Architectural Diagram

```
  HOST MCU                          MESHX ENGINE
  ---------                         ------------
  [App Layer]                       [App / Control Task]
      |                                   |
  mxcp_send_cmd()                   mxcp_dispatch()
      |                                   |
  [MXCP Frame Builder]              [MXCP Frame Parser]
      |                                   |
  [UART TX]  ----[wire]---->        [UART RX State Machine]
                                      |
                                     mxcp_dispatch_frame()
                                      |
                               +------+------+
                               |             |
                        mxcp_cmd_table[]  mxcp_evt_table[]
                        (single lookup)    (event TX path)
                               |
                          handler(payload, len)
```

## 3. Key Architectural Changes

| Aspect | Before (MXSP) | After (MXCP) |
|--------|---------------|--------------|
| Dispatch | 2-level: `frame_dispatch[]` → `sys_cmd_dispatch[]` | Single flat: `mxcp_cmd_table[]` |
| Command ID | Split: MXSP_MSG_TYPE + ctrl_evt_id | Single flat `mxcp_cmd_id_t` enum |
| Events | Ad-hoc send functions (`mxsp_send_ctrl_event`, `mxsp_send_data_event`, `mxsp_send_gpio_rsp`, `mxsp_send_gpio_evt`) | Single `mxcp_send_event()` API |
| GPIO | Separate switch-case in `mxsp_handle_gpio_cmd()` | Integrated into command table |
| Header | `meshx_ctrl_msg_header_t` / `meshx_app_element_msg_header_t` embedded in payload | Unified in MXCP frame header |
| Frame format | `[SOF][LEN][TYPE][PAYLOAD][CHK][EOF]` | `[SOF][LEN][TYPE][PAYLOAD][CHK][EOF]` (same structure, TYPE encodes DIR+ID in 1 byte) |

*(REQ-001, REQ-004, REQ-005)*
