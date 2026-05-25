# Task Breakdown

## Wave 1
*(Foundational changes to headers. All other tasks depend on this.)*

### TASK-001: Define Unified Structures and Enums
- **Description:** Move Data and Control enums, payload structures, and unified send/receive API signatures into `meshx_api.h`. Apply `#pragma pack(1)`. Remove duplicate and redundant definitions from `meshx_mxcp.h` and update it to rely on `meshx_api.h`.
- **Linked Requirements:** REQ-001, REQ-002, REQ-003
- **Linked Design:** 5.2.1, 5.2.2
- **Complexity:** S
- **Dependencies:** None
- **Wave:** 1

---

## Wave 2
*(Parallel implementation across different files and languages, depending on the new headers.)*

### TASK-002: Implement C Firmware SDK unified API
- **Description:** Update `meshx_api.c` to implement the new unified `meshx_api_data_send` and `meshx_api_ctrl_send` functions. Implement routing logic to push messages to local registered callbacks and/or the MXCP layer based on Hosted Mode status.
- **Linked Requirements:** REQ-002
- **Linked Design:** 7.1
- **Complexity:** M
- **Dependencies:** TASK-001
- **Wave:** 2

### TASK-003: Refactor MXCP Core Serialization
- **Description:** Update `meshx_mxcp.c` to directly serialize/deserialize the new `meshx_msg_data_t` and `meshx_msg_ctrl_t` structures over UART. Update the MXCP dispatch tables to map to the new unified enum IDs.
- **Linked Requirements:** REQ-001, REQ-002
- **Linked Design:** 4.1
- **Complexity:** M
- **Dependencies:** TASK-001
- **Wave:** 2

### TASK-004: Update Internal Apps and Elements
- **Description:** Refactor existing BLE Mesh code (e.g., `meshx_composition_builder.cpp`, `meshx_uvp_dispatcher.cpp`, `meshx_api_client/server` models) to use the new `meshx_api_data_send` and `meshx_api_ctrl_send` APIs instead of the legacy app and element messaging APIs.
- **Linked Requirements:** REQ-001
- **Linked Design:** 5.1
- **Complexity:** L
- **Dependencies:** TASK-001
- **Wave:** 2

### TASK-005: Create Python Host SDK
- **Description:** Create/update Python dataclasses and the demuxer client API in `tools/web_console/server/` or `tools/scripts/autotest/` to strictly map to the C structures and provide `data_send` and `ctrl_send` methods for the UI/Testing frameworks.
- **Linked Requirements:** REQ-005
- **Linked Design:** 5.3.3
- **Complexity:** M
- **Dependencies:** TASK-001
- **Wave:** 2

### TASK-006: Create Host MCU SDK (C Implementation)
- **Description:** Create the external Host MCU C SDK template files (e.g., `meshx_host_sdk.c` / `meshx_host_sdk.h`) providing the high-level API, MXCP frame parsing logic, and the generic porting interface (`meshx_port_tx`) for external microcontrollers.
- **Linked Requirements:** REQ-005
- **Linked Design:** 5.3.2
- **Complexity:** M
- **Dependencies:** TASK-001
- **Wave:** 2

---

## Wave 3
*(Cleanup tasks that touch files already modified in Wave 2)*

### TASK-007: Isolate GPIO APIs for Production
- **Description:** Wrap GPIO-related APIs, structs, and dispatch handlers in `meshx_api.c`, `meshx_api.h`, and `meshx_mxcp.c` inside `#ifdef CONFIG_MESHX_ENABLE_GPIO_TEST_API` to physically exclude them from production builds.
- **Linked Requirements:** REQ-004
- **Linked Design:** 6.1
- **Complexity:** S
- **Dependencies:** TASK-002, TASK-003
- **Wave:** 3
