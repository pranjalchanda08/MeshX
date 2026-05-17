# UVP Routing and ACK Fix Tasks

| Task ID | Title | Complexity | Wave | Dependencies | REQ / Design Link |
|---------|-------|------------|------|--------------|-------------------|
| TASK-001 | Update Structs and Context | Small | 1 | None | REQ-001, REQ-002, REQ-004 (Section 5.1) |
| TASK-002 | Control Task Bridge Update | Small | 2 | TASK-001 | REQ-001 (Section 7.1.2) |
| TASK-003 | BLE Port Layer Update | Small | 3 | TASK-002 | REQ-001 (Section 7.1.1) |
| TASK-004 | Update Element Interface | Medium | 4 | TASK-001 | REQ-002 (Section 7.1.4) |
| TASK-005 | Dispatcher Context Population | Small | 5 | TASK-004 | REQ-002 (Section 7.1.3) |
| TASK-006 | Element Dual-Routing Logic | Medium | 6 | TASK-005, TASK-004 | REQ-003, REQ-005 (Section 6.1) |

## Task Details

### Wave 1
#### TASK-001: Update Structs and Context
- **Description**: 
  1. Modify `meshx_uvp_header_t` in `meshx_uvp.h` to use bitfields: `element_idx:5`, `ack_req:1`, `rfu:2`.
  2. Add `dst_addr` to `control_task_uvp_meta_t` in `meshx_control_task.h`.
  3. Create the `meshx_uvp_ctx_t` struct (containing `src_addr`, `dst_addr`, `tid`, `ack_req`).
- **Files**: `main/component/meshx/inc/meshx_uvp.h`, `main/component/meshx/inc/meshx_control_task.h`

### Wave 2
#### TASK-002: Control Task Bridge Update
- **Description**: Update the signature and implementation of `control_task_msg_publish_uvp` to accept `uint16_t dst_addr` and store it inside the queued metadata block.
- **Files**: `main/component/meshx/inc/meshx_control_task.h`, `main/component/meshx/src/meshx_control_task.c`

### Wave 3
#### TASK-003: BLE Port Layer Update
- **Description**: In `esp_ble_mesh_vendor_server_cb`, extract `ctx->recv_dst` and pass it as the new argument to `control_task_msg_publish_uvp`.
- **Files**: `port/platform/esp/esp_idf/ble_mesh/server/esp_ven_srv_model.c`

### Wave 4
#### TASK-004: Update Element Interface
- **Description**: Update the virtual method signature of `meshXElementIF::on_model_cb` and all its implementations to accept `const meshx_uvp_ctx_t* ctx`.
- **Files**: `main/component/meshx/ble_mesh/common/inc/meshx_fwd_decl.hpp`, `main/component/meshx/ble_mesh/elements/inc/meshx_element_class.hpp`, `main/component/meshx/ble_mesh/elements/src/meshx_element_class.cpp`, `main/component/meshx/ble_mesh/elements/inc/variants/meshx_uvp_element.hpp`, `main/component/meshx/ble_mesh/elements/src/variants/meshx_uvp_element.cpp`

### Wave 5
#### TASK-005: Dispatcher Context Population
- **Description**: In `uvp_rx_task_handler`, construct the new `meshx_uvp_ctx_t` using `p_meta->src_addr`, `p_meta->dst_addr`, and parsing the `ack_req` flag from the UVP header. Pass this context into the element's callback.
- **Files**: `main/component/meshx/ble_mesh/common/src/meshx_uvp_dispatcher.cpp`

### Wave 6
#### TASK-006: Element Dual-Routing Logic
- **Description**: In the specific `meshXUVPElement` implementation (or base element logic, as appropriate), replace simple broadcasting with Dual-Routing logic: 
  - If `ctx->ack_req == 1`, call `meshx_uvp_send` (or equivalent transmission wrapper) to send a direct response to `ctx->src_addr`. 
  - If the element's publish address is set and `ctx->src_addr` != `pub_addr`, publish the state change.
- **Files**: `main/component/meshx/ble_mesh/elements/src/variants/meshx_uvp_element.cpp`
