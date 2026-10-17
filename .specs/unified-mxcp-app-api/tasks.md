# Tasks & Implementation Plan: Unified MXCP and Application APIs

**Milestone:** Release v0.6  
**Issue:** #143  
**Status:** In Review  
**Target Subsystem:** `meshx_api`, `meshx_mxcp`, `meshx_control_task`, `meshx_uvp_dispatcher`

---

## Task List

### Phase 1: Header Architecture & Data Structure Unification (REQ-001, REQ-003)
- [x] 1. Define Canonical Headers in `meshx_api.h`
  - [x] 1.1 Define 8-byte packed `meshx_data_msg_hdr_t` (`element_id`, `element_type`, `func_id`, `msg_len`)
  - [x] 1.2 Define packed `meshx_ctrl_msg_hdr_t` and `meshx_ctrl_evt_id_t`
  - [x] 1.3 Add new API declarations (`meshx_data_send_to_app`, `meshx_data_send_to_element`, `meshx_ctrl_send_event`)
  - [x] 1.4 Add new callback registration prototypes (`meshx_app_reg_data_callback`, `meshx_app_reg_ctrl_callback`)
  - [x] 1.5 Add backward-compatible wrapper functions for legacy APIs
- [x] 2. Update `meshx_mxcp.h` to use Unified Header Types
  - [x] 2.1 Aliasing `mxcp_cmd_el_send_t`, `mxcp_evt_el_data_rx_notify_t`, and `mxcp_evt_el_data_tx_notify_t` to `meshx_data_msg_hdr_t`
  - [x] 2.2 Verify Wire framing alignment for `MXCP_CMD_EL_SEND` (0x10) and `MXCP_EVT_EL_DATA_RX_NOTIFY` (0x10)

### Phase 2: Decouple Data & Control Execution Paths (REQ-002)
- [x] 3. Refactor Engine in `meshx_api.c`
  - [x] 3.1 Separate internal handlers for `CONTROL_TASK_MSG_EVT_DATA` and `CONTROL_TASK_MSG_EVT_CTRL`
  - [x] 3.2 Implement `meshx_data_send_to_app` with direct buffer packing to MXCP and local app data callback
  - [x] 3.3 Implement `meshx_data_send_to_element` to route commands directly to UVP Dispatcher (`CONTROL_TASK_MSG_CODE_TO_MESHX`)
  - [x] 3.4 Implement `meshx_ctrl_send_event` mapping internal control events to MXCP control events
  - [x] 3.5 Eliminate obsolete heap allocations and static union sizing restrictions

### Phase 3: Production Guarding for GPIO APIs (REQ-004)
- [x] 4. Implement `CONFIG_MESHX_ENABLE_GPIO_TEST_API`
  - [x] 4.1 In `meshx_mxcp.h`: Wrap GPIO commands, events, and payload structs with `#if CONFIG_MESHX_ENABLE_GPIO_TEST_API`
  - [x] 4.2 In `meshx_mxcp.c`: Wrap GPIO handlers `mxcp_cmd_fn_gpio_*` and entries in `mxcp_cmd_table[]` with `#if CONFIG_MESHX_ENABLE_GPIO_TEST_API`
  - [x] 4.3 Ensure build default enables this in test builds and disables in production builds

### Phase 4: Integration with UVP Dispatcher & Elements
- [x] 5. Adapt `meshx_uvp_dispatcher.cpp`
  - [x] 5.1 Update `uvp_app_command_cb` to unpack `meshx_data_msg_hdr_t` directly
  - [x] 5.2 Validate compatibility with `meshXElementRegistry::find_element()` and `on_model_cb()`

### Phase 5: Verification & Quality Assurance
- [x] 6. Build and Test Validation
  - [x] 6.1 Clean build under Windows IDF environment: `meshx.py -bc`
  - [ ] 6.2 Run autotest single-node pairing & telemetry test suite: `test_functional_pairing.py` *(Pending: Phase 1 MXCP wire frame & epoch query verified on COM3; full binding suite requires provisioned node bench)*
  - [x] 6.3 Test compilation with `CONFIG_MESHX_ENABLE_GPIO_TEST_API=0` to verify GPIO exclusion in production mode
