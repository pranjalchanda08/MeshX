# Requirements Specification: UVP-Driven Provisioning & Security Management with MXCP Capability

**Feature:** UVP-Driven Remote Provisioning, 5-Minute Key Rotation, and Provisioner Redundancy via MXCP  
**Status:** Approved Architectural Pivot (Zero-Bloat UVP Architecture)  
**Target Subsystems:** `meshx_prov_srv`, `meshx_uvp_dispatcher`, `meshx_mxcp`, `meshx_api`, `meshx_nvs`

---

## 1. Executive Summary & Architectural Paradigm

Previous designs considered enabling heavy native BLE Mesh SIG stacks (`CONFIG_BLE_MESH_PROVISIONER`, `CONFIG_BLE_MESH_RPR_SRV`, `CONFIG_BLE_MESH_RPR_CLI`, `CONFIG_BLE_MESH_CONFIG_CLIENT`) which would consume `>140 KB` of flash and `>6 KB` of static RAM.

This specification adopts a **Zero-Bloat UVP Architecture**:
1. **Virgin Node Onboarding (Native Prov Server Only)**: The local node utilizes the lightweight, mandatory `CONFIG_BLE_MESH_PROV=y` (Provisioning Server) solely for initial network joining (receiving its primary NetKey and Unicast Address).
2. **Remote Provisioning (RPR) via UVP Tunneling**: Out-of-range unprovisioned device beacons are captured by standard mesh nodes via BLE advertisement scanning and tunneled across the mesh network as UVP Vendor Messages (`OP_UVP_RPR_*`). **Native RPR Client/Server stacks remain completely disabled (`=n`)**.
3. **5-Minute Key Rotation via UVP Command Protocol**: Key Refresh (Phases 1, 2, and 3) is orchestrated using authenticated, encrypted UVP vendor frames. Receiving nodes update keys locally via node-level key management APIs. **Native Config Client / Provisioner stacks remain completely disabled (`=n`)**.
4. **Provisioner Redundancy (Active/Standby Pair)**: State synchronization, heartbeat tokens, and partitioned address pools are coordinated via UVP control messages between Master and Standby nodes.

---

## 2. Functional Requirements (REQ-F)

### REQ-F01: Separation of Concerns
- **Local Prov Server**: Native PB-ADV/PB-GATT provisioning server is maintained only for first-time onboarding.
- **Advanced Management (Remote Prov, Key Rotation, Redundancy)**: Executed entirely over the **MeshX Unified Vendor Protocol (UVP)** and exposed to external Host controllers via **MXCP**.
- No native SIG Client models (`CONFIG_BLE_MESH_CONFIG_CLIENT=n`) or native Provisioner stacks (`CONFIG_BLE_MESH_PROVISIONER=n`) shall be introduced.

### REQ-F02: Local Provisioning MXCP Command Set (Host → Engine)
- **`MXCP_CMD_PROV_ENABLE`**: Enable unprovisioned device beaconing on PB-ADV/PB-GATT.
- **`MXCP_CMD_PROV_DISABLE`**: Disable device provisioning beacons.
- **`MXCP_CMD_PROV_SET_DEVICE_NAME`**: Set or update unprovisioned device advertising friendly name.
- **`MXCP_CMD_PROV_SET_STATIC_OOB`**: Configure static OOB authentication key.
- **`MXCP_CMD_PROV_INPUT_AUTH_RSP`**: Submit host input response for input OOB authentication.
- **`MXCP_CMD_PROV_GET_STATE`**: Query current provisioning state, unicast address, and NetKey index.

### REQ-F03: Local Provisioning MXCP Event Set (Engine → Host)
- **`MXCP_EVT_PROV_STATE_IND`**: Status indication (Unprovisioned, Beaconing, Provisioning, Provisioned).
- **`MXCP_EVT_PROV_LINK_OPEN`**: Emitted when PB-ADV/PB-GATT link opens.
- **`MXCP_EVT_PROV_LINK_CLOSE`**: Emitted when link closes.
- **`MXCP_EVT_PROV_AUTH_REQ`**: Emitted when OOB display or user input is needed.
- **`MXCP_EVT_PROV_COMP`**: Emitted when provisioning completes.
- **`MXCP_EVT_PROV_FAILED`**: Emitted on failure or timeout.
- **`MXCP_EVT_NODE_RESET_IND`**: Emitted when node is reset.

### REQ-F04: UVP-Driven Remote Provisioning (RPR)
- Any operational mesh node with BLE scan enabled shall capture raw unprovisioned beacons (`0x2B` Mesh Beacon with unprov flag).
- The node encapsulates the beacon (UUID, OOB info, RSSI) and transmits a UVP message (`OP_UVP_RPR_BEACON_REPORT`) to the Master/Host.
- The Master transmits provisioning PDUs encapsulated in `OP_UVP_RPR_TUNNEL_DATA`.
- The intermediate node injects the raw PDU into the air via PB-ADV to the target unprovisioned node.
- **Zero dependency on SIG RPR models (`CONFIG_BLE_MESH_RPR_*=n`)**.

### REQ-F05: UVP-Driven 5-Minute Key Rotation (P1 Priority)
- The Master (or Host via MXCP) triggers Key Rotation every 5 minutes (`300 seconds` $\pm 15$s jitter).
- **Phase 1 (Distribution)**: Master distributes new NetKey/AppKey payload over UVP (`OP_UVP_KEY_REFRESH_DISTRIBUTE`). Nodes install the new key in the secondary key slot and accept both old and new keys.
- **Phase 2 (Switchover)**: Master broadcasts `OP_UVP_KEY_REFRESH_SWITCHOVER`. Nodes switch primary transmission to the new key.
- **Phase 3 (Revocation)**: Master broadcasts `OP_UVP_KEY_REFRESH_REVOKE_OLD`. Nodes invalidate and wipe the old key from memory and NVS.
- **Zero dependency on native Config Client model (`CONFIG_BLE_MESH_CONFIG_CLIENT=n`)**.

### REQ-F06: Provisioner Redundancy (Active/Standby Pair)
- When multiple controller nodes exist, exactly **two provisioners remain active concurrently**:
  - **Master Provisioner (Active)**: Authoritative entity allocating addresses from Pool A (`0x0002..0x3FFF`).
  - **Standby Provisioner (Shadow)**: Replicated mirror with Pool B (`0x4000..0x7FFF`).
- Heartbeat and node database sync packets are exchanged over UVP vendor opcodes.
- If Master fails (3 missed 5s heartbeats = 15s timeout), Standby promotes itself to Master and onboards using Pool B, preventing any address collision.

### REQ-F07: Fast Bulk Provisioning on top of RPR (UVP-Driven)
- When multiple unprovisioned devices are detected in a cluster (e.g., a factory floor or installation with tens of unprovisioned nodes), the Master Provisioner SHALL support **Fast Bulk Provisioning**:
  - **Batched Remote Discovery**: Intermediate nodes report multiple unprovisioned beacons in a aggregated scan list over UVP rather than one-by-one.
  - **Pipelined Session Provisioning**: Instead of completing standard full-length sequential exchanges (which can take 8-15 seconds per device), the Provisioner utilizes pipelined RPR sessions or static pre-shared key batches to provision nodes in parallel across intermediate relay proxies.
  - **Automated Block Address Allocation**: The Master issues contiguous address chunks (`base_addr`, `count`) in a single bulk onboarding descriptor to intermediate proxies.
  - **MXCP Control**:
    - `MXCP_CMD_UVP_FAST_PROV_START`: Initiates bulk onboarding with UUID mask, target count, and auto-address block.
    - `MXCP_EVT_UVP_FAST_PROV_PROGRESS`: Emitted as nodes are batched (reporting progress percentage, successfully onboarded nodes, and failed nodes).


---

## 3. Non-Functional Requirements (REQ-NF)

### REQ-NF01: Flash Budget (< 15 KB Flash Impact)
- Total new code footprint for the entire UVP-driven suite (Remote Prov tunneling + 5-min Key Rotation + Redundancy) SHALL NOT exceed **15 KB** of flash.
- Static RAM impact SHALL NOT exceed **1.5 KB**.

### REQ-NF02: Baseline Native Stack Preservation
- Retain the baseline lightweight node configuration (`CONFIG_BLE_MESH_NODE=y`, `CONFIG_BLE_MESH_PROV=y`) unaltered to preserve standard SIG smartphone app onboarding compatibility over PB-ADV/PB-GATT.
- No heavy SIG models (`CONFIG_BLE_MESH_PROVISIONER`, `CONFIG_BLE_MESH_CONFIG_CLIENT`, `CONFIG_BLE_MESH_RPR_*`) shall be enabled; all advanced operations (remote provisioning, key rotation, redundancy) are executed over the UVP vendor layer.


### REQ-NF03: Deterministic Timeout & Link Teardown
- RPR tunnel sessions and OOB authentication links shall timeout deterministically after 30 seconds of radio inactivity.

---

## 4. Future Requirements Roadmap (Gateway & Connectivity Evolution)

### REQ-F-ROADMAP-01: Binary MXCP Encapsulation over MQTT
- The Gateway capability SHALL support transporting MXCP frames encapsulated directly as binary payloads over MQTT topics rather than heavy JSON text representations.
- **Topic Hierarchy**:
  - `meshx/{mesh_id}/downlink`: Host $\rightarrow$ Gateway $\rightarrow$ Mesh nodes (`MXCP_CMD_*`).
  - `meshx/{mesh_id}/uplink`: Mesh nodes $\rightarrow$ Gateway $\rightarrow$ Host / Cloud (`MXCP_EVT_*`).
- **Memory & Bandwidth Savings**:
  - Eliminates JSON string parsing, deserialization buffers, and formatting overhead, saving ~15 KB heap and reducing Wi-Fi payload sizes to bare wire bytes (`[SOF][LEN][TYPE][PAYLOAD][CHK][EOF]`).

### REQ-F-ROADMAP-02: TCP / Stream-based OTA Client
- The Gateway node SHALL provide background TCP/HTTPS OTA fetch capability targeting the dual `ota_0`/`ota_1` (`1900 KB`) slots.
- Download pipelines SHALL stream image blocks directly into flash sectors with minimal internal RAM buffering (e.g. 4 KB chunking).

### REQ-F-ROADMAP-03: lwIP mDNS Discovery & Zero-Config LAN Provisioning
- The network stack SHALL integrate `CONFIG_LWIP_MDNS_ENABLED=y` on the lwIP layer.
- **Service Advertising**:
  - Broadcasts service `_meshx-gateway._tcp.local` on port 1883 (MQTT) or 8080 (Web Console / REST) allowing auto-discovery by mobile apps, local dashboards, and LAN controllers without hardcoded IP configuration.
- **Footprint Constraint**:
  - mDNS daemon memory overhead shall remain constrained below 4 KB DRAM.

