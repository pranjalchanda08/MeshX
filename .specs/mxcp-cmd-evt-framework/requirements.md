# MXCP Framework Restructuring — Requirements

## Overview

Restructure the current MXSP (MeshX Serial Protocol) into a proper **MXCP (MeshX Command Protocol)** framework with a unified command-and-event table-driven architecture, structured telemetry serialization, and single-layer dispatch.

---

## REQ-001: Unified MXCP Command and Event Telemetry Model

**Priority:** P0

**Description:**
The framework shall define a unified telemetry model where every operation is expressed as either a **Command** (Host → Engine) or an **Event** (Engine → Host). Commands and Events shall be first-class citizens with well-defined IDs, typed payloads, and explicit serialization.

**Acceptance Criteria:**
- All MXSP message types are replaced or refactored into MXCP Commands (Host → Engine) and MXCP Events (Engine → Host).
- Each command and event has a unique numeric ID within a single flat namespace.
- The frame header distinguishes direction (CMD vs EVT) and carries the command/event ID.

---

## REQ-002: Command-to-Event Association

**Priority:** P0

**Description:**
Every command shall have an associated **sync response event** (acknowledgment) and/or an **async notification event**. The framework shall define this mapping explicitly so that the host can correlate a response to the command that triggered it.

**Acceptance Criteria:**
- Each command entry in the command table declares its associated sync event ID and optional async event ID.
- Sync events carry a reference (e.g., command ID or sequence number) linking them back to the originating command.
- Async events are fire-and-forget but still have a declared association with the originating command type.
- GPIO commands (SET_LEVEL, GET_LEVEL, etc.) each produce a corresponding GPIO response event.
- System commands (NODE_RESET, GET_COMPOSITION, etc.) each produce a corresponding system response event.

---

## REQ-003: Structured Serialised Telemetry with Header and Footer

**Priority:** P0

**Description:**
All commands and events shall use associated strongly-typed C structures that are packed and serialized into the MXCP frame payload. The frame itself shall have a consistent header and footer structure.

**Acceptance Criteria:**
- Every command has a typed `mxcp_cmd_<name>_t` structure for its payload.
- Every event has a typed `mxcp_evt_<name>_t` structure for its payload.
- All payload structures use `#pragma pack(push, 1)` for deterministic serialization.
- The MXCP frame header includes: SOF, length, direction (CMD/EVT), command/event ID.
- The MXCP frame footer includes: checksum and EOF.
- Existing `mxsp_gpio_cmd_payload_t`, `mxsp_gpio_rsp_payload_t`, etc. are refactored into the new MXCP typed payload model.
- Existing `meshx_ctrl_msg_header_t` and `meshx_app_element_msg_header_t` are unified into the MXCP frame header.

---

## REQ-004: Command and Event Table-Driven Architecture

**Priority:** P0

**Description:**
The framework shall be designed around a **single command table** and a **single event table**. These tables serve as the central registry for all telemetry, replacing the ad-hoc dispatch mechanism.

**Acceptance Criteria:**
- A single `mxcp_cmd_table[]` maps each command ID to: its handler function, its payload type/size, and its associated sync/async event IDs.
- A single `mxcp_evt_table[]` maps each event ID to: its serializer/deserializer, payload type/size, and originating command ID reference.
- Adding a new command or event requires only adding an entry to the appropriate table — no dispatch code changes.
- The existing `frame_dispatch[]` and `sys_cmd_dispatch[]` two-level tables are replaced by the single command table.
- GPIO commands are integrated into the unified command table (no separate switch-case dispatch).

---

## REQ-005: Single-Layer Dispatch (Eliminate 2-Layer Command Structure)

**Priority:** P0

**Description:**
The current 2-layer dispatch (Level 1: frame type → handler, Level 2: evt_id → sub-handler) shall be eliminated. The new framework shall use a **single dispatch lookup** from the frame header directly to the command/event handler.

**Acceptance Criteria:**
- The MXCP frame header carries both the direction bit and the command/event ID — no secondary header extraction is needed.
- A single `mxcp_dispatch()` function parses the frame header and performs one table lookup to find the handler.
- No nested dispatch (no `mxsp_cmd_sys_dispatch`-style second level).
- The `meshx_ctrl_msg_header_t` (evt_id + reserved) is no longer embedded in the payload — the command/event ID lives in the frame header.
- The `mxsp_msg_type_t` enum (SYS_CMD, EL_CMD, GPIO_CMD, etc.) is replaced by a flat command/event ID namespace with a direction bit.

---

## REQ-006: Backward-Compatible Frame Format Transition

**Priority:** P1

**Description:**
The new MXCP frame format shall be documented and the transition from MXSP shall be clean. Existing functionality (GPIO, system control, element data path) must be preserved under the new framework.

**Acceptance Criteria:**
- All current MXSP capabilities are available under MXCP: node reset, get composition, get element state, set console routing, element commands, GPIO commands/responses/events.
- The UART RX state machine in `meshx_serial.c` is updated to parse the new MXCP frame format.
- The UART TX path uses the new MXCP frame builder.
- Frame checksum calculation is preserved or upgraded (XOR → CRC16 if desired, but must be documented).

---

## REQ-007: Hosted Mode and GPIO Integration

**Priority:** P1

**Description:**
The GPIO hosted-mode command/event path shall be fully integrated into the MXCP command/event tables with no special-case handling.

**Acceptance Criteria:**
- All GPIO commands (SET_LEVEL, GET_LEVEL, TOGGLE, SET_PWM_DUTY, SET_PWM_FREQ, INTR_ENABLE, INTR_DISABLE, GET_CONFIG, GET_STATE) are entries in `mxcp_cmd_table[]`.
- All GPIO responses and async events are entries in `mxcp_evt_table[]`.
- The `mxsp_handle_gpio_cmd()` switch-case is eliminated — each GPIO command dispatches directly from the command table.
- GPIO hosted event callback (`meshx_gpio_hosted_event_handler`) uses the new MXCP event API to send events.

---

## REQ-008: Control and Data Path Unification

**Priority:** P1

**Description:**
The current split between "system control path" and "element data path" shall be unified under the single MXCP framework. Both paths are just different command IDs in the same table.

**Acceptance Criteria:**
- System commands (reset, composition, state, console routing) and element commands are in the same `mxcp_cmd_table[]`.
- System events (provisioning, identify, composition response) and data events (element state change) are in the same `mxcp_evt_table[]`.
- `mxsp_send_ctrl_event()` and `mxsp_send_data_event()` are replaced by a single `mxcp_send_event()` API that takes an event ID and payload.
- The existing callback registration (`meshx_app_reg_element_callback`, `meshx_app_reg_system_events_callback`) continues to work (or is unified into a single registration API).

---

## REQ-009: Extensibility and New Command/Event Registration

**Priority:** P2

**Description:**
The framework shall make it straightforward to add new commands and events without modifying core dispatch logic.

**Acceptance Criteria:**
- Adding a new command requires: (1) defining a command ID, (2) defining a payload struct, (3) adding an entry to `mxcp_cmd_table[]`.
- Adding a new event requires: (1) defining an event ID, (2) defining a payload struct, (3) adding an entry to `mxcp_evt_table[]`.
- No changes to `mxcp_dispatch()` or frame parsing when adding new telemetry.
- A macro or template is provided for common table entry patterns (e.g., `MXCP_CMD_ENTRY(id, handler, payload_size, sync_evt_id, async_evt_id)`).
