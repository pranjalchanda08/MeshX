# Requirements Document: Unified MXCP and Application APIs

**Milestone:** Release v0.6  
**Issue:** #143 (Bug / Architecture Enhancement)  
**Status:** In Review  
**Target Subsystem:** `meshx_api`, `meshx_mxcp`, `meshx_control_task`

---

## 1. Introduction

In MeshX, two independent communication paradigms currently exist to interact with nodes:
1. **MXCP (MeshX Command Protocol)**: External binary command/event wire protocol exchanging frames between an external Host MCU and the MeshX Engine via serial/transport.
2. **`meshx_api` (Application API)**: In-firmware API used by application components, logical models, and event handlers to exchange messages over internal FreeRTOS task queues (`meshx_control_task`).

Over time, this led to structural duplication (`mxcp_cmd_el_send_t` vs `meshx_app_element_msg_header_t`), lack of clear distinction between application data and node control paths, awkward buffer allocations (`MESHX_APP_API_MSG_MAX_SIZE`), and unconditioned test commands (such as raw GPIO actuation) exposed in production builds.

This specification unifies the foundation data structures, enforces a strict physical and logical boundary between Data and Control paths, refactors `meshx_api.h` for clean and efficient usage, and guards test/debug GPIO APIs behind build-time configuration flags for production readiness.

---

## 2. Glossary

- **MXCP**: MeshX Command Protocol. Single-layer binary wire protocol (`[SOF][LEN][TYPE][PAYLOAD][CHK][EOF]`).
- **App API (`meshx_api`)**: C-level public programming interface exposed to user applications running directly on the MeshX node.
- **Data Path**: Communication path exclusively handling BLE Mesh element telemetry and actuation (TX/RX), such as On/Off, Level, CTL, HSL, and Sensor readings.
- **Control Path**: Communication path handling system management, node provisioning lifecycle, dynamic composition, network reset, and autonomous pairing handshakes.
- **Hosted Mode**: Deployment where an external Host MCU commands MeshX via MXCP over UART.
- **Non-Hosted Mode**: Standalone deployment where application code runs directly on the same MCU as the MeshX engine.
- **UVP**: Unified Vendor Protocol. Single BLE Mesh Vendor Model carrying element and management traffic.

---

## 3. Requirements

### Requirement 1: Unified Data Structures (REQ-001)

**User Story:** As an embedded software developer, I want MXCP wire handlers and internal `meshx_api` to use identical header and payload data structures, so that the stack avoids redundant type casting, duplicate definitions, and memory overhead.

#### Acceptance Criteria
1. THE system SHALL define a single canonical 8-byte packed Element Data Header structure (`meshx_data_msg_hdr_t`) containing `element_id` (16-bit), `element_type` (16-bit), `func_id` (16-bit), and `msg_len` (16-bit).
2. THE MXCP protocol layer SHALL define `mxcp_cmd_el_send_t` and `mxcp_evt_el_data_rx_notify_t` / `mxcp_evt_el_data_tx_notify_t` using `meshx_data_msg_hdr_t`.
3. THE internal application API message structures SHALL use `meshx_data_msg_hdr_t` directly instead of maintaining a separate header definition.
4. WHEN an element event is received over the air or generated locally, THE same payload bytes SHALL be consumable by both local app callbacks and MXCP wire serialization without intermediate data conversions.

---

### Requirement 2: Strict Separation of Data and Control Paths (REQ-002)

**User Story:** As a firmware architect, I want element sensor/actuation data and system control events to flow through clearly separated communication paths, so that high-frequency element telemetry cannot corrupt or block critical system control operations.

#### Acceptance Criteria
1. THE Data Path SHALL exclusively handle BLE Mesh element telemetry and actuation messages (both TX to mesh and RX from mesh).
2. THE Control Path SHALL exclusively handle system management and node lifecycle events:
   - Node Provisioning (`PROV_START`, `PROV_COMP`, `PROV_FAILED`)
   - Node Configuration (`NODE_RESET`, `SET_COMPOSITION`, `GET_COMPOSITION`)
   - System Diagnostics (`IDENTIFY_START`, `IDENTIFY_STOP`)
   - Autonomous Pairing & Topology updates (`CAPABILITY_REPORT`, `BIND_STATUS`, `UNBIND_STATUS`, `LINK_TABLE_RSP`, `LINK_EPOCH_RSP`)
3. THE system SHALL provide distinct registration entry points for application callbacks:
   - `meshx_app_reg_data_callback()` for the Data Path.
   - `meshx_app_reg_ctrl_callback()` for the Control Path.
4. THE internal event publishing task (`meshx_control_task`) SHALL route data messages using `CONTROL_TASK_MSG_EVT_DATA` and control messages using `CONTROL_TASK_MSG_EVT_CTRL`.
5. Non-element control data SHALL NEVER be routed to the Data Path callback.

---

### Requirement 3: Refactored and Enhanced `meshx_api.h` (REQ-003)

**User Story:** As an application developer, I want a modern, cleanly structured C API in `meshx_api.h`, so that my application can easily send and receive data without relying on brittle fixed-size unions or vague parameter lists.

#### Acceptance Criteria
1. THE header `meshx_api.h` SHALL expose explicit, symmetric function signatures:
   - `meshx_data_send_to_app(const meshx_data_msg_hdr_t *hdr, const void *payload)`
   - `meshx_data_send_to_element(const meshx_data_msg_hdr_t *hdr, const void *payload)`
   - `meshx_ctrl_send_event(meshx_ctrl_evt_id_t evt_id, uint16_t status, const void *payload, uint16_t payload_len)`
2. THE callback signatures SHALL accept `const meshx_data_msg_hdr_t *` with a const payload pointer, enabling zero-copy inspection when applicable.
3. THE API SHALL maintain backward-compatible inline wrappers for deprecated legacy functions (`meshx_send_msg_to_app`, `meshx_send_msg_to_element`, `meshx_app_reg_element_callback`, `meshx_app_reg_system_events_callback`).
4. THE header SHALL NOT require or expose `meshx_c_header.h` to C++ translation units in violation of the project coding guidelines.

---

### Requirement 4: Production Conditioning of GPIO APIs (REQ-004)

**User Story:** As a product security and release engineer, I want raw GPIO control and test commands disabled in production builds while available during automated test runs, so that released products cannot be tampered with via serial GPIO commands.

#### Acceptance Criteria
1. THE build system SHALL provide a configuration option `CONFIG_MESHX_ENABLE_GPIO_TEST_API` (or equivalent Kconfig macro).
2. WHEN `CONFIG_MESHX_ENABLE_GPIO_TEST_API` is disabled (default in production builds):
   - All GPIO command handlers (`mxcp_cmd_fn_gpio_*`) SHALL be excluded from compilation.
   - All GPIO entries in `mxcp_cmd_table[]` SHALL be excluded from compilation.
   - Any attempt by a host to send a GPIO command (`0x21`..`0x29`) SHALL be rejected with `MXCP_STATUS_UNKNOWN_CMD`.
   - GPIO test event and command structs in `meshx_mxcp.h` SHALL be conditionally excluded.
3. WHEN `CONFIG_MESHX_ENABLE_GPIO_TEST_API` is enabled (enabled for test and debug builds):
   - All existing GPIO command functionality SHALL remain fully operational.
   - The Python automated test suite (`test_functional_pairing.py` and GPIO verification cases) SHALL execute without regressions.
