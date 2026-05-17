# UVP Routing and ACK Fix Requirements

## REQ-001: Propagate Destination Address to Control Task
- **Description**: The lowest-level ESP-IDF BLE Mesh callback currently drops the message destination address when bridging to the `control_task`. This address must be preserved.
- **Acceptance Criteria**: `control_task_uvp_meta_t` includes a `dst_addr` field, and it is correctly populated from `ctx->recv_dst` in `esp_ble_mesh_vendor_server_cb`.
- **Priority**: P0

## REQ-002: Element Callback Context Propagation
- **Description**: The UVP Dispatcher drops routing metadata (source address) before invoking the application-level element callback. A structured context must be passed down.
- **Acceptance Criteria**: A new `meshx_uvp_ctx_t` struct is defined (containing `src_addr`, `dst_addr`, and `tid`). The signature of `meshXElementIF::on_model_cb` is updated to accept this context, and the UVP dispatcher supplies it.
- **Priority**: P0

## REQ-003: Unicast ACK Routing Capability
- **Description**: Vendor model elements must be able to use the provided routing context to direct status/ACK responses strictly to the requesting client rather than publishing broadly.
- **Acceptance Criteria**: The routing context is successfully made available inside the element logic (e.g., `meshXUVPElement`), giving it the necessary addresses to target outbound status messages directly to `ctx->src_addr`.
- **Priority**: P0

## REQ-004: ACK Request Indicator in UVP Header
- **Description**: The client request payload currently lacks a mechanism to specify whether an ACK is required. The UVP header must include a flag to indicate if the client expects an acknowledgment.
- **Acceptance Criteria**: The `meshx_uvp_header_t` struct is updated to include an `ack_req` flag (e.g., by utilizing reserved bits in the `element_idx` field). This flag is parsed and passed via the new routing context (`meshx_uvp_ctx_t`) to the element callback.
- **Priority**: P0

## REQ-005: Dual-Routing Response (Unicast ACK + Group Publish)
- **Description**: To keep the network state synchronized, if a client sends a request to a unicast address or a group address that is *different* from the server's registered publish address, the server must still publish its new state to its registered publish address.
- **Acceptance Criteria**: If the client requires an ACK (REQ-004), a unicast ACK is sent directly to the client's `src_addr`. Additionally, if a publish address is configured, the server must *also* publish the state change to the registered publish address.
- **Priority**: P0

