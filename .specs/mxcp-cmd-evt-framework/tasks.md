# MXCP Framework — Task Breakdown

## Wave 0 — Foundation (no dependencies)

### TASK-001: Create `meshx_mxcp.h` — Types, Enums, and Payload Structures
- **Linked REQs:** REQ-001, REQ-002, REQ-003, REQ-005
- **Linked TRDs:** TRD-02, TRD-03, TRD-04
- **Files:** `common/inc/meshx_mxcp.h` (new)
- **Description:** Create the public MXCP header with frame format macros (`MXCP_TYPE_DIR_CMD`, `MXCP_TYPE_DIR_EVT`, `MXCP_MAKE_TYPE`, etc.), `mxcp_frame_t`, `mxcp_cmd_id_t` and `mxcp_evt_id_t` enums, all typed payload structures (`mxcp_cmd_*_t`, `mxcp_evt_*_t`), table entry types (`mxcp_cmd_entry_t`, `mxcp_evt_entry_t`), `MXCP_CMD_ENTRY` macro, and API declarations (`mxcp_send_event`, `mxcp_send_cmd`, `mxcp_dispatch_frame`, individual handler prototypes).
- **Complexity:** L
- **Dependencies:** None
- **Wave:** 0

### TASK-002: Update `module_id.h` — No changes needed
- **Linked REQs:** N/A
- **Description:** Verify no changes needed. Placeholder task for completeness — currently no new module IDs are required for MXCP.
- **Complexity:** S
- **Dependencies:** None
- **Wave:** 0

## Wave 1 — Core Engine Implementation (depends on TASK-001)

### TASK-003: Create `meshx_mxcp.c` — Tables, Dispatch, TX API, Command Handlers
- **Linked REQs:** REQ-001, REQ-002, REQ-004, REQ-005, REQ-007, REQ-008
- **Linked TRDs:** TRD-05, TRD-06
- **Files:** `common/src/meshx_mxcp.c` (new)
- **Description:** Implement the MXCP core source file containing: `mxcp_cmd_table[]` and `mxcp_evt_table[]` definitions, `mxcp_dispatch_frame()` single-layer dispatch, `mxcp_send_event()` and `mxcp_send_cmd()` frame TX functions, `mxcp_send_frame()` internal frame builder (replaces `mxsp_send_frame`), and all individual command handlers (`mxcp_cmd_fn_hosted_mode`, `mxcp_cmd_fn_node_reset`, `mxcp_cmd_fn_get_composition`, `mxcp_cmd_fn_get_element_state`, `mxcp_cmd_fn_set_console_routing`, `mxcp_cmd_fn_el_send`, and all 9 GPIO handlers). GPIO handlers call `meshx_gpio_*` APIs directly (no switch-case) and send responses via `mxcp_send_event()`. Handlers for GET_COMPOSITION and GET_ELEMENT_STATE call `meshx_get_element_composition_data()` and `meshx_get_element_state_data()` respectively.
- **Complexity:** XL
- **Dependencies:** TASK-001
- **Wave:** 1

### TASK-004: Refactor `meshx_serial.h` — Remove Old MXSP Types, Include MXCP
- **Linked REQs:** REQ-003, REQ-006
- **Linked TRDs:** TRD-02, TRD-06
- **Files:** `common/inc/meshx_serial.h` (modified)
- **Description:** Remove `mxsp_msg_type_t`, `mxsp_gpio_cmd_t`, `mxsp_gpio_evt_t`, all old GPIO payload structs (`mxsp_gpio_cmd_payload_t`, `mxsp_gpio_rsp_payload_t`, `mxsp_gpio_evt_payload_t`, `mxsp_gpio_cmd_data_t`, `mxsp_gpio_rsp_data_t` and their sub-structs), `mxsp_comp_entry_header_t`, `mxsp_state_entry_header_t`, and `mxsp_frame_t`. Remove `mxsp_send_frame()`, `mxsp_send_ctrl_event()`, `mxsp_send_data_event()`, `mxsp_send_gpio_rsp()`, `mxsp_send_gpio_evt()`. Add `#include "meshx_mxcp.h"`. Retain `meshx_serial_init()`, `meshx_serial_set_hosted_mode()`, `meshx_serial_is_hosted_mode_enabled()`. Keep `MXSP_SOF`/`MXSP_EOF`/`MXSP_PAYLOAD_MAX_SIZE` as aliases or remove if redundant with MXCP defines.
- **Complexity:** M
- **Dependencies:** TASK-001
- **Wave:** 1

## Wave 2 — Serial Layer Rewiring (depends on Wave 1)

### TASK-005: Refactor `meshx_serial.c` — Use MXCP Frame TX and Updated Includes
- **Linked REQs:** REQ-003, REQ-006
- **Linked TRDs:** TRD-02, TRD-05, TRD-06
- **Files:** `common/src/meshx_serial.c` (modified)
- **Description:** Update includes to use `meshx_mxcp.h` instead of `meshx_mxsp_cmd.h`. Replace `mxsp_send_frame()` calls in `mxsp_send_ctrl_event()`, `mxsp_send_data_event()`, `mxsp_send_gpio_rsp()`, `mxsp_send_gpio_evt()` — these functions become thin wrappers that call `mxcp_send_event()` internally. Remove `mxsp_send_frame()`; all TX now goes through `mxcp_send_event()` or `mxcp_send_cmd()`. Update `mxsp_uart_rx_task` — the state machine itself is unchanged but on valid frame completion, call `mxcp_dispatch_frame(type, payload, len)` instead of `mxsp_dispatch_frame()`. Update `meshx_gpio_hosted_event_handler` to call `mxcp_send_event(MXCP_EVT_GPIO_ASYNC, ...)`. Remove `calculate_checksum()` if now in `meshx_mxcp.c`. Keep `meshx_serial_init()`, `meshx_serial_set_hosted_mode()`, `meshx_serial_is_hosted_mode_enabled()`, `meshx_serial_parse_byte()` (state machine unchanged).
- **Complexity:** L
- **Dependencies:** TASK-003, TASK-004
- **Wave:** 2

### TASK-006: Update `meshx_api.h` — Wire Internal Send Functions to MXCP
- **Linked REQs:** REQ-008
- **Linked TRDs:** TRD-05, TRD-06, TRD-07
- **Files:** `inc/meshx_api.h` (modified) + its implementation source
- **Description:** The public API (`meshx_send_msg_to_app()`, `meshx_send_ctrl_msg_to_app()`, `meshx_app_data_cb_t`, `meshx_app_ctrl_cb_t`, callback registration functions) remains unchanged. Internally, `meshx_send_msg_to_app()` now calls `mxcp_send_event(MXCP_EVT_EL_DATA_NOTIFY, ...)` instead of `mxsp_send_data_event()`. `meshx_send_ctrl_msg_to_app()` now calls `mxcp_send_event()` with the appropriate EVT ID instead of `mxsp_send_ctrl_event()`. Remove direct references to old MXSP send functions. Keep the `meshx_data_payload_t` and `meshx_ctrl_payload_t` unions for internal app callback use.
- **Complexity:** M
- **Dependencies:** TASK-003, TASK-004
- **Wave:** 2

### TASK-007: Update `meshx_composition_builder.cpp` — Use MXCP Struct Names
- **Linked REQs:** REQ-003, REQ-006
- **Linked TRDs:** TRD-04, TRD-06
- **Files:** `elements/src/meshx_composition_builder.cpp` (modified)
- **Description:** Update `meshx_get_element_composition_data()` and `meshx_get_element_state_data()` to use new MXCP payload types. Replace `mxsp_comp_entry_header_t` references with `mxcp_evt_composition_rsp_t`-compatible layout. Replace `mxsp_state_entry_header_t` with `mxcp_evt_element_state_rsp_t`-compatible layout. The serialization logic remains the same; only struct type names change.
- **Complexity:** S
- **Dependencies:** TASK-001
- **Wave:** 2

## Wave 3 — Cleanup and Host Tooling (depends on Wave 2)

### TASK-008: Remove Old `meshx_mxsp_cmd.h` and `meshx_mxsp_cmd.c`
- **Linked REQs:** REQ-005, REQ-006
- **Linked TRDs:** TRD-06
- **Files:** `common/inc/meshx_mxsp_cmd.h` (deleted), `common/src/meshx_mxsp_cmd.c` (deleted)
- **Description:** Delete both files. Verify no remaining references to `mxsp_dispatch_frame`, `mxsp_send_element_info_response`, `frame_dispatch`, `sys_cmd_dispatch`, or any old handler names. Update any CMakeLists.txt or build system files that reference these source files to reference `meshx_mxcp.c` instead.
- **Complexity:** S
- **Dependencies:** TASK-005, TASK-006
- **Wave:** 3

### TASK-009: Update `server.py` — Migrate Host Web Console to MXCP TYPE Encoding
- **Linked REQs:** REQ-006
- **Linked TRDs:** TRD-08
- **Files:** `tools/web_console/server/server.py` (modified)
- **Description:** Update all `send_cmd()` callers in `server.py`: (1) Hosted mode: `0x03` → `0x01` in `start()`, `resume()`, `enable_mxcp()`. (2) GPIO: Replace single `0xD1` dispatch with per-command IDs `0x21`-`0x29` and new typed payloads in `send_gpio_command()` and WebSocket handlers. (3) Response parsing: Replace `msg_type == 0xD2`/`0xD3` with EVT ID set `{0xA1..0xA9}` and `0xBE`, update struct unpacking to match new `mxcp_evt_gpio_rsp_t` / `mxcp_evt_gpio_async_t` layouts. (4) System events: Replace `msg_type == 0xB1` with `0x86`/`0x87`/`0x90`, remove `meshx_ctrl_msg_header_t` extraction. (5) Composition requests: Replace `0xC2` + `{0x07, 0x00}` payload with `0x03` + empty payload. (6) Element commands: Replace `0xC1` with `0x10`.
- **Complexity:** L
- **Dependencies:** TASK-003
- **Wave:** 3

## Wave Summary

| Wave | Tasks | Estimated Complexity |
|------|-------|---------------------|
| 0 | TASK-001, TASK-002 | L + S |
| 1 | TASK-003, TASK-004 | XL + M |
| 2 | TASK-005, TASK-006, TASK-007 | L + M + S |
| 3 | TASK-008, TASK-009 | S + L |
